/*  This file (mu2e_event.c) was created by Ron Rechenmacher <ron@fnal.gov> on
		Feb  5, 2014. "TERMS AND CONDITIONS" governing this file are in the README
		or COPYING file. If you do not have such a file, one can be obtained by
		contacting Ron or Fermi Lab in Batavia IL, 60510, phone: 630-840-3000.
		$RCSfile: .emacs.gnu,v $
		rev="$Revision: 1.23 $$Date: 2012/01/23 15:32:40 $";
		*/

#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/pci.h>     /* struct pci_dev *pci_get_device */
#include <linux/timer.h>   /* del_timer_sync  */
#include <linux/version.h> /* LINUX_VERSION_COD, KERNEL_VERSION */

#include "trace.h" /* TRACE */

#include "mu2e_event.h"
#include "mu2e_mem.h"           /* bar_info_t, extern mu2e_pci*  */
#include "mu2e_proto_globals.h" /* mu2e_channel_info */
#include "xdma_hw.h"            /* S2C, C2S, Dma_mReadChReg, Dma_mWriteReg */

#define PACKET_POLL_HZ 1000

/// <summary>
/// Data for retrieving DTC ID from timer list
/// </summary>
struct timer_data
{
	struct timer_list timer;  ///< Timer instance
	int dtc;                  ///< DTC Number
};
struct timer_data packets_timer[MU2E_MAX_NUM_DTCS];
int packets_timer_guard[MU2E_MAX_NUM_DTCS] = {1};

extern int checkDmaEngine(int dtc, unsigned chn, unsigned dir);

irqreturn_t DmaInterrupt(int irq, void *dev_id)
{
#if MU2E_RECV_INTER_ENABLED
	unsigned long base;
	unsigned chn;
	int dtc = MINOR(((struct pci_dev *)dev_id)->dev.devt);

	TRACE(TLVL_DEBUG+10, "DmaInterrupt: start irq=%d, dev=%p, dtc=%d", irq, dev_id, dtc);

	base = (unsigned long)(mu2e_pcie_bar_info[dtc].baseVAddr);
	Dma_mIntDisable(base);

	TRACE(TLVL_DEBUG+11, "DmaInterrupt: Checking DMA Engines");
	/* Check interrupt for error conditions */
	for (chn = 0; chn < 2; ++chn)
	{
		checkDmaEngine(dtc, chn, C2S);
	}

	TRACE(TLVL_DEBUG+12, "DmaInterrupt: Calling poll routine");
	/* Handle DMA and any user interrupts */
	if (mu2e_force_poll(dtc) == 0)
	{
		TRACE(TLVL_DEBUG+13, "DmaInterrupt: Marking Interrupt as acked");
		Dma_mIntAck(base, DMA_ENG_ALLINT_MASK | DMA_ENG_INT_ENABLE/*temporarily reenable INT until checkDmaEngine (above) and in ioctl BUF_GIVE is understood */);
		return IRQ_HANDLED;
	}
	else
#endif
	{
		TRACE(TLVL_DEBUG+14, "DmaInterrupt: ERROR Processing interrupt or interrupts not enabled!");
		return IRQ_NONE;
	}
}

/* Poll for completed "read dma (C2S)" buffers.
   Called from timer or interrupt (indirectly via mu2e_force_poll).
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
static void poll_packets(unsigned long dc)
#else
static void poll_packets(struct timer_list *t)
#endif
{
	unsigned long base;
	int error, did_work;
	int chn, dir;
	unsigned nxtCachedCmpltIdx;
#if MU2E_RECV_INTER_ENABLED == 0
	int offset;
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
	int dtc = (int)dc;
#else
	struct timer_data *tt = from_timer(tt, t, timer);
	int dtc = tt->dtc;  // FIXME: from_timer(, t, );
#endif
	mu2e_buffdesc_C2S_t *buffdesc_C2S_p;

	error = 0;
	did_work = 0;

	TRACE(TLVL_DEBUG+15, "poll_packets: begin dtc=%d", dtc);
	/* DMA registers are offset from BAR0 */
	base = (unsigned long)(mu2e_pcie_bar_info[dtc].baseVAddr);
	TRACE(TLVL_DEBUG+16, "poll_packets: After reading BAR0=0x%lx", base);

	// check channel 0 reciever
	TRACE(TLVL_DEBUG+17,
		"poll_packets: "
		"CNTL=0x%08x "
		"H_NEXT=%u "
		"S_NEXT=%u "
		"H_CPLT=%u "
		"CPBYTS=0x%08x ",
		Dma_mReadChnReg(dtc, 0, C2S, REG_DMA_ENG_CTRL_STATUS),
		descDmaAdr2idx(Dma_mReadChnReg(dtc, 0, C2S, REG_HW_NEXT_BD), dtc, 0, C2S, mu2e_channel_info_[dtc][0][C2S].hwIdx),
		descDmaAdr2idx(Dma_mReadChnReg(dtc, 0, C2S, REG_SW_NEXT_BD), dtc, 0, C2S, mu2e_channel_info_[dtc][0][C2S].swIdx),
		descDmaAdr2idx(Dma_mReadChnReg(dtc, 0, C2S, REG_HW_CMPLT_BD), dtc, 0, C2S, mu2e_channel_info_[dtc][0][C2S].hwIdx),
		Dma_mReadChnReg(dtc, 0, C2S, REG_DMA_ENG_COMP_BYTES));
	TRACE(TLVL_DEBUG+18, "poll_packets: App0: gen=0x%x pktlen=0x%04x chk/loop=0x%x", Dma_mReadReg(base, 0x9100),
		  Dma_mReadReg(base, 0x9104), Dma_mReadReg(base, 0x9108));

	dir = C2S;
	for (chn = 0; chn < MU2E_MAX_CHANNELS; ++chn)
	{  // Read the HW register and convert (Dma) addr in reg to idx.
		u32 newCmpltIdx = descDmaAdr2idx(Dma_mReadChnReg(dtc, chn, dir, REG_HW_CMPLT_BD), dtc, chn, dir,
										 mu2e_channel_info_[dtc][chn][dir].hwIdx);

		u32 do_once = 0;

		if (newCmpltIdx >= MU2E_NUM_RECV_BUFFS)
		{
			TRACE(TLVL_ERROR, "poll_packets: newCmpltIdx (0x%x) is above maximum sane value!!! (%x) Current idx=0x%x", newCmpltIdx,
				  MU2E_NUM_RECV_BUFFS, mu2e_channel_info_[dtc][chn][dir].hwIdx);
			TRACE_CNTL("modeM,0");
			error = 1;
			// continue;
			break;
		}
		TRACE(TLVL_DEBUG+19, "poll_packets: MU2E_NUM_RECV_BUFFS=%i newCmpltIdx=0x%x Current_hwIdx=0x%x", MU2E_NUM_RECV_BUFFS,
			  newCmpltIdx, mu2e_channel_info_[dtc][chn][dir].hwIdx);
		// check just-read-HW-val (converted to idx) against "cached" copy
		while (newCmpltIdx !=
			   mu2e_channel_info_[dtc][chn][dir].hwIdx /*ie.cachedCmplt*/)
		{  // NEED TO UPDATE Receive Byte Counts
			int *BC_p = (int *)mu2e_mmap_ptrs[dtc][chn][dir][MU2E_MAP_META];
			uint64_t *dma_data_p;
			nxtCachedCmpltIdx = idx_add(mu2e_channel_info_[dtc][chn][dir].hwIdx, 1, dtc, chn, dir);
			dma_data_p = (uint64_t *)(mu2e_pci_recver[dtc][chn].databuffs[nxtCachedCmpltIdx]);
			buffdesc_C2S_p = idx2descVirtAdr(nxtCachedCmpltIdx, dtc, chn, dir);
			BC_p[nxtCachedCmpltIdx] = buffdesc_C2S_p->ByteCount;
			if (buffdesc_C2S_p->ByteCount > sizeof(mu2e_databuff_t)) {
			  TRACE(TLVL_ERROR,"DMA Engine dtc=%d chn=%d dir=%d TRANSFERRED PAST END OF BUFFER - CONSIDER REBOOT",dtc,chn,dir);
			  // look in syslog (/var/log/messages or journalctl) and dmesg
			}
			TRACE(TLVL_DEBUG+20, "poll_packets: dtc|chn|dir=0x%03x %p[idx=%u]=byteCnt=%d newCmpltIdx=%u",
			      (dtc<<8)|(chn<<4)|dir, (void *)BC_p, nxtCachedCmpltIdx, buffdesc_C2S_p->ByteCount, newCmpltIdx);
			mu2e_channel_info_[dtc][chn][dir].hwIdx = nxtCachedCmpltIdx;
			// Now system SW can see another buffer with valid meta data
			TRACE(TLVL_DEBUG+21, "poll_packets: dtc|chn|dir=0x%03x "
			      "0x%016Lx 0x%016Lx 0x%016Lx 0x%016Lx 0x%016Lx 0x%016Lx 0x%016Lx 0x%016Lx 0x%016Lx",
			      (dtc<<8)|(chn<<4)|dir, 
			      dma_data_p[0], dma_data_p[1], dma_data_p[2], dma_data_p[3],
			      dma_data_p[4], dma_data_p[5], dma_data_p[6], dma_data_p[7], dma_data_p[8] );
			if (buffdesc_C2S_p->ByteCount > 72) {
			  int lwd = buffdesc_C2S_p->ByteCount / 8;
			  TRACE(TLVL_DEBUG+21, "poll_packets: dtc|chn|dir=0x%03x "
			      "0x%016Lx 0x%016Lx 0x%016Lx 0x%016Lx 0x%016Lx 0x%016Lx 0x%016Lx 0x%016Lx 0x%016Lx",
			      (dtc<<8)|(chn<<4)|dir, 
			      dma_data_p[lwd-8], dma_data_p[lwd-7], dma_data_p[lwd-6], dma_data_p[lwd-5],
			      dma_data_p[lwd-4], dma_data_p[lwd-3], dma_data_p[lwd-2], dma_data_p[lwd-1], dma_data_p[lwd] );
			}
			do_once = 1;
			did_work = 1;
		}
		if (do_once)
		{ /* and wake up the user process waiting for data */
			wake_up_interruptible(&get_info_wait_queue);
		}
	}

#if MU2E_RECV_INTER_ENABLED == 1
	if (did_work)
	{
		// Reschedule immediately
		TRACE(TLVL_DEBUG+22, "poll_packets: dtc=%d chn=%d dir=%d did_work=%d rescheduling poll", dtc, chn, dir, did_work);
#if 1
		packets_timer[dtc].timer.expires = jiffies;
		add_timer(&packets_timer[dtc].timer);
#else
		mu2e_force_poll(dtc);
#endif
	}
	else
	{
		// Re-enable interrupts.
		TRACE(TLVL_DEBUG+23, "poll_packets: dtc=%d chn=%d dir=%d did_work=%d re-enabling interrupts", dtc, chn, dir, did_work);
		packets_timer_guard[dtc] = 1;
		Dma_mIntEnable(base);
	}
#else

	// S2C checked in xmit ioctl or write

	// Reschedule poll routine.
	packets_timer_guard[dtc] = 1;
	offset = HZ / PACKET_POLL_HZ + (error ? 5 * HZ : 0);
	packets_timer[dtc].timer.expires = jiffies + offset;
	add_timer(&packets_timer[dtc].timer);
	TRACE(TLVL_DEBUG+24, "poll_packets: After reschedule, offset=%i", offset);
#endif
}

//////////////////////////////////////////////////////////////////////////////

int mu2e_event_up(int dtc)
{
	TRACE(TLVL_DEBUG+25, "mu2e_event_up dtc=%d", dtc);
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
	TRACE(TLVL_DEBUG+26, "mu2e_event_up calling init_timer");
	init_timer(&(packets_timer[dtc].timer));
	packets_timer[dtc].timer.function = poll_packets;
	packets_timer[dtc].timer.data = dtc;
	init_timer(&(packets_timer[dtc].timer));
#else
	TRACE(TLVL_DEBUG+27, "mu2e_event_up calling timer_setup");
	packets_timer[dtc].dtc = dtc;
	timer_setup(&packets_timer[dtc].timer, poll_packets, 0);
#endif
	packets_timer_guard[dtc] = 1;
	TRACE(TLVL_DEBUG+28, "mu2e_event_up complete, calling mu2e_sched_poll");
	return mu2e_sched_poll(dtc);
}

int mu2e_sched_poll(int dtc)
{
	TRACE(TLVL_DEBUG+29, "mu2e_sched_poll dtc=%d packets_timer_guard[dtc]=%d", dtc, packets_timer_guard[dtc]);
	if (packets_timer_guard[dtc])
	{
		packets_timer_guard[dtc] = 0;
		packets_timer[dtc].timer.expires = jiffies
#if MU2E_RECV_INTER_ENABLED == 0
										   + (HZ / PACKET_POLL_HZ)
#endif
			;
		// Timer->data=(unsigned long) pdev;
		TRACE(TLVL_DEBUG+30, "Adding poll_packets timer for dtc %d=%d", dtc, packets_timer[dtc].dtc);
		add_timer(&packets_timer[dtc].timer);
	}
	return (0);
}

int mu2e_force_poll(int dtc)
{
	TRACE(TLVL_DEBUG+31, "mu2e_force_poll dtc=%d packets_timer_guard[dtc]=%d", dtc, packets_timer_guard[dtc]);
	if (packets_timer_guard[dtc])
	{
		packets_timer_guard[dtc] = 0;
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
		poll_packets(dtc);
#else
		poll_packets(&packets_timer[dtc].timer);
#endif
	}
	return 0;
}

void mu2e_event_down(int dtc)
{
	// while (packets_timer_guard[dtc] == 0) {}
	packets_timer_guard[dtc] = 0;  // Ensure that mu2e_force_poll won't call poll_packets again
	del_timer_sync(&packets_timer[dtc].timer);
	packets_timer_guard[dtc] = 0;  // Ensure that mu2e_force_poll won't call poll_packets again
}
