/*  This file (pci_devel_main.c) was created by Ron Rechenmacher <ron@fnal.gov> on
		Apr 23, 2014. "TERMS AND CONDITIONS" governing this file are in the README
		or COPYING file. If you do not have such a file, one can be obtained by
		contacting Ron or Fermi Lab in Batavia IL, 60510, phone: 630-840-3000.
		$RCSfile: .emacs.gnu,v $
		rev="$Revision: 1.23 $$Date: 2012/01/23 15:32:40 $";
		*/
#include <linux/uaccess.h> /* access_ok, copy_to_user */
#include <linux/cdev.h>    /* cdev_add */
#include <linux/delay.h>   /* msleep */
#include <linux/device.h>  /* class_create */
#include <linux/fs.h>      /* struct inode */
#include <linux/init.h>    // module_init,_exit
#include <linux/kernel.h>  // KERN_INFO, printk
#include <linux/module.h>  // module_param, THIS_MODULE
#include <linux/pci.h>     /* struct pci_dev *pci_get_device */
#include <linux/types.h>   /* dev_t */
#include <linux/version.h> /* KERNEL_VERSION */

#include "trace.h" /* TRACE */

#include "pcidevl_ioctl.h" /* PCIDEVL_DEV_FILE */

#define DRIVER_NAME "devl_driver"
#define XILINX_VENDOR_ID 0x10ee
#define DTC_DEVICE_ID 0x7042
#define CFO_DEVICE_ID 0x7043

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 39)
#define IOCTL_ARGS(inode, filep, cmd, arg) inode, filep, cmd, arg
#define IOCTL_FILE_OPS_MEMBER ioctl
#define IOCTL_RET_TYPE int
#else
#define IOCTL_ARGS(inode, filep, cmd, arg) filep, cmd, arg
#define IOCTL_FILE_OPS_MEMBER unlocked_ioctl
#define IOCTL_RET_TYPE long
#endif

#if LINUX_VERSION_CODE <= KERNEL_VERSION(4, 0, 0)
#define ACCESS_OK_READ(addr, size) access_ok(VERIFY_READ, addr, size)
#define ACCESS_OK_WRITE(addr, size) access_ok(VERIFY_WRITE, addr, size)
#else
#define ACCESS_OK_READ(addr, size) access_ok(addr, size)
#define ACCESS_OK_WRITE(addr, size) access_ok(addr, size)
#endif

/* GLOBALS */

struct pci_dev *pci_dev_sp[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

/// <summary>
/// Information about the base memory address of the DTC register space
/// </summary>
typedef struct
{
	unsigned long basePAddr; /**< Base address of device memory */
	unsigned long baseLen;   /**< Length of device memory */
	void __iomem *baseVAddr; /**< VA - mapped address */
} bar_info_t;

bar_info_t pcie_bar_info[10] = {{0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}};

//////////////////////////////////////////////////////////////////////////////

IOCTL_RET_TYPE devl_ioctl(IOCTL_ARGS(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg));

static struct file_operations devl_file_ops = {
	.owner = THIS_MODULE,
	.llseek = NULL,                      /* lseek        */
	.read = NULL,                        /* read         */
	.write = NULL,                       /* write        */
										 /*.readdir= NULL,              readdir      */
	.poll = NULL,                        /* poll         */
	.IOCTL_FILE_OPS_MEMBER = devl_ioctl, /* ioctl  */
	.mmap = NULL,                        /* mmap         */
	.open = NULL                         /* open         */
										 /* flush        */
										 /* release (close?)*/
										 /* fsync        */
										 /* fasync       */
										 /* check_media_change */
										 /* revalidate   */
										 /* lock         */
};

dev_t devl_dev_number;
int next_minor_number = 0;
struct class *devl_dev_class;
struct cdev devl_cdev;

int devl_fs_up(void)
{
	int sts;
	sts = alloc_chrdev_region(&devl_dev_number, 0, 10, "devl_drv");

	if (sts < 0)
	{
		TRACE(3, "dcm_init(): Failed to get device numbers");
		return (sts);
	}

#if RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(9, 4)
	devl_dev_class = class_create(THIS_MODULE, "devl_dev");
#else
	devl_dev_class = class_create("devl_dev");
#endif

	cdev_init(&devl_cdev, &devl_file_ops);

	devl_cdev.owner = THIS_MODULE;
	devl_cdev.ops = &devl_file_ops;

	sts = cdev_add(&devl_cdev, devl_dev_number, 10);

	TRACE(5, "devl_fs_up MAJOR=%d MINOR=%d", MAJOR(devl_dev_number), MINOR(devl_dev_number));
	return (0);
}  // devl_fs_up

void devl_fs_down(void)
{
	cdev_del(&devl_cdev);
	class_destroy(devl_dev_class);
	unregister_chrdev_region(devl_dev_number, 1);
}  // devl_fs_down

static int devl_pci_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	int pciRet;

	TRACE(6, "devl_pci_probe pdev=%p", pdev);
	/* Initialize device before it is used by driver. Ask low-level
	 * code to enable I/O and memory. Wake up the device if it was
	 * suspended. Beware, this function can fail.
	 */
	pciRet = pci_enable_device(pdev);
	if (pciRet < 0)
	{
		TRACE(0, KERN_ERR "PCI device enable failed.");
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
		TRACE(0, KERN_ERR "Could not request PCI regions.");
		pci_disable_device(pdev);
		return (pciRet);
	}

	pciRet = pci_set_dma_mask(pdev, DMA_BIT_MASK(32));
	if (pciRet < 0)
	{
		TRACE(0, KERN_ERR "pci_set_dma_mask failed.");
		goto out2;
	}

	TRACE(5, "devl_pci_probe MAJOR=%d MINOR=%d", MAJOR(devl_dev_number), next_minor_number);
	if (next_minor_number < 10)
	{
		pdev->dev.devt = MKDEV(MAJOR(devl_dev_number), next_minor_number);
		device_create(devl_dev_class, NULL, pdev->dev.devt, NULL, PCIDEVL_DEV_FILE, next_minor_number);
		pci_dev_sp[next_minor_number] = pdev; /* GLOBAL */
		next_minor_number++;
	}
	else
		goto out2;
	return (0);

out2:
	TRACE(0, "devl_pci_probe - out2");
	pci_release_regions(pdev);
	pci_disable_device(pdev);
	return (1); /* error */
}  // devl_pci_probe

static void devl_pci_remove(struct pci_dev *pdev)
{
	int ii;
	int match = -1;
	for (ii = 0; ii < next_minor_number; ++ii)
	{
		if (pdev == pci_dev_sp[ii])
		{
			pci_dev_sp[ii] = 0;
			match = ii;
		}
	}

	if (match < 0)
	{
		TRACE(6, "devl_pci_remove device already removed");
		return;
	}

	TRACE(6, "devl_pci_remove start pdev=%p", pdev);
	device_destroy(devl_dev_class, pdev->dev.devt);
	TRACE(6, "devl_pci_remove after device_destroy, before release_regions");
	pci_release_regions(pdev);
	TRACE(6, "devl_pci_remove after release_regions, before disable_device");
	pci_disable_device(pdev);
	TRACE(6, "devl_pci_remove complete");

}  // devl_remove

static struct pci_device_id xilinx_ids[] = {
	{XILINX_VENDOR_ID, DTC_DEVICE_ID, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0UL},
	{XILINX_VENDOR_ID, CFO_DEVICE_ID, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0UL},
	{} /* terminate list with empty entry */
};

static struct pci_driver devl_driver = {
	.name = DRIVER_NAME, .id_table = xilinx_ids, .probe = devl_pci_probe, .remove = devl_pci_remove};

void devl_pci_down(void)
{
	int ii;
	for (ii = 0; ii < next_minor_number; ++ii)
	{
		if (pcie_bar_info[ii].baseVAddr != 0)
		{
			TRACE(6, "devl_pci_down - iounmap( pcie_bar_info.baseVAddr )");
			iounmap(pcie_bar_info[ii].baseVAddr);
			pcie_bar_info[ii].baseVAddr = 0;
		}
		if (pci_dev_sp[ii] == 0)
			TRACE(6, "devl_pci_down - pci_unregister - device NOT registered");
		else
		{
			devl_pci_remove(pci_dev_sp[ii]);
			TRACE(6, "devl_pci_down - pci_unregister called");
		}
	}
	pci_unregister_driver(&devl_driver);
}  // devl_pci_down

IOCTL_RET_TYPE devl_ioctl(IOCTL_ARGS(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg))
{
	IOCTL_RET_TYPE sts = 0;
	int devIdx = iminor(filp->f_path.dentry->d_inode);
	TRACE(6, "devl_ioctl devIdx=%d", devIdx);

	TRACE(6, "Checking IOC_TYPE");
	if (_IOC_TYPE(cmd) != DEVL_IOC_MAGIC) return -ENOTTY;

	TRACE(6, "Checking for correct access for command");
	/* Check read/write and corresponding argument */
	if (_IOC_DIR(cmd) & _IOC_READ)
		if (!ACCESS_OK_WRITE((void *)arg, _IOC_SIZE(cmd))) return -EFAULT;
	if (_IOC_DIR(cmd) & _IOC_WRITE)
		if (!ACCESS_OK_READ((void *)arg, _IOC_SIZE(cmd))) return -EFAULT;

	switch (cmd)
	{
		case IOC_HELLO:
			TRACE(6, "devl_ioctl - hello");
			break;
		case IOC_IOREMAP:
			if (sts == 0)
			{
				u32 size;
				if ((size = pci_resource_len(pci_dev_sp[devIdx], 0 /*bar*/)) == 0)
				{
					TRACE(1, KERN_ERR "BAR 0 not valid, aborting.");
					return (-1);
				}
				if (!(pci_resource_flags(pci_dev_sp[devIdx], 0 /*bar*/) & IORESOURCE_MEM))
				{
					TRACE(1, KERN_ERR "BAR 0 is of wrong type, aborting.");
					return (-1);
				}
				pcie_bar_info[devIdx].basePAddr = pci_resource_start(pci_dev_sp[devIdx], 0 /*bar*/);
				pcie_bar_info[devIdx].baseLen = size;

				pcie_bar_info[devIdx].baseVAddr = ioremap(pcie_bar_info[devIdx].basePAddr, size);
				if (pcie_bar_info[devIdx].baseVAddr == 0UL)
				{
					TRACE(1, KERN_ERR "Cannot map BAR 0 space, invalidating.");
					return (-1);
				}
				TRACE(6, "devl_ioctl - IOREMAP - pcie_bar_info[%d].baseVAddr=%p len=%u", devIdx,
					  pcie_bar_info[devIdx].baseVAddr, size);
			}
			break;
		case IOC_IOUNMAP:
			if (pcie_bar_info[devIdx].baseVAddr == 0)
				TRACE(6, "nothing to unmap");
			else
			{
				TRACE(6, "iounmap-ing %p", pcie_bar_info[devIdx].baseVAddr);
				iounmap(pcie_bar_info[devIdx].baseVAddr);
				pcie_bar_info[devIdx].baseVAddr = 0;
			}
			break;
		case IOC_UINT32:
			TRACE(6, "IOC_UINT32");
			if (pcie_bar_info[devIdx].baseVAddr == 0)
			{
				sts = devl_ioctl(IOCTL_ARGS(inode, filp, IOC_IOREMAP, 0));
				if (sts != 0)
				{
					return -1;
				}
			}
			{
				u32 off, val;
				ulong base = (ulong)pcie_bar_info[devIdx].baseVAddr;
				if (get_user(off, (u32 __user *)arg)) return -EFAULT;
				base += off;
				val = *(u32 *)base;
				TRACE(7, "reading base(%p) + offset(%u/0x%x) = 0x%08x", pcie_bar_info[devIdx].baseVAddr, off, off, val);
				return put_user(val, (u32 __user *)arg);
			}
			break;
		case IOC_IOOP:
			if (pcie_bar_info[devIdx].baseVAddr == 0)
			{
				sts = devl_ioctl(IOCTL_ARGS(inode, filp, IOC_IOREMAP, 0));
				if (sts != 0)
				{
					return -1;
				}
			}
			{
				struct ioc_ioop ioop;
				ulong addr = (ulong)pcie_bar_info[devIdx].baseVAddr;
				if (copy_from_user(&ioop, (void *)arg, sizeof(struct ioc_ioop)))
				{
					TRACE(0, "devl_ioctl IOC_IOOP: copy_from_user failed");
					return -EFAULT;
				}
				addr += ioop.offset;
				if (ioop.ops_mask & ioop_write) *(u32 *)addr = ioop.write_val;
				if (ioop.ops_mask & ioop_read) ioop.read_val = *(u32 *)addr;
				if (copy_to_user((void *)arg, &ioop, sizeof(struct ioc_ioop)))
				{
					TRACE(0, "devl_ioctl IOC_IOOP: copy_to_user failed");
					return -EFAULT;
				}
			}
			break;
		default:
			TRACE(0, "devl_ioctl: unknown cmd");
			return (-1);  // some error
	}
	return (sts);
}  // devl_ioctl

void free_mem(void) {}  // free_mem

//////////////////////////////////////////////////////////////////////////////

static int __init init_devl(void)
{
	int ret = 0; /* SUCCESS */

	TRACE(2, "init_devl");

	// fs interface, pci, memory, events(i.e polling)

	ret = devl_fs_up();
	if (ret) goto out;

	ret = pci_register_driver(&devl_driver);
	TRACE(6, "devl_ioctl - pci_register=%d", ret);

	if (ret) goto out2;

	return (ret);
out2:
	TRACE(0, "Unregistering pci driver");
	devl_pci_down();
out:
	TRACE(0, "Error - freeing memory");
	devl_fs_down();
	return (-1);
}  // init_devl

static void __exit exit_devl(void)
{
	TRACE(1, "exit_devl() called");

	devl_pci_down();

	devl_fs_down();

}  // exit_devl

module_init(init_devl);
module_exit(exit_devl);

MODULE_AUTHOR("Ron Rechenmacher");
MODULE_DESCRIPTION("devl pcie driver");
MODULE_LICENSE("GPL"); /* Get rid of taint message by declaring code as GPL */
