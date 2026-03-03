// This file (mu2e_init.h) was created by Ron Rechenmacher <ron@fnal.gov> on
// Feb  5, 2014. "TERMS AND CONDITIONS" governing this file are in the README
// or COPYING file. If you do not have such a file, one can be obtained by
// contacting Ron or Fermi Lab in Batavia IL, 60510, phone: 630-840-3000.
// $RCSfile: .emacs.gnu,v $
// rev="$Revision: 1.23 $$Date: 2012/01/23 15:32:40 $";

#ifndef MU2E_PCI_H
#define MU2E_PCI_H

#include "mu2e_mmap_ioctl.h"

#define XILINX_VENDOR_ID   0x10EE
#define XILINX_DEVICE_ID   0x7042
#define XILINX_DEVICE_ID_2 0x7043

extern int           mu2e_dtc_num;
extern dev_t         mu2e_dev_number;
extern struct class* mu2e_dev_class;

int  mu2e_pci_up(void);
void mu2e_pci_down(void);

#endif  // MU2E_PCI_H
