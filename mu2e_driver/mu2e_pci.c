/*  This file (mu2e_init.c) was created by Ron Rechenmacher <ron@fnal.gov> on
		Feb  5, 2014. "TERMS AND CONDITIONS" governing this file are in the README
		or COPYING file. If you do not have such a file, one can be obtained by
		contacting Ron or Fermi Lab in Batavia IL, 60510, phone: 630-840-3000.
		$RCSfile: .emacs.gnu,v $
		rev="$Revision: 1.23 $$Date: 2012/01/23 15:32:40 $";
		*/

#include <linux/fs.h>  /* struct inode */
#include <linux/pci.h> /* pci_* */

#include "trace.h" /* TRACE */

#include "mu2e_event.h"
#include "mu2e_mem.h"
#include "mu2e_pci.h"
#include "mu2e_proto_globals.h" /* C2S */
#include "xdma_hw.h"            /* Dma_mIntDisable nests xio.h -> xbasic_types.h */

#define DRIVER_NAME "mu2e_driver"

/** PCI device structure which probes for targeted design */
static struct pci_device_id xilinx_ids[] = {
	{XILINX_VENDOR_ID, XILINX_DEVICE_ID, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0UL},
	{XILINX_VENDOR_ID, XILINX_DEVICE_ID_2, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0UL},
	{} /* terminate list with empty entry */
};

int MSIEnabled[MU2E_MAX_NUM_DTCS] = {0};

/* Return 0==SUCCESS, 1=FAIL
 */
static int ReadDMAEngineConfiguration(struct pci_dev *pdev
									  /*, struct privData * dmaInfo*/)
{
	unsigned long base, reg_offset;
	u32 val, type, dirn, num, bc;
	int ii;
	//    Dma_Engine *  eptr;
	u32 Hardware_design_version;
	int dtc = MINOR(pdev->dev.devt);
	TRACE(19, "ReadDMAEngineConfiguration MINOR(pdev->dev.devt)=%d", dtc);

	/* DMA registers are offset from BAR0 */
	base = (unsigned long)(mu2e_pcie_bar_info[dtc].baseVAddr);

	Hardware_design_version = XIo_In32(base + 0x9000);
	printk(KERN_INFO "Hardware design version %x from %lx\n", Hardware_design_version, base);
	if (Hardware_design_version == 0xffffffff)
	{
		printk(KERN_ERR "ReadDMAEngineConfiguration: Invalid Hardware_design_version 0x%x\n", Hardware_design_version);
		return (1);
	}

	/* Walk through the capability register of all DMA engines */
	for (reg_offset = DMA_OFFSET, ii = 0; reg_offset < DMA_SIZE; reg_offset += DMA_ENGINE_PER_SIZE, ++ii)
	{
		val = Dma_mReadReg((base + reg_offset), REG_DMA_ENG_CTRL_STATUS);
		val = Dma_mReadReg((base + reg_offset), REG_DMA_ENG_CAP);
		TRACE(21, "REG_DMA_ENG_CAP (capability, reg_offset=0x%04lx) returned 0x%x\n", reg_offset + REG_DMA_ENG_CAP, val);

		if (val & DMA_ENG_PRESENT_MASK)
		{
			printk("DMA Engine present at reg_offset %lx: ", reg_offset);

			dirn = (val & DMA_ENG_DIRECTION_MASK);
			if (dirn == DMA_ENG_C2S)
				printk("C2S, ");
			else
				printk("S2C, ");

			type = (val & DMA_ENG_TYPE_MASK);
			if (type == DMA_ENG_BLOCK)
				printk("Block DMA, ");
			else if (type == DMA_ENG_PACKET)
				printk("Packet DMA, ");
			else
				printk("Unknown DMA %x, ", type);

			num = (val & DMA_ENG_NUMBER) >> DMA_ENG_NUMBER_SHIFT;
			printk("Eng. Number %d, ", num);

			bc = (val & DMA_ENG_BD_MAX_BC) >> DMA_ENG_BD_MAX_BC_SHIFT;
			printk("Max Byte Count 2^%d\n", bc);

			if (type != DMA_ENG_PACKET)
			{
				printk(KERN_ERR "This driver is capable of only Packet DMA\n");
				continue;
			}
		}
	}
	return (0);
}  // ReadDMAEngineConfiguration

static int mu2e_pci_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	int pciRet;
	int bar = 0;
	u32 size;
	int dtc;
	struct device *devptr;

	for (dtc = 0; dtc < MU2E_MAX_NUM_DTCS;)
	{
		if (!mu2e_pci_dev[dtc]) break;
		++dtc;
	}
	if (dtc == MU2E_MAX_NUM_DTCS)
	{
		TRACE(0, "mu2e_pci_probe: TOO MANY DTCS!!!");
		return -2;
	}
	pdev->dev.devt = MKDEV(MAJOR(mu2e_dev_number), dtc);

	TRACE(0, "mu2e_pci_probe pdev=%p, MINOR(pdev->dev.devt)=%d", pdev, MINOR(pdev->dev.devt));
	/* Initialize device before it is used by driver. Ask low-level
	 * code to enable I/O and memory. Wake up the device if it was
	 * suspended. Beware, this function can fail.
	 */
	pciRet = pci_enable_device(pdev);
	if (pciRet < 0)
	{
		printk(KERN_ERR "PCI device enable failed.\n");
		return (pciRet);
	}

	/*
	 * Enable bus-mastering on device. Calls pcibios_set_master() to do
	 * the needed architecture-specific settings.
	 */
	pci_set_master(pdev);

	pciRet = pci_request_regions(pdev, DRIVER_NAME);
	if (pciRet < 0)
	{
		printk(KERN_ERR "Could not request PCI regions.\n");
		pci_disable_device(pdev);
		return (pciRet);
	}

	pciRet = pci_set_dma_mask(pdev, DMA_BIT_MASK(32));
	if (pciRet < 0)
	{
		printk(KERN_ERR "pci_set_dma_mask failed\n");
		goto out2;
	}

	if ((size = pci_resource_len(pdev, bar)) == 0)
	{
		printk(KERN_ERR "BAR %d not valid, aborting.\n", bar);
		goto out2;
	}

	/* Check all BARs for memory-mapped or I/O-mapped. The driver is
	 * intended to be memory-mapped.
	 */
	if (!(pci_resource_flags(pdev, bar) & IORESOURCE_MEM))
	{
		printk(KERN_ERR "BAR %d is of wrong type, aborting.\n", bar);
		goto out2;
	}

	mu2e_pcie_bar_info[dtc].basePAddr = pci_resource_start(pdev, bar);
	mu2e_pcie_bar_info[dtc].baseLen = size;

	mu2e_pcie_bar_info[dtc].baseVAddr = ioremap(mu2e_pcie_bar_info[dtc].basePAddr, size);
	if (mu2e_pcie_bar_info[dtc].baseVAddr == 0UL)
	{
		printk(KERN_ERR "Cannot map BAR %d space, invalidating.\n", bar);
		goto out2;
	}

	/* Disable global interrupts */
	Dma_mIntDisable(mu2e_pcie_bar_info[dtc].baseVAddr);

	TRACE(1, "mu2e_pci_probe read a channel reg to quiet compiler 0x%x", Dma_mReadChnReg(dtc, 0, C2S, REG_HW_CMPLT_BD));

	/* Read DMA engine configuration and initialise data structures */
	if (ReadDMAEngineConfiguration(pdev /*, dmaData*/) != 0) goto out2;
	mu2e_pci_dev[dtc] = pdev; /* GLOBAL */

#if 1
	if (alloc_mem(dtc) != 0) goto out2;
#endif

	TRACE(1, "mu2e_pci_probe creating device");
	devptr = device_create(mu2e_dev_class, NULL, pdev->dev.devt, NULL, MU2E_DEV_FILE, dtc);
	if ((void *)devptr == ERR_PTR) goto out2;

	TRACE(1, "mu2e_pci_probe enabling events");
	mu2e_event_up(dtc);

#if MU2E_RECV_INTER_ENABLED
#if 1
	TRACE(1, "mu2e_pci_probe calling pci_enable_msi");
	/* Now enable interrupts using MSI mode */
	if (!pci_enable_msi(mu2e_pci_dev[dtc]))
	{
		TRACE(1, "MSI enabled");
		MSIEnabled[dtc] = 1;
	}
#endif

	TRACE(1, "mu2e_pci_probe request_irq");
	pciRet = request_irq(mu2e_pci_dev[dtc]->irq, DmaInterrupt, IRQF_SHARED, "mu2e", mu2e_pci_dev[dtc]);
	if (pciRet)
	{
		TRACE(0, "xdma could not allocate interrupt %d", mu2e_pci_dev[dtc]->irq);
		TRACE(0, "Unload driver and try running with polled mode instead");
		goto out2;
	}
	Dma_mIntEnable((unsigned long)mu2e_pcie_bar_info[dtc].baseVAddr);
#endif

	TRACE(1, "mu2e_pci_probe complete");
	return (0); /* SUCCESS */

out2:
	printk("mu2e_pci_probe - out2\n");
	pci_release_regions(pdev);
	pci_disable_device(pdev);
	return (1); /* error */
}  // mu2e_pci_probe

static void mu2e_pci_remove(struct pci_dev *pdev)
{
	int dtc = MINOR(pdev->dev.devt);

	if (mu2e_pci_dev[dtc] == 0) return;
	printk("mu2e_pci_remove start dtc=%d\n ", dtc);

	printk("mu2e_pci_remove dtc=%d disabling interrupts\n", dtc);
	Dma_mIntDisable((unsigned long)mu2e_pcie_bar_info[dtc].baseVAddr);

#if MU2E_RECV_INTER_ENABLED
	printk("mu2e_pci_remove dtc=%d freeing IRQ %d\n", dtc, pdev->irq);
	free_irq(pdev->irq, pdev);

	printk("mu2e_pci_remove dtc=%d disabling MSI (enabled=%d)\n", dtc, MSIEnabled[dtc]);
	if (MSIEnabled[dtc]) pci_disable_msi(pdev);
#endif

	printk("mu2e_pci_remove dtc=%d disabling events\n", dtc);
	mu2e_event_down(dtc);

	printk("mu2e_pci_remove dtc=%d destroying device\n", dtc);
	device_destroy(mu2e_dev_class, pdev->dev.devt);

#if 1
	printk("mu2e_pci_remove dtc=%d freeing memory\n", dtc);
	free_mem(dtc);
#endif

	printk("mu2e_pci_remove dtc=%d calling iounmap\n", dtc);
	iounmap(mu2e_pcie_bar_info[dtc].baseVAddr);

	printk("mu2e_pci_remove dtc=%d releasing pci regions\n", dtc);
	pci_release_regions(pdev);
	printk("mu2e_pci_remove dtc=%d after release_regions, before disable_device\n", dtc);
	pci_disable_device(pdev);
	printk("mu2e_pci_remove dtc=%d after disable_device, before set_drvdata\n", dtc);
	pci_set_drvdata(pdev, NULL);
	printk("mu2e_pci_remove dtc=%d complete\n", dtc);
	mu2e_pci_dev[dtc] = 0;
}  // mu2e_remove

static struct pci_driver mu2e_driver = {
	.name = DRIVER_NAME, .id_table = xilinx_ids, .probe = mu2e_pci_probe, .remove = mu2e_pci_remove};

int mu2e_pci_up(void)
{
	int sts;
	sts = pci_register_driver(&mu2e_driver);
	return (0);
}  // mu2e_pci_up

void mu2e_pci_down(void)
{
	printk("mu2e_pci_down BEGIN\n");
	pci_unregister_driver(&mu2e_driver);
	printk("mu2e_pci_down END\n");
}  // mu2e_pci_down
