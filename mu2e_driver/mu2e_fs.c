/*  This file (mu2e_fs.c) was created by Ron Rechenmacher <ron@fnal.gov> on
    Feb  5, 2014. "TERMS AND CONDITIONS" governing this file are in the README
    or COPYING file. If you do not have such a file, one can be obtained by
    contacting Ron or Fermi Lab in Batavia IL, 60510, phone: 630-840-3000.
    $RCSfile: .emacs.gnu,v $
    rev="$Revision: 1.23 $$Date: 2012/01/23 15:32:40 $";
    */

#include <linux/cdev.h>   /* cdev_add */
#include <linux/device.h> /* class_create */
#include <linux/fs.h>     /* alloc_chrdev_region */
#include <linux/types.h>  /* dev_t */
#include <linux/cred.h>   /* current_uid, current_euid */

#include "trace.h" /* TRACE */

#include "mu2e_fs.h"
#include "mu2e_mem.h"
#include "mu2e_mmap_ioctl.h" /* MU2E_DEV_FILE */

static struct file_operations mu2e_file_ops = {
	.owner = THIS_MODULE,
	.llseek = NULL, /* lseek        */
	.read = NULL,   /* read         */
	.write = NULL,  /* write        */
	/*.readdir= NULL,              readdir      */
	.poll = NULL,                        /* poll         */
	.IOCTL_FILE_OPS_MEMBER = mu2e_ioctl, /* ioctl  */
	.mmap = mu2e_mmap,                   /* mmap         */
	.open = mu2e_open,                   /* open         */
	.flush = NULL,                       /* flush        */
	.release = mu2e_release,             /* release (close?)*/
	.fsync = 0                                    /* fsync        */
										 /* fasync       */
										 /* check_media_change */
										 /* revalidate   */
										 /* lock         */
};

struct cdev mu2e_cdev;

static int mu2e_dtc_count[MU2E_MAX_NUM_DTCS] = {0};
static kuid_t mu2e_dtc_owner[MU2E_MAX_NUM_DTCS] = {{0}};
static DEFINE_SPINLOCK(mu2e_fs_spinlock);

int mu2e_fs_up()
{
	int sts;
	sts = alloc_chrdev_region(&mu2e_dev_number, 0, MU2E_MAX_NUM_DTCS, "mu2e_drv");

	if (sts < 0)
	{
		TRACE(3, "dcm_init(): Failed to get device numbers");
		return (sts);
	}

#   if RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION( 9, 4 )
	mu2e_dev_class = class_create(THIS_MODULE, "mu2e_dev");
#   else
	mu2e_dev_class = class_create("mu2e_dev");
#   endif
	if((void*)mu2e_dev_class == ERR_PTR) {
		TRACE(0, "mu2e_fs_up failed to create device class");
		return -1;
	}

	cdev_init(&mu2e_cdev, &mu2e_file_ops);

	mu2e_cdev.owner = THIS_MODULE;
	mu2e_cdev.ops = &mu2e_file_ops;

	sts = cdev_add(&mu2e_cdev, mu2e_dev_number, MU2E_MAX_NUM_DTCS);
	// NOTE: permissions set -- use udev - i.e:
	// echo 'KERNEL=="mu2e*", MODE="0666"' >/etc/udev/rules.d/98-mu2e.rules

	return (0);
}  // mu2e_fs

void mu2e_fs_down()
{
	cdev_del(&mu2e_cdev);
	class_destroy(mu2e_dev_class);
	unregister_chrdev_region(mu2e_dev_number, MU2E_MAX_NUM_DTCS);
}

int mu2e_open(struct inode *inode, struct file *filp)
{
	int dtc = iminor(inode);

	spin_lock(&mu2e_fs_spinlock);
	if (mu2e_dtc_count[dtc] &&
		(mu2e_dtc_owner[dtc].val != current_uid().val) &&  /* allow user */
		(mu2e_dtc_owner[dtc].val != current_euid().val) && /* allow whoever did su */
		!capable(CAP_DAC_OVERRIDE))
	{ /* still allow root */
		spin_unlock(&mu2e_fs_spinlock);
		return -EBUSY; /* -EPERM would confuse the user */
	}

	if (mu2e_dtc_count[dtc] == 0)
	{
		mu2e_dtc_owner[dtc] = current_uid(); /* grab it */
		inode->i_uid = current_uid();
		mark_inode_dirty(inode);
	}

	mu2e_dtc_count[dtc]++;
	spin_unlock(&mu2e_fs_spinlock);

	return 0;
}

int mu2e_release(struct inode *inode, struct file *filp)
{
	int dtc = iminor(inode);
	spin_lock(&mu2e_fs_spinlock);
	mu2e_dtc_count[dtc]--;
	if (mu2e_dtc_count[dtc] == 0)
	{
		inode->i_uid = (kuid_t)0;
		mark_inode_dirty(inode);
    }
	spin_unlock(&mu2e_fs_spinlock);
	return 0;
}
