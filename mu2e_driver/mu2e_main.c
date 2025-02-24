/*  This file (mu2e.c) was created by Ron Rechenmacher <ron@fnal.gov> on
		Feb  5, 2014. "TERMS AND CONDITIONS" governing this file are in the README
		or COPYING file. If you do not have such a file, one can be obtained by
		contacting Ron or Fermi Lab in Batavia IL, 60510, phone: 630-840-3000.
		$RCSfile: .emacs.gnu,v $
		rev="$Revision: 1.23 $$Date: 2012/01/23 15:32:40 $";
		*/
#include <linux/uaccess.h> /* access_ok, copy_to_user */
#include <linux/delay.h>   /* msleep */
#include <linux/fs.h>      /* struct inode */
#include <linux/init.h>    // module_init,_exit
#include <linux/jiffies.h> /* msec_to_jiffies */
#include <linux/kernel.h>  // KERN_INFO, printk
#include <linux/module.h>  // module_param, THIS_MODULE
#include <linux/pci.h>     /* struct pci_dev *pci_get_device */
#include <linux/version.h> /* LINUX_VERISON_CODE, KERNEL_VERSION */
#include <linux/wait.h>    /* wait_event_interruptible_timeout */

#include "xdma_hw.h" /* struct BuffDesc */

#include "trace.h" /* TRACE */

#include "mu2e_event.h"
#include "mu2e_fs.h" /* mu2e_ioctl prototype */
#include "mu2e_mem.h"
#include "mu2e_mmap_ioctl.h"
#include "mu2e_pci.h"           /* bar_info_t, extern mu2e_pci*  */
#include "mu2e_proto_globals.h" /* MU2E_MAX_CHANNEL, etc. */

#if LINUX_VERSION_CODE <= KERNEL_VERSION(4, 0, 0)
#define ACCESS_OK_READ(addr, size) access_ok(VERIFY_READ, addr, size)
#define ACCESS_OK_WRITE(addr, size) access_ok(VERIFY_WRITE, addr, size)
#else
#define ACCESS_OK_READ(addr, size) access_ok(addr, size)
#define ACCESS_OK_WRITE(addr, size) access_ok(addr, size)
#endif

#define DRIVER_VERSION_STRING "v3_00_00"

/* GLOBALS */

struct pci_dev *mu2e_pci_dev[MU2E_MAX_NUM_DTCS] = {0};

bar_info_t mu2e_pcie_bar_info[MU2E_MAX_NUM_DTCS] = {{0}};

dev_t mu2e_dev_number;
struct class *mu2e_dev_class;

pci_sender_t mu2e_pci_sender[MU2E_MAX_NUM_DTCS][MU2E_NUM_SEND_CHANNELS] = {{{0}}};

pci_recver_t mu2e_pci_recver[MU2E_MAX_NUM_DTCS][MU2E_NUM_RECV_CHANNELS] = {{{0}}};

// This variable name is used in a macro that expects the same
// variable name in the user-space "library"
m_ioc_get_info_t mu2e_channel_info_[MU2E_MAX_NUM_DTCS][MU2E_MAX_CHANNELS][2];  // See enums in mu2e_mmap_ioctl.h (0=C2S)

//    ch,dir,buffers/meta
volatile void *mu2e_mmap_ptrs[MU2E_MAX_NUM_DTCS][MU2E_MAX_CHANNELS][2][2];

/* for exclusion of all program flows (processes, ISRs and BHs) */
static DEFINE_MUTEX(DmaStatsLock);
static DEFINE_MUTEX(ReadbackLock);
static DEFINE_MUTEX(DcsTransactionLock);
static DEFINE_SPINLOCK(GetInfoLock);

/**
 * The get_info_wait_queue allows this module to put
 * userspace processes that are reading data to sleep
 * if there is no data available.
 */
DECLARE_WAIT_QUEUE_HEAD(get_info_wait_queue);

#define MAX_STATS 100
/* Statistics-related variables */
DMAStatistics DStats[MAX_DMA_ENGINES][MAX_STATS];
SWStatistics SStats[MAX_DMA_ENGINES][MAX_STATS];
TRNStatistics TStats[MAX_STATS];
int dstatsRead[MAX_DMA_ENGINES], dstatsWrite[MAX_DMA_ENGINES];
int dstatsNum[MAX_DMA_ENGINES], sstatsRead[MAX_DMA_ENGINES];
int sstatsWrite[MAX_DMA_ENGINES], sstatsNum[MAX_DMA_ENGINES];
int tstatsRead, tstatsWrite, tstatsNum;
u32 SWrate[MAX_DMA_ENGINES];

/* DCS Transaction lock (software-implemented) */
bool mu2e_dcs_locks[MU2E_MAX_NUM_DTCS] = {0};

//////////////////////////////////////////////////////////////////////////////
/* forward decl */
static int ReadPCIState(struct pci_dev *pdev, m_ioc_pcistate_t *pcistate);

/**
   Currently called from mu2e_event.c:DmaInterrupt and ioctl BUF_GIVE
 */
int checkDmaEngine(int dtc, unsigned chn, unsigned dir)
{
	int sts = 0;
	u32 status = Dma_mReadChnReg(dtc, chn, dir, REG_DMA_ENG_CTRL_STATUS);
	int lc = 5;

	if (dir == C2S &&
		(status & (DMA_ENG_INT_ALERR | DMA_ENG_INT_FETERR | DMA_ENG_INT_ABORTERR | DMA_ENG_INT_CHAINEND)) != 0)
	{
		TRACE(20, "checkDmaEngine: One of the error bits set: dtc=%d chn=%d dir=%d sts=0x%llx", dtc, chn, dir,
			  (unsigned long long)status);
		printk("DTC DMA Interrupt Error Bits Set: dtc=%d chn=%d dir=%d, sts=0x%llx", dtc, chn, dir, (unsigned long long)status);
		/* Perform soft reset of DMA engine */
		Dma_mWriteChnReg(dtc, chn, dir, REG_DMA_ENG_CTRL_STATUS, DMA_ENG_USER_RESET);
		status = Dma_mReadChnReg(dtc, chn, dir, REG_DMA_ENG_CTRL_STATUS);
		while ((status & DMA_ENG_USER_RESET) != 0 && lc > 0)
		{
			status = Dma_mReadChnReg(dtc, chn, dir, REG_DMA_ENG_CTRL_STATUS);
			--lc;
		}
		lc = 5;
		Dma_mWriteChnReg(dtc, chn, dir, REG_DMA_ENG_CTRL_STATUS, DMA_ENG_RESET);
		while ((status & DMA_ENG_RESET) != 0 && lc > 0)
		{
			status = Dma_mReadChnReg(dtc, chn, dir, REG_DMA_ENG_CTRL_STATUS);
			--lc;
		}
		sts = 1;
	}

	if ((status & DMA_ENG_ENABLE) == 0)
	{
		TRACE(20, "checkDmaEngine: DMA ENGINE DISABLED! Re-enabling... dtc=%d chn=%d dir=%d", dtc, chn, dir);
		if (dir == C2S)
		{
			Dma_mWriteChnReg(dtc, chn, dir, REG_DMA_ENG_CTRL_STATUS, DMA_ENG_ENABLE | DMA_ENG_INT_ENABLE);
		}
		else
		{
			Dma_mWriteChnReg(dtc, chn, dir, REG_DMA_ENG_CTRL_STATUS, DMA_ENG_ENABLE);
		}
		sts = 1;
	}

	if ((status & DMA_ENG_STATE_MASK) != 0)
	{
		TRACE(20, "checkDmaEngine: DMA Engine Status: dtc=%d, chn=%d dir=%d r=%d, w=%d", dtc, chn, dir,
			  ((status & DMA_ENG_RUNNING) != 0 ? 1 : 0), ((status & DMA_ENG_WAITING) != 0 ? 1 : 0));
	}
	return sts;
} /* checkDmaEngine */

IOCTL_RET_TYPE mu2e_ioctl(IOCTL_ARGS(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg))
{
	IOCTL_RET_TYPE retval = 0;
	unsigned long base;
	unsigned jj;
	m_ioc_reg_access_t reg_access;
	m_ioc_get_info_t get_info;
	int chn, dir, num;
	unsigned myIdx, nxtIdx, hwIdx;
	volatile mu2e_buffdesc_S2C_t *desc_S2C_p;
	u32 descDmaAdr_swNxt;
	m_ioc_pcistate_t pcistate;
	m_ioc_engstate_t eng;
	m_ioc_engstats_t es;
	mu2e_string_t ms;
	TRNStatsArray tsa;
	int which_engine, len, ii;
	DMAStatistics *ds;
	TRNStatistics *ts;
	unsigned tmo_jiffies;
	int dtc = iminor(filp->f_path.dentry->d_inode);

	TRACE(11, "mu2e_ioctl: start - dtc=%d cmd=0x%x", dtc, cmd);
	if (_IOC_TYPE(cmd) != MU2E_IOC_MAGIC) return -ENOTTY;

	/* Check read/write and corresponding argument */
	if (_IOC_DIR(cmd) & _IOC_READ)
		if (!ACCESS_OK_WRITE((void *)arg, _IOC_SIZE(cmd))) return -EFAULT;
	if (_IOC_DIR(cmd) & _IOC_WRITE)
		if (!ACCESS_OK_READ((void *)arg, _IOC_SIZE(cmd))) return -EFAULT;

	/* DMA registers are offset from BAR0 */
	base = (unsigned long)(mu2e_pcie_bar_info[dtc].baseVAddr);

	TRACE(11, "mu2e_ioctl: start2");
	switch (cmd)
	{
		case M_IOC_GET_TST_STATE:
			TRACE(12, "mu2e_ioctl: cmd=GET_TST_STATE");
			break;
		case M_IOC_TEST_START:
			TRACE(13, "mu2e_ioctl: cmd=TEST_START");
			// enable dma ch0/C2S w/GENERATOR
			Dma_mWriteChnReg(dtc, 0, C2S, REG_DMA_ENG_CTRL_STATUS, DMA_ENG_ENABLE);
			msleep(20);
			Dma_mWriteReg(mu2e_pcie_bar_info[dtc].baseVAddr, 0x9108, /*0x3f*/ 0xffffffff);  // LOOPBACK
			msleep(10);
			// Dma_mWriteReg( base, 0x9100, 1 );  // 1=enable generator
			break;
		case M_IOC_TEST_STOP:
			TRACE(14, "mu2e_ioctl: cmd=TEST_STOP");
			break;
			/* ------------------------------------------------------------------- */

		case M_IOC_GET_PCI_STATE: /* m_ioc_pcistate_t; formerly IGET_PCI_STATE      _IOR(XPMON_MAGIC,4,PCIState) */
			TRACE(15, "mu2e_ioctl: cmd=GET_PCI_STATE");
			ReadPCIState(mu2e_pci_dev[dtc], &pcistate);
			if (copy_to_user((m_ioc_pcistate_t *)arg, &pcistate, sizeof(m_ioc_pcistate_t)))
			{
				printk("copy_to_user failed\n");
				retval = -EFAULT;
				break;
			}
			break;
		case M_IOC_GET_ENG_STATE: /* m_ioc_engstate_t; formerly IGET_ENG_STATE      _IOR(XPMON_MAGIC,5,EngState) */
			TRACE(16, "mu2e_ioctl: cmd=GET_ENG_STATE");
			if (copy_from_user(&eng, (m_ioc_engstate_t *)arg, sizeof(m_ioc_engstate_t)))
			{
				printk("\ncopy_from_user failed\n");
				retval = -EFAULT;
				break;
			}

			which_engine = eng.Engine;  // printk("For engine %d\n", i);

			/* First, check if requested engine is valid */
			if ((which_engine >= MAX_DMA_ENGINES) /*|| (!((dmaData->engineMask) & (1LL << i)))*/)
			{
				printk("Invalid engine %d\n", which_engine);
				retval = -EFAULT;
				break;
			}

			/* First, get the user state */
			eng.Buffers = 4;                        // ustate.Buffers;
			eng.MinPktSize = 64;                    // ustate.MinPktSize;
			eng.MaxPktSize = SET_DTC_MAX_DMA_SIZE;  // ustate.MaxPktSize;
			eng.TestMode = 1;                       // ustate.TestMode;

			/* Now add the DMA state */
			eng.BDs = 399;    /* FNAL devel -- linked to sguser.c:#define NUM_BUFS  and DmaSetupTransmit(handle[0],100) ??? */
			eng.BDerrs = 0;   // rptr->BDerrs;
			eng.BDSerrs = 0;  // rptr->BDSerrs;
#ifdef TH_BH_ISR
			eng.IntEnab = 1;
#else
			eng.IntEnab = 0;
#endif
			if (copy_to_user((m_ioc_engstate_t *)arg, &eng, sizeof(m_ioc_engstate_t)))
			{
				printk("copy_to_user failed\n");
				retval = -EFAULT;
				break;
			}
			break;
		case M_IOC_GET_DMA_STATS: /* m_ioc_engstats_t; formerly IGET_DMA_STATISTICS _IOR(XPMON_MAGIC,6,EngStatsArray) */
			TRACE(17, "mu2e_ioctl: cmd=GET_DMA_STATS");
			if (copy_from_user(&es, (m_ioc_engstats_t *)arg, sizeof(m_ioc_engstats_t)))
			{
				printk("copy_from_user failed\n");
				retval = -1;
				break;
			}

			ds = es.engptr;
			len = 0;
			for (ii = 0; ii < es.Count; ++ii)
			{
				DMAStatistics from;
				int j;

				/* Must copy in a round-robin manner so that reporting is fair */
				for (j = 0; j < MAX_DMA_ENGINES; j++)
				{
					if (!dstatsNum[j]) continue;

					mutex_lock(&DmaStatsLock);
					from = DStats[j][dstatsRead[j]];
					from.Engine = j;
					dstatsNum[j] -= 1;
					dstatsRead[j] += 1;
					if (dstatsRead[j] == MAX_STATS) dstatsRead[j] = 0;
					mutex_unlock(&DmaStatsLock);

					if (copy_to_user(ds, &from, sizeof(DMAStatistics)))
					{
						printk("copy_to_user failed\n");
						retval = -EFAULT;
						break;
					}

					len++;
					ii++;
					if (ii >= es.Count) break;
					ds++;
				}
				if (retval < 0) break;
			}
			es.Count = len;
			if (copy_to_user((m_ioc_engstats_t *)arg, &es, sizeof(m_ioc_engstats_t)))
			{
				printk("copy_to_user failed\n");
				retval = -EFAULT;
				break;
			}
			break;
		case M_IOC_GET_TRN_STATS: /* TRNStatsArray;    formerly IGET_TRN_STATISTICS _IOR(XPMON_MAGIC,7,TRNStatsArray) */
			TRACE(18, "mu2e_ioctl: cmd=GET_TRN_STATS");
			if (copy_from_user(&tsa, (TRNStatsArray *)arg, sizeof(TRNStatsArray)))
			{
				printk("copy_from_user failed\n");
				retval = -1;
				break;
			}

			ts = tsa.trnptr;
			len = 0;
			for (ii = 0; ii < tsa.Count; ++ii)
			{
				TRNStatistics from;

				if (!tstatsNum) break;

				mutex_lock(&DmaStatsLock);
				from = TStats[tstatsRead];
				tstatsNum -= 1;
				tstatsRead += 1;
				if (tstatsRead == MAX_STATS) tstatsRead = 0;
				mutex_unlock(&DmaStatsLock);

				if (copy_to_user(ts, &from, sizeof(TRNStatistics)))
				{
					printk("copy_to_user failed\n");
					retval = -EFAULT;
					break;
				}

				len++;
				ts++;
			}
			tsa.Count = len;
			if (copy_to_user((TRNStatsArray *)arg, &tsa, sizeof(TRNStatsArray)))
			{
				printk("copy_to_user failed\n");
				retval = -EFAULT;
				break;
			}
			break;

			/* ------------------------------------------------------------------- */
		case M_IOC_REG_ACCESS:

			if (copy_from_user(&reg_access, (void *)arg, sizeof(reg_access)))
			{
				printk("copy_from_user failed\n");
				return (-EFAULT);
			}
			if (reg_access.access_type)
			{
				if (reg_access.access_type == 2) mutex_lock(&ReadbackLock);  // Lock for readback
				TRACE(19, "mu2e_ioctl: cmd=REG_ACCESS - write dtc=%d offset=0x%x, val=0x%x", dtc, reg_access.reg_offset, reg_access.val);
				Dma_mWriteReg(base, reg_access.reg_offset, reg_access.val);
				if (reg_access.access_type == 1) return retval;
			}

			TRACE(18, "mu2e_ioctl: cmd=REG_ACCESS - read offset=0x%x", reg_access.reg_offset);
			reg_access.val = Dma_mReadReg(base, reg_access.reg_offset);
			if (reg_access.access_type == 2) mutex_unlock(&ReadbackLock);  // Unlock for readback
			TRACE(19, "mu2e_ioctl: cmd=REG_ACCESS - read dtc=%d offset=0x%x, val=0x%x", dtc, reg_access.reg_offset, reg_access.val);
			if (copy_to_user((void *)arg, &reg_access, sizeof(reg_access)))
			{
				printk("copy_to_user failed\n");
				return (-EFAULT);
			}

			break;
		case M_IOC_GET_INFO:
			if (copy_from_user(&get_info, (void *)arg, sizeof(m_ioc_get_info_t)))
			{
				TRACE(0, "copy_from_user failed\n");
				return (-EFAULT);
			}
			tmo_jiffies = msecs_to_jiffies(get_info.tmo_ms);
			dir = get_info.dir;
			chn = get_info.chn;
			if (get_info.dir == C2S)
			{
				if (!mu2e_chn_info_delta_(dtc, get_info.chn, C2S, &mu2e_channel_info_))
				{
					TRACE(20, "mu2e_ioctl: cmd=GET_INFO wait_event_interruptible_timeout get_info.tmo_ms=%u jiffies=%u", get_info.tmo_ms, tmo_jiffies);
					spin_lock_irq(&GetInfoLock);
					if (tmo_jiffies == 0)
					{
						TRACE(20, "mu2e_ioctl: cmd=GET_INFO: Skipping wait due to 0 timeout");
					}
					else if (wait_event_interruptible_lock_irq_timeout(get_info_wait_queue,
																	   mu2e_chn_info_delta_(dtc, get_info.chn, C2S, &mu2e_channel_info_),
																	   GetInfoLock,
																	   tmo_jiffies) == 0)
					{
						TRACE(20, "mu2e_ioctl: cmd=GET_INFO tmo");
					}
					spin_unlock_irq(&GetInfoLock);
				}
			}
			else
			{
				jj = mu2e_channel_info_[dtc][chn][dir].num_buffs;
				hwIdx = mu2e_channel_info_[dtc][chn][dir].hwIdx;
				while (((mu2e_buffdesc_S2C_t *)idx2descVirtAdr(hwIdx, dtc, chn, dir))->Complete &&
					   hwIdx != mu2e_channel_info_[dtc][chn][dir].swIdx && jj--)
				{
					hwIdx = idx_add(hwIdx, 1, dtc, chn, dir);
					TRACE(20, "ioctl GET_INFO mu2e_channel_info_[dtc][chn][dir].hwIdx=%u swIdx=%u lps=%u", hwIdx,
						  mu2e_channel_info_[dtc][chn][dir].swIdx, jj);
				}
				mu2e_channel_info_[dtc][chn][dir].hwIdx = hwIdx;
			}
			get_info = mu2e_channel_info_[dtc][get_info.chn][get_info.dir];
			TRACE(20, "mu2e_ioctl: cmd=GET_INFO dir=%d get_info.dir=%u hwIdx=%u swIdx=%u", dir, get_info.dir, get_info.hwIdx,
				  get_info.swIdx);
			if (copy_to_user((void *)arg, &get_info, sizeof(m_ioc_get_info_t)))
			{
				TRACE(0, "copy_to_user failed\n");
				return (-EFAULT);
			}
			break;
		case M_IOC_BUF_GIVE:
			TRACE(25, "mu2e_ioctl: cmd=BUF_GIVE");
			chn = arg >> 24;
			dir = (arg >> 16) & 1;
			num = arg & 0xffff;
			TRACE(26, "mu2e_ioctl: BUF_GIVE chn:%u dir:%u num:%u", chn, dir, num);
			myIdx = idx_add(mu2e_channel_info_[dtc][chn][dir].swIdx, num, dtc, chn, dir);
			Dma_mWriteChnReg(dtc, chn, dir, REG_SW_NEXT_BD, idx2descDmaAdr(myIdx, dtc, chn, dir));
			checkDmaEngine(dtc, chn, dir);
			mu2e_channel_info_[dtc][chn][dir].swIdx = myIdx;
			break;
		case M_IOC_DUMP:
			TRACE(10, "SERDES LOOPBACK Enable 0x%x", Dma_mReadReg(mu2e_pcie_bar_info[dtc].baseVAddr, 0x9108));
			TRACE(10, "Link Enable 0x%x", Dma_mReadReg(mu2e_pcie_bar_info[dtc].baseVAddr, 0x9114));
			TRACE(10, "SERDES Rx Disparity error (2 bits/link) 0x%x",
				  Dma_mReadReg(mu2e_pcie_bar_info[dtc].baseVAddr, 0x911c));
			TRACE(10, "SERDES Rx character not in table (2 bits/link) 0x%x",
				  Dma_mReadReg(mu2e_pcie_bar_info[dtc].baseVAddr, 0x9120));
			TRACE(10, "SERDES unlock error 0x%x", Dma_mReadReg(mu2e_pcie_bar_info[dtc].baseVAddr, 0x9124));
			TRACE(10, "SERDES PLL lock 0x%x", Dma_mReadReg(mu2e_pcie_bar_info[dtc].baseVAddr, 0x9128));
			TRACE(10, "SERDES Tx buffer status (2 bits/link) 0x%x", Dma_mReadReg(mu2e_pcie_bar_info[dtc].baseVAddr, 0x912c));
			TRACE(10, "SERDES Rx buffer status (3 bits/link) 0x%x", Dma_mReadReg(mu2e_pcie_bar_info[dtc].baseVAddr, 0x9130));
			TRACE(10, "SERDES Reset done 0x%x", Dma_mReadReg(mu2e_pcie_bar_info[dtc].baseVAddr, 0x9138));

			for (chn = 0; chn < 2; ++chn)
				for (dir = 0; dir < 2; ++dir)
				{
					u32 hw_next = Dma_mReadChnReg(dtc, chn, dir, REG_HW_NEXT_BD);
					u32 sw_next = Dma_mReadChnReg(dtc, chn, dir, REG_SW_NEXT_BD);
					u32 hw_cmplt = Dma_mReadChnReg(dtc, chn, dir, REG_HW_CMPLT_BD);
					TRACE(10, "chn=%d dir=%x (0x%08x) hw_next_idx=%u sw_next_idx=%u hw_cmplt_idx=%u", chn,
						  dir == 0 ? 0xC25 : 0x52C, hw_next, descDmaAdr2idx(hw_next, dtc, chn, dir, 0),
						  descDmaAdr2idx(sw_next, dtc, chn, dir, 0), descDmaAdr2idx(hw_cmplt, dtc, chn, dir, 0));
					if (dir == 0)  // C25
					{
						u32 sw_has_recv_data;
						u32 hw = descDmaAdr2idx(hw_next, dtc, chn, dir, 0);
						u32 sw = descDmaAdr2idx(sw_next, dtc, chn, dir, 0);
						u32 hw_has_recv_data = ((hw >= sw) ? hw - sw : MU2E_NUM_RECV_BUFFS + hw - sw);
						hw = mu2e_channel_info_[dtc][chn][dir].hwIdx;
						sw = mu2e_channel_info_[dtc][chn][dir].swIdx;
						sw_has_recv_data = ((hw >= sw) ? hw - sw : MU2E_NUM_RECV_BUFFS + hw - sw);
						TRACE(10, "hw_has_recv_data=%u sw=%u", hw_has_recv_data, sw_has_recv_data);
					}
				}
			TRACE(10, "RECV[0] BUFFS:");
			for (jj = 0; jj < MU2E_NUM_RECV_BUFFS; ++jj)
			{
				TRACE(10, "%3u Addr=0x%p", jj, (void *)&mu2e_pci_recver[dtc][0].buffdesc_ring[jj]);
				TRACE(10, "%3u %2x 0x%08x (cmplt=%u, short=%u, err=%u)", jj, 0,
					  ((u32 *)&(mu2e_pci_recver[dtc][0].buffdesc_ring[jj]))[0],
					  mu2e_pci_recver[dtc][0].buffdesc_ring[jj]->Complete, mu2e_pci_recver[dtc][0].buffdesc_ring[jj]->Short,
					  mu2e_pci_recver[dtc][0].buffdesc_ring[jj]->Error);
				TRACE(10, "%3u %2x 0x%016llx (UserStatus)", jj, 4, mu2e_pci_recver[dtc][0].buffdesc_ring[jj]->UserStatus);
				TRACE(10, "%3u %2x 0x%08x (CardAddress)", jj, 12, mu2e_pci_recver[dtc][0].buffdesc_ring[jj]->CardAddress);
				TRACE(10, "%3u %2x 0x%08x IrqComplete=%u, IrqError=%u", jj, 16,
					  ((u32 *)&(mu2e_pci_recver[dtc][0].buffdesc_ring[jj]))[4],
					  mu2e_pci_recver[dtc][0].buffdesc_ring[jj]->IrqComplete,
					  mu2e_pci_recver[dtc][0].buffdesc_ring[jj]->IrqError);
				TRACE(10, "%3u %2x 0x%016llx (SystemAddress)", jj, 20,
					  mu2e_pci_recver[dtc][0].buffdesc_ring[jj]->SystemAddress);
				TRACE(10, "%3u %2x 0x%08x (NextDescPtr)", jj, 28, mu2e_pci_recver[dtc][0].buffdesc_ring[jj]->NextDescPtr);
				TRACE(10, "%3u meta@%p[%d]=%u", jj, mu2e_mmap_ptrs[dtc][0][C2S][MU2E_MAP_META], jj,
					  ((u32 *)(mu2e_mmap_ptrs[dtc][0][C2S][MU2E_MAP_META]))[jj]);
				TRACE(10, "%3u Raw Data: 0x%08x 0x%08x 0x%08x 0x%08x", jj, ((u32 *)&(mu2e_pci_recver[dtc][0].databuffs[jj]))[0],
					  ((u32 *)&(mu2e_pci_recver[dtc][0].databuffs[jj]))[1],
					  ((u32 *)&(mu2e_pci_recver[dtc][0].databuffs[jj]))[2],
					  ((u32 *)&(mu2e_pci_recver[dtc][0].databuffs[jj]))[3]);
			}
			TRACE(10, "SEND[0] BUFFS:");
			for (jj = 0; jj < MU2E_NUM_SEND_BUFFS; ++jj)
			{
				TRACE(10, "%3u Addr=0x%p", jj, (void *)&mu2e_pci_sender[dtc][0].buffdesc_ring[jj]);
				TRACE(10, "%3u %2x 0x%08x (cmplt=%u, short=%u, error=%u)", jj, 0,
					  ((u32 *)&(mu2e_pci_sender[dtc][0].buffdesc_ring[jj]))[0],
					  mu2e_pci_sender[dtc][0].buffdesc_ring[jj].Complete, mu2e_pci_sender[dtc][0].buffdesc_ring[jj].Short,
					  mu2e_pci_sender[dtc][0].buffdesc_ring[jj].Error);
				TRACE(10, "%3u %2x 0x%016llx (UserControl)", jj, 4, mu2e_pci_sender[dtc][0].buffdesc_ring[jj].UserControl);
				TRACE(10, "%3u %2x 0x%08x (CardAddress)", jj, 12, ((u32 *)&(mu2e_pci_sender[dtc][0].buffdesc_ring[jj]))[3]);
				TRACE(10, "%3u %2x 0x%08x IrqComplete=%u, IrqError=%u", jj, 16,
					  ((u32 *)&(mu2e_pci_sender[dtc][0].buffdesc_ring[jj]))[4],
					  mu2e_pci_sender[dtc][0].buffdesc_ring[jj].IrqComplete,
					  mu2e_pci_sender[dtc][0].buffdesc_ring[jj].IrqError);
				TRACE(10, "%3u %2x 0x%016llx (SystemAddress)", jj, 20, mu2e_pci_sender[dtc][0].buffdesc_ring[jj].SystemAddress);
				TRACE(10, "%3u %2x 0x%08x (NextDescPtr)", jj, 28, mu2e_pci_sender[dtc][0].buffdesc_ring[jj].NextDescPtr);
				TRACE(10, "%3u meta@%p[%d]=%u", jj, mu2e_mmap_ptrs[dtc][0][S2C][MU2E_MAP_META], jj,
					  ((u32 *)(mu2e_mmap_ptrs[dtc][0][S2C][MU2E_MAP_META]))[jj]);
				TRACE(10, "%3u RawData: 0x%08x 0x%08x 0x%08x 0x%08x", jj, ((u32 *)&(mu2e_pci_sender[dtc][0].databuffs[jj]))[0],
					  ((u32 *)&(mu2e_pci_sender[dtc][0].databuffs[jj]))[1],
					  ((u32 *)&(mu2e_pci_sender[dtc][0].databuffs[jj]))[2],
					  ((u32 *)&(mu2e_pci_sender[dtc][0].databuffs[jj]))[3]);
			}
			break;
		case M_IOC_BUF_XMIT:

			chn = arg >> 24;
			dir = S2C;

			// look at next descriptor and verify that it is complete
			// FIX ME --- race condition
			myIdx = mu2e_channel_info_[dtc][chn][dir].swIdx;
			desc_S2C_p = idx2descVirtAdr(myIdx, dtc, chn, dir);
			if (desc_S2C_p->Complete != 1)
			{
				TRACE(22, "ioctl BUF_XMIT -EAGAIN myIdx=%u err=%d desc_S2C_p=%p 0x%016llx counts(in,sts)=%u,%u", myIdx,
					  desc_S2C_p->Error, desc_S2C_p, *(u64 *)desc_S2C_p, desc_S2C_p->ByteCount, desc_S2C_p->ByteCnt);
				return -EAGAIN;
			}
			TRACE(22, "mu2e_ioctl: cmd=BUF_XMIT desc_S2C_p=%p 0x%016llx", desc_S2C_p, *(u64 *)desc_S2C_p);

			desc_S2C_p->Complete = 0;               // FIX ME --- race condition
			desc_S2C_p->ByteCount = arg & 0xfffff;  // 20 bits max
			desc_S2C_p->ByteCnt = arg & 0xfffff;    // 20 bits max
#if MU2E_RECV_INTER_ENABLED
			desc_S2C_p->IrqComplete = 1;
			desc_S2C_p->IrqError = 1;
#else
			desc_S2C_p->IrqComplete = 0;
			desc_S2C_p->IrqError = 0;
#endif
			desc_S2C_p->StartOfPkt = 1;
			desc_S2C_p->EndOfPkt = 1;
			{
				void *data = ((mu2e_databuff_t *)(mu2e_mmap_ptrs[dtc][chn][dir][MU2E_MAP_BUFF]))[myIdx];
				TRACE(22,
					  "ioctl BUF_XMIT myIdx=%u desc_S2C_p(%p)=%016llx ByteCnt=%d data(%p)[2-5]=%016llx %016llx %016llx %016llx",
					  myIdx, desc_S2C_p, *(u64 *)desc_S2C_p, desc_S2C_p->ByteCnt, data, ((u64 *)data)[2], ((u64 *)data)[3],
					  ((u64 *)data)[4], ((u64 *)data)[5]);
			}

			/* See Transmit (S2C) Descriptor Management
		 on page 56 of kc705_TRD_k7_pcie_dma_ddr3_base_Doc_13.4.pdf */
			nxtIdx = idx_add(myIdx, 1, dtc, chn, dir);
			descDmaAdr_swNxt = idx2descDmaAdr(nxtIdx, dtc, chn, dir);

			// update hwIdx here - MUST CHECK Buffer Descriptor Complete bit!!! (not register!!!)
			jj = mu2e_channel_info_[dtc][chn][dir].num_buffs;
			hwIdx = mu2e_channel_info_[dtc][chn][dir].hwIdx;
			while (((mu2e_buffdesc_S2C_t *)idx2descVirtAdr(hwIdx, dtc, chn, dir))->Complete && hwIdx != myIdx && jj--)
			{
				hwIdx = idx_add(hwIdx, 1, dtc, chn, dir);
				TRACE(22, "ioctl BUF_XMIT mu2e_channel_info_[dtc][chn][dir].hwIdx=%u swIdx=%u lps=%u", hwIdx,
					  mu2e_channel_info_[dtc][chn][dir].swIdx, jj);
			}
			mu2e_channel_info_[dtc][chn][dir].hwIdx = hwIdx;

			// Dma_mReadReg(mu2e_pcie_bar_info[dtc].baseVAddr, 0x9108); // DEBUG read "user" reg.
			TRACE(22,
				  "mu2e_ioctl BUF_XMIT b4 WriteChnReg REG_SW_NEXT_BD(idx=%u) TELLING DMA TO GO (DO THIS BD) hwIdx=%u "
				  "->Complete=%d [0].Complete=%d",
				  nxtIdx, hwIdx, ((mu2e_buffdesc_S2C_t *)idx2descVirtAdr(hwIdx, dtc, chn, dir))->Complete,
				  ((mu2e_buffdesc_S2C_t *)idx2descVirtAdr(0, dtc, chn, dir))->Complete);
			Dma_mWriteChnReg(dtc, chn, dir, REG_SW_NEXT_BD, descDmaAdr_swNxt);

			mu2e_channel_info_[dtc][chn][dir].swIdx = nxtIdx;
			TRACE(22, "mu2e_ioctl BUF_XMIT after WriteChnReg REG_SW_NEXT_BD swIdx=%u hwIdx=%u ->Complete=%d CmpltIdx=%u",
				  nxtIdx, hwIdx, ((mu2e_buffdesc_S2C_t *)idx2descVirtAdr(hwIdx, dtc, chn, dir))->Complete,
				  descDmaAdr2idx(Dma_mReadChnReg(dtc, chn, dir, REG_HW_NEXT_BD), dtc, chn, dir, 0));
			break;
		case M_IOC_DCS_LOCK:
			TRACE(23, "mu2e_ioctl DCS_LOCK before taking DcsTransactionLock");
			mutex_lock(&DcsTransactionLock);
			TRACE(23, "mu2e_ioctl DCS_LOCK after taking DcsTransactionLock, locks[dtc]=%d", mu2e_dcs_locks[dtc]);
			if (mu2e_dcs_locks[dtc])
			{
				retval = -EAGAIN;
			}
			else
			{
				mu2e_dcs_locks[dtc] = 1;
				retval = 0;
			}
			TRACE(23, "mu2e_ioctl DCS_LOCK before releasing DcsTransactionLock, locks[dtc]=%d", mu2e_dcs_locks[dtc]);
			mutex_unlock(&DcsTransactionLock);
			TRACE(23, "mu2e_ioctl DCS_LOCK after releasing DcsTransactionLock retval=%ld", retval);
			break;
		case M_IOC_DCS_RELEASE:
			TRACE(24, "mu2e_ioctl DCS_UNLOCK before taking DcsTransactionLock");
			mutex_lock(&DcsTransactionLock);
			TRACE(24, "mu2e_ioctl DCS_UNLOCK after taking DcsTransactionLock, locks[dtc]=%d", mu2e_dcs_locks[dtc]);
			mu2e_dcs_locks[dtc] = 0;
			retval = 0;
			TRACE(24, "mu2e_ioctl DCS_UNLOCK before releasing DcsTransactionLock, locks[dtc]=%d", mu2e_dcs_locks[dtc]);
			mutex_unlock(&DcsTransactionLock);
			TRACE(24, "mu2e_ioctl DCS_UNLOCK after releasing DcsTransactionLock");
			break;
		case M_IOC_GET_VERSION:

			TRACE(10, "mu2e_ioctl: cmd=GET_VRESION v=%s", DRIVER_VERSION_STRING);
			strncpy(ms, DRIVER_VERSION_STRING, sizeof(mu2e_string_t) - 1);
			if (copy_to_user((mu2e_string_t *)arg, &ms, sizeof(mu2e_string_t)))
			{
				TRACE(0, "copy_to_user failed\n");
				return (-EFAULT);
			}
			retval = 0;
			break;
		default:
			TRACE(11, "mu2e_ioctl: unknown cmd");
			return (-ENOSYS);  // some error
	}
	TRACE(11, "mu2e_ioctl: end");
	return (retval);
}  // mu2e_ioctl

/////////////////////////////////////////////////////////////////////////////////////////////

static int ReadPCIState(struct pci_dev *pdev, m_ioc_pcistate_t *pcistate)
{
	int pos;
	u16 valw;
	u8 valb;
	unsigned long base;
	int dtc = MINOR(pdev->dev.devt);

	/* Since probe has succeeded, indicates that link is up. */
	pcistate->LinkState = LINK_UP;
	pcistate->VendorId = XILINX_VENDOR_ID;
	pcistate->DeviceId = XILINX_DEVICE_ID;

	/* Read Interrupt setting - Legacy or MSI/MSI-X */
	pci_read_config_byte(pdev, PCI_INTERRUPT_PIN, &valb);
	if (!valb)
	{
		if (pci_find_capability(pdev, PCI_CAP_ID_MSIX))
			pcistate->IntMode = INT_MSIX;
		else if (pci_find_capability(pdev, PCI_CAP_ID_MSI))
			pcistate->IntMode = INT_MSI;
		else
			pcistate->IntMode = INT_NONE;
	}
	else if ((valb >= 1) && (valb <= 4))
		pcistate->IntMode = INT_LEGACY;
	else
		pcistate->IntMode = INT_NONE;

	if ((pos = pci_find_capability(pdev, PCI_CAP_ID_EXP)))
	{
		/* Read Link Status */
		pci_read_config_word(pdev, pos + PCI_EXP_LNKSTA, &valw);
		pcistate->LinkSpeed = (valw & 0x0003);
		pcistate->LinkWidth = (valw & 0x03f0) >> 4;

		/* Read MPS & MRRS */
		pci_read_config_word(pdev, pos + PCI_EXP_DEVCTL, &valw);
		pcistate->MPS = 128 << ((valw & PCI_EXP_DEVCTL_PAYLOAD) >> 5);
		pcistate->MRRS = 128 << ((valw & PCI_EXP_DEVCTL_READRQ) >> 12);
	}
	else
	{
		printk("Cannot find PCI Express Capabilities\n");
		pcistate->LinkSpeed = pcistate->LinkWidth = 0;
		pcistate->MPS = pcistate->MRRS = 0;
	}

	/* Read Initial Flow Control Credits information */
	base = (unsigned long)(mu2e_pcie_bar_info[dtc].baseVAddr);

	pcistate->InitFCCplD = XIo_In32(base + 0x901c) & 0x00000FFF;
	pcistate->InitFCCplH = XIo_In32(base + 0x9020) & 0x000000FF;
	pcistate->InitFCNPD = XIo_In32(base + 0x9024) & 0x00000FFF;
	pcistate->InitFCNPH = XIo_In32(base + 0x9028) & 0x000000FF;
	pcistate->InitFCPD = XIo_In32(base + 0x902c) & 0x00000FFF;
	pcistate->InitFCPH = XIo_In32(base + 0x9030) & 0x000000FF;
	pcistate->Version = XIo_In32(base + 0x9000);

	return 0;
}  // ReadPCIState

#define descIdx2dmaAddr(idx, dtc, ch, dir)

//////////////////////////////////////////////////////////////////////////////

static int __init init_mu2e(void)
{
	int ret = 0; /* SUCCESS */

	TRACE(0, "init_mu2e");

	// fs interface, pci

	ret = mu2e_fs_up();
	if (ret != 0)
	{
		ret = -2;
		goto out_fs;
	}
	ret = mu2e_pci_up();
	if (ret != 0)
	{
		ret = -5;
		goto out_pci;
	}

	TRACE(1, "init_mu2e completed with ret=%d", ret);
	return (ret);

out_pci:
	TRACE(0, "Error - destroying pci device");
	mu2e_pci_down();
out_fs:
	TRACE(0, "Error - destroying filesystem entry");
	mu2e_fs_down();
	return (ret);
}  // init_mu2e

static void __exit exit_mu2e(void)
{
	TRACE(1, "exit_mu2e() called");

	// events, memory, pci, fs interface
	// free_mem();
	mu2e_pci_down();
	mu2e_fs_down();

	printk("exit_mu2e complete\n");
}  // exit_mu2e

module_init(init_mu2e);
module_exit(exit_mu2e);

MODULE_AUTHOR("Ron Rechenmacher");
MODULE_DESCRIPTION("mu2e pcie driver");
MODULE_LICENSE("GPL"); /* Get rid of taint message by declaring code as GPL */
