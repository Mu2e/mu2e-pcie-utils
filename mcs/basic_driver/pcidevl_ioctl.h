// This file (dvl_ioctl.h) was created by Ron Rechenmacher <ron@fnal.gov> on
// Feb  5, 2014. "TERMS AND CONDITIONS" governing this file are in the README
// or COPYING file. If you do not have such a file, one can be obtained by
// contacting Ron or Fermi Lab in Batavia IL, 60510, phone: 630-840-3000.
// $RCSfile: .emacs.gnu,v $
// rev="$Revision: 1.23 $$Date: 2012/01/23 15:32:40 $";

#ifndef DEVL_IOCTL_H
#define DEVL_IOCTL_H

#ifdef __KERNEL__
#include <asm/ioctl.h>  // _IOWR
#else
#include <sys/ioctl.h>  // _IOWR
#include <sys/types.h>
#include <unistd.h>  // sysconf
#endif

#define PCIDEVL_DEV_FILE "pcidev%d"
/*
 For _IO,_IOR,_IOW,_IORW ref. Documentation/ioctl/ioctl-number.txt
 _IO   - implementation has neither copy_from nor copy_to user (or equivalents)
 _IOR  - implementation has copy_to_user   (or equiv., at end)
 _IOW  - implementation has copy_from_user (or equiv., at beginnning)
 _IOWR - implementaions has both copy_from_user (at beginnning) and
				 copy_to_user (at end)
NOTE: for _IOR, _IOW: the size is only for the data at the address used in the
		  ioctl call; NOT for the size at an address contained within the data
		  pointed to by the address used in the ioctl call.  So, if a small
		  structure is pointed to (to be copied in) which has an address of a
		  large buffer, the only thing that these macros should consider is the
		  pointer used (directly) in the ioctl call.
 */
#define DEVL_IOC_MAGIC 'D'

#define IOC_HELLO _IO(DEVL_IOC_MAGIC, 14)

#define IOC_IOREMAP _IO(DEVL_IOC_MAGIC, 17)
#define IOC_IOUNMAP _IO(DEVL_IOC_MAGIC, 18)

#define IOC_UINT32 _IOWR(DEVL_IOC_MAGIC, 18, uint32_t)

enum ioc_ioop_ops
{
	ioop_read = 0x1,
	ioop_write = 0x2
};
/// <summary>
/// IOC I/O Operation definition
/// </summary>
struct ioc_ioop
{
	uint32_t offset;            /**< register offset */
	enum ioc_ioop_ops ops_mask; /**< can do write then read to same offset */
	uint32_t write_val;         ///< Value to write to register
	uint32_t read_val;          ///< Value read from register
};

#define IOC_IOOP _IOWR(DEVL_IOC_MAGIC, 19, struct ioc_ioop)

#endif  // DEVL_IOCTL_H
