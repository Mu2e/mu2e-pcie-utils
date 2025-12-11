
#include <linux/delay.h> /* msleep */
#include <linux/fs.h>
#include <linux/kernel.h>  // KERN_INFO, printk
#include <linux/module.h>  // module_param, THIS_MODULE
#include <linux/pci.h>
#include <linux/time.h>

#include "mu2e_mem.h"
#include "mu2e_proto_globals.h"

int mu2e_mmap(struct file *file, struct vm_area_struct *vma)
{
	int ch, dir, map;
	unsigned long phys_addr, uaddr;
	int sts = 0, ii;
	int dtc = iminor(file->f_path.dentry->d_inode);

	page2chDirMap(vma->vm_pgoff, ch, dir, map);
	TRACE(TLVL_DEBUG + 4, "mu2e_mmap: vm_pgoff:%lu dtc:%d ch:%d dir:%d map:%d: %p", vma->vm_pgoff, dtc, ch, dir, map,
		  mu2e_mmap_ptrs[dtc][ch][dir][map]);
	if (map == MU2E_MAP_META) vm_flags_clear(vma, VM_WRITE);

	if (dir == C2S && map == MU2E_MAP_BUFF)
	{
		uaddr = vma->vm_start;
		for (ii = 0; ii < MU2E_NUM_RECV_BUFFS; ++ii)
		{
			phys_addr = virt_to_phys(((void **)mu2e_mmap_ptrs[dtc][ch][dir][map])[ii]);
			sts |= io_remap_pfn_range(vma, uaddr, phys_addr >> PAGE_SHIFT, sizeof(mu2e_databuff_t), vma->vm_page_prot);
			uaddr += sizeof(mu2e_databuff_t);
		}
	}
	else
	{
		phys_addr = virt_to_phys(mu2e_mmap_ptrs[dtc][ch][dir][map]);
		sts =
			io_remap_pfn_range(vma, vma->vm_start, phys_addr >> PAGE_SHIFT, vma->vm_end - vma->vm_start, vma->vm_page_prot);
	}
	if (sts) return -EAGAIN;

	return (0);
}  // mu2e_mmap

/////////////////////////////////////////////////////////////////////////////////////////////
int alloc_mem(int dtc)
{
	int ret = 0; /* SUCCESS */
	unsigned chn, ii, jj, dir;
	void *va, *vb;
	unsigned long databuff_sz;
	unsigned long buffdesc_sz;
	u32 descDmaAdr;
	u32 ctrlStsVal;

	/* Use "Dma_" routines to init FPGA "user" application ("DTC") registers.
  NOTE: a few more after dma engine setup (below).
  */
	Dma_mWriteReg((unsigned long)mu2e_pcie_bar_info[dtc].baseVAddr, 0x9100, 1);  // Hard reset
	TRACE(TLVL_INFO, "alloc_mem reset done bits: 0x%08x MU2E_NUM_RECV_CHANNELS=%d MU2E_NUM_RECV_BUFFS=%d MU2E_NUM_SEND_BUFFS=%d",
		  Dma_mReadReg((unsigned long)mu2e_pcie_bar_info[dtc].baseVAddr, 0x9138), MU2E_NUM_RECV_CHANNELS,
		  MU2E_NUM_RECV_BUFFS, MU2E_NUM_SEND_BUFFS);

	Dma_mWriteReg((unsigned long)mu2e_pcie_bar_info[dtc].baseVAddr, 0x9030, mu2e_host_hash(dtc, NULL));  // DTC ID
	Dma_mWriteReg((unsigned long)mu2e_pcie_bar_info[dtc].baseVAddr, 0x9188, time(0));              // Set time register

	/* DMA Engine (channels) setup... (buffers and descriptors (and metadata)) */
	dir = C2S;
	for (chn = 0; chn < MU2E_NUM_RECV_CHANNELS; ++chn)
	{
		TRACE(TLVL_LOG, "alloc_mem dma_alloc (#=%d)", MU2E_NUM_RECV_BUFFS);

		va = kmalloc(MU2E_NUM_RECV_BUFFS * sizeof(void *), GFP_KERNEL);  // Array of data buffer pointers
		vb = kmalloc(MU2E_NUM_RECV_BUFFS * sizeof(void *), GFP_KERNEL);  // Array of buffdesc pointers
		if (va == NULL || vb == NULL) goto out;
		mu2e_pci_recver[dtc][chn].databuffs = va;
		mu2e_pci_recver[dtc][chn].buffdesc_ring = vb;

		va = kmalloc(MU2E_NUM_RECV_BUFFS * sizeof(dma_addr_t), GFP_KERNEL);  // dma addresses of data buffers
		vb = kmalloc(MU2E_NUM_RECV_BUFFS * sizeof(dma_addr_t), GFP_KERNEL);  // dma addresses of buffdesc memory
		if (va == NULL || vb == NULL) goto out;
		mu2e_pci_recver[dtc][chn].databuffs_dma = va;
		mu2e_pci_recver[dtc][chn].buffdesc_ring_dma = vb;

		for (ii = 0; ii < MU2E_NUM_RECV_BUFFS; ++ii)
		{
			va = dma_alloc_coherent(&mu2e_pci_dev[dtc]->dev, sizeof(mu2e_databuff_t), &(mu2e_pci_recver[dtc][chn].databuffs_dma[ii]), GFP_KERNEL);
			if (va == NULL) goto out;
			mu2e_pci_recver[dtc][chn].databuffs[ii] = va;
			va = dma_alloc_coherent(&mu2e_pci_dev[dtc]->dev, sizeof(mu2e_buffdesc_C2S_t), &(mu2e_pci_recver[dtc][chn].buffdesc_ring_dma[ii]), GFP_KERNEL);
			if (va == NULL) goto out;
			mu2e_pci_recver[dtc][chn].buffdesc_ring[ii] = va;
			TRACE(TLVL_LOG,
				  "alloc_mem mu2e_pci_recver[%d][%u][%u].databuffs=%p databuffs_dma=0x%llx "
				  "buffdesc_ring=%p buffdesc_ring_dma=0x%llx",
				  dtc, chn, ii, mu2e_pci_recver[dtc][chn].databuffs[ii], mu2e_pci_recver[dtc][chn].databuffs_dma[ii],
				  mu2e_pci_recver[dtc][chn].buffdesc_ring[ii], mu2e_pci_recver[dtc][chn].buffdesc_ring_dma[ii]);
		}

		mu2e_mmap_ptrs[dtc][chn][dir][MU2E_MAP_BUFF] = mu2e_pci_recver[dtc][chn].databuffs;
		mu2e_mmap_ptrs[dtc][chn][dir][MU2E_MAP_META] = (void *)__get_free_pages(GFP_KERNEL, 0);

		TRACE(TLVL_LOG, "alloc_mem mu2e_pci_recver[%d][%u].meta@%p", dtc, chn, mu2e_mmap_ptrs[dtc][chn][dir][MU2E_MAP_META]);

		mu2e_channel_info_[dtc][chn][dir].chn = chn;
		mu2e_channel_info_[dtc][chn][dir].dir = dir;
		mu2e_channel_info_[dtc][chn][dir].buff_size = sizeof(mu2e_databuff_t);
		mu2e_channel_info_[dtc][chn][dir].num_buffs = MU2E_NUM_RECV_BUFFS;

		for (jj = 0; jj < MU2E_NUM_RECV_BUFFS; ++jj)
		{ /* ring -> link to next (and last to 1st via modulus) */
			mu2e_pci_recver[dtc][chn].buffdesc_ring[jj]->NextDescPtr =
				mu2e_pci_recver[dtc][chn].buffdesc_ring_dma[(jj + 1) % MU2E_NUM_RECV_BUFFS];
			/* put the _buffer_ address in the descriptor */
			mu2e_pci_recver[dtc][chn].buffdesc_ring[jj]->SystemAddress = mu2e_pci_recver[dtc][chn].databuffs_dma[jj];
			/* and the size of the buffer also */
			mu2e_pci_recver[dtc][chn].buffdesc_ring[jj]->RsvdByteCnt = sizeof(mu2e_databuff_t);
#if MU2E_RECV_INTER_ENABLED
			mu2e_pci_recver[dtc][chn].buffdesc_ring[jj]->IrqComplete = 1;
			mu2e_pci_recver[dtc][chn].buffdesc_ring[jj]->IrqError = 1;
#else
			mu2e_pci_recver[dtc][chn].buffdesc_ring[jj]->IrqComplete = 0;
			mu2e_pci_recver[dtc][chn].buffdesc_ring[jj]->IrqError = 0;
#endif
		}

		// now write to the HW...
		TRACE(TLVL_LOG, "alloc_mem write 0x%llx to 32bit reg", mu2e_pci_recver[dtc][chn].buffdesc_ring_dma[0]);
		Dma_mWriteChnReg(dtc, chn, dir, REG_DMA_ENG_CTRL_STATUS, DMA_ENG_RESET);
		msleep(20);
		Dma_mWriteChnReg(dtc, chn, dir, REG_HW_NEXT_BD, (u32)mu2e_pci_recver[dtc][chn].buffdesc_ring_dma[0]);
		mu2e_channel_info_[dtc][chn][dir].hwIdx = 0;
		// TRACE(TLVL_LOG, "recver[chn=%d] REG_HW_NEXT_BD=%u"
		//    , chn, descDmaAdr2idx( (u32)mu2e_pci_recver[dtc][chn].buffdesc_ring_dma,chn,dir));

		// set "DMA_ENG" (ie. HW) last/complete == SW NEXT to show "num avail" == 0
		descDmaAdr = idx2descDmaAdr(MU2E_NUM_RECV_BUFFS - 1, dtc, chn, dir);
		Dma_mWriteChnReg(dtc, chn, dir, REG_SW_NEXT_BD, descDmaAdr);
		Dma_mWriteChnReg(dtc, chn, dir, REG_HW_CMPLT_BD, descDmaAdr);
		mu2e_channel_info_[dtc][chn][dir].hwIdx = MU2E_NUM_RECV_BUFFS - 1;
		mu2e_channel_info_[dtc][chn][dir].swIdx = MU2E_NUM_RECV_BUFFS - 1;

		ctrlStsVal = DMA_ENG_ENABLE;
#if MU2E_RECV_INTER_ENABLED
		TRACE(TLVL_LOG, "alloc_mem: ctrlStsVal |= DMA_ENG_INT_ENABLE");
		ctrlStsVal |= DMA_ENG_INT_ENABLE;
#else
		TRACE(TLVL_LOG, "alloc_mem: no DmaInterrrupt");
#endif
		Dma_mWriteChnReg(dtc, chn, dir, REG_DMA_ENG_CTRL_STATUS, ctrlStsVal);
	}

	dir = S2C;
	for (chn = 0; chn < MU2E_NUM_SEND_CHANNELS; ++chn)
	{
		databuff_sz = sizeof(mu2e_databuff_t) * MU2E_NUM_SEND_BUFFS;
		buffdesc_sz = sizeof(mu2e_buffdesc_C2S_t) * MU2E_NUM_SEND_BUFFS;
		TRACE(TLVL_LOG, "alloc_mem dma_alloc (#=%d) databuff_sz=%lu buffdesc_sz=%lu", MU2E_NUM_SEND_BUFFS, databuff_sz,
			  buffdesc_sz);
		va = dma_alloc_coherent(&mu2e_pci_dev[dtc]->dev, databuff_sz, &mu2e_pci_sender[dtc][chn].databuffs_dma, GFP_KERNEL);
		if (va == NULL) goto out;
		mu2e_pci_sender[dtc][chn].databuffs = va;
		mu2e_mmap_ptrs[dtc][chn][dir][MU2E_MAP_BUFF] = va;
		va = dma_alloc_coherent(&mu2e_pci_dev[dtc]->dev, buffdesc_sz, &mu2e_pci_sender[dtc][chn].buffdesc_ring_dma,
								GFP_KERNEL);
		if (va == NULL) goto out;
		mu2e_pci_sender[dtc][chn].buffdesc_ring = va;
		mu2e_mmap_ptrs[dtc][chn][dir][MU2E_MAP_META] = (void *)__get_free_pages(GFP_KERNEL, 0);

		TRACE(TLVL_LOG,
			  "alloc_mem mu2e_pci_sender[%d][%u].databuffs=%p databuffs_dma=0x%llx "
			  "buffdesc_ring_dma=0x%llx meta@%p",
			  dtc, chn, mu2e_pci_sender[dtc][chn].databuffs, mu2e_pci_sender[dtc][chn].databuffs_dma,
			  mu2e_pci_sender[dtc][chn].buffdesc_ring_dma, mu2e_mmap_ptrs[dtc][chn][dir][MU2E_MAP_META]);
		mu2e_channel_info_[dtc][chn][dir].chn = chn;
		mu2e_channel_info_[dtc][chn][dir].dir = dir;
		mu2e_channel_info_[dtc][chn][dir].buff_size = sizeof(mu2e_databuff_t);
		mu2e_channel_info_[dtc][chn][dir].num_buffs = MU2E_NUM_SEND_BUFFS;

		for (jj = 0; jj < MU2E_NUM_SEND_BUFFS; ++jj)
		{ /* ring -> link to next (and last to 1st via modulus) */
			mu2e_pci_sender[dtc][chn].buffdesc_ring[jj].NextDescPtr =
				mu2e_pci_sender[dtc][chn].buffdesc_ring_dma + sizeof(mu2e_buffdesc_S2C_t) * ((jj + 1) % MU2E_NUM_SEND_BUFFS);
			/* put the _buffer_ address in the descriptor */
			mu2e_pci_sender[dtc][chn].buffdesc_ring[jj].SystemAddress =
				mu2e_pci_sender[dtc][chn].databuffs_dma + sizeof(mu2e_databuff_t) * jj;
			mu2e_pci_sender[dtc][chn].buffdesc_ring[jj].Complete =
				1;  // Only reset just before giving to Engine -- enables check for complete -- which should be done before
					// memcpy
		}

		// now write to the HW...
		Dma_mWriteChnReg(dtc, chn, dir, REG_DMA_ENG_CTRL_STATUS, DMA_ENG_RESET);
		msleep(20);
		// HW_NEXT and SW_Next registers to start of ring
		Dma_mWriteChnReg(dtc, chn, dir, REG_HW_NEXT_BD, (u32)mu2e_pci_sender[dtc][chn].buffdesc_ring_dma);
		mu2e_channel_info_[dtc][chn][dir].hwIdx = 0;
		Dma_mWriteChnReg(dtc, chn, dir, REG_SW_NEXT_BD, (u32)mu2e_pci_sender[dtc][chn].buffdesc_ring_dma);
		mu2e_channel_info_[dtc][chn][dir].swIdx = 0;

		// reset HW_Completed register
		Dma_mWriteChnReg(dtc, chn, dir, REG_HW_CMPLT_BD, 0);

		TRACE(TLVL_LOG, "alloc_mem enabling DMA engine");
		Dma_mWriteChnReg(dtc, chn, dir, REG_DMA_ENG_CTRL_STATUS, DMA_ENG_ENABLE);
	}

	TRACE(TLVL_LOG, "alloc_mem setting DTC DMA registers");
	/* Now, finish up with some more mu2e fpga user application stuff... */
	Dma_mWriteReg((unsigned long)mu2e_pcie_bar_info[dtc].baseVAddr, 0x9104, (SET_DTC_MAX_DMA_SIZE << 16) | 0x0040);  // write max and min DMA xfer sizes
	Dma_mWriteReg((unsigned long)mu2e_pcie_bar_info[dtc].baseVAddr, 0x9150, 0x00000010);                             // set ring packet size

	TRACE(TLVL_LOG, "alloc_mem complete");
	return ret;
out:
	TRACE(TLVL_ERROR, "alloc_mem, failed to allocate, aborting!");
	free_mem(dtc);
	return -1;
}

void free_mem(int dtc)
{
	unsigned chn, jj, ii;

	TRACE(TLVL_LOG, "free_mem reset DTC");
	// stop "app"

	for (chn = 0; chn < 2; ++chn)
	{
		// stop engines (both C2S and S2C channels)
		for (jj = 0; jj < 2; ++jj)  // this is "direction"
		{
			TRACE(TLVL_LOG, "free_mem Shutting down dma engine dtc=%d, chn=%d, dir=%d", dtc, chn, jj);
			Dma_mWriteChnReg(dtc, chn, jj, REG_DMA_ENG_CTRL_STATUS, DMA_ENG_USER_RESET);
			msleep(10);
			Dma_mWriteChnReg(dtc, chn, jj, REG_DMA_ENG_CTRL_STATUS, DMA_ENG_RESET);
			msleep(10);
		}
	}

	TRACE(TLVL_LOG, "free_mem Freeing RECV buffers");
	for (chn = 0; chn < MU2E_NUM_RECV_CHANNELS; ++chn)
	{
		for (ii = 0; ii < MU2E_NUM_RECV_BUFFS; ++ii)
		{
			if (mu2e_pci_recver[dtc][chn].databuffs[ii])
				dma_free_coherent(&mu2e_pci_dev[dtc]->dev, sizeof(mu2e_databuff_t), mu2e_pci_recver[dtc][chn].databuffs[ii],
								  mu2e_pci_recver[dtc][chn].databuffs_dma[ii]);
			if (mu2e_pci_recver[dtc][chn].buffdesc_ring[ii])
				dma_free_coherent(&mu2e_pci_dev[dtc]->dev, sizeof(mu2e_buffdesc_C2S_t),
								  mu2e_pci_recver[dtc][chn].buffdesc_ring[ii], mu2e_pci_recver[dtc][chn].buffdesc_ring_dma[ii]);
		}
		kfree(mu2e_pci_recver[dtc][chn].databuffs);
		kfree(mu2e_pci_recver[dtc][chn].buffdesc_ring);
		kfree(mu2e_pci_recver[dtc][chn].databuffs_dma);
		kfree(mu2e_pci_recver[dtc][chn].buffdesc_ring_dma);
		free_pages((unsigned long)mu2e_mmap_ptrs[dtc][chn][C2S][MU2E_MAP_META], 0);
	}
	TRACE(TLVL_LOG, "free_mem Freeing SEND buffers");
	for (chn = 0; chn < MU2E_NUM_SEND_CHANNELS; ++chn)
	{
		if (mu2e_pci_sender[dtc][chn].databuffs)
			dma_free_coherent(&mu2e_pci_dev[dtc]->dev, sizeof(mu2e_databuff_t) * MU2E_NUM_SEND_BUFFS,
							  mu2e_pci_sender[dtc][chn].databuffs, mu2e_pci_sender[dtc][chn].databuffs_dma);
		if (mu2e_pci_sender[dtc][chn].buffdesc_ring)
			dma_free_coherent(&mu2e_pci_dev[dtc]->dev, sizeof(mu2e_buffdesc_S2C_t) * MU2E_NUM_SEND_BUFFS,
							  mu2e_pci_sender[dtc][chn].buffdesc_ring, mu2e_pci_sender[dtc][chn].buffdesc_ring_dma);
		free_pages((unsigned long)mu2e_mmap_ptrs[dtc][chn][S2C][MU2E_MAP_META], 0);
	}
}  // free_mem
