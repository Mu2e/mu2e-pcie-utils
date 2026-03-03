#ifndef MU2E_MEM_H
#define MU2E_MEM_H

#include "mu2e_mmap_ioctl.h"

/// <summary>
/// Information about the base memory address of the DTC register space
/// </summary>
typedef struct
{
	unsigned long basePAddr; /**< Base address of device memory */
	unsigned long baseLen;   /**< Length of device memory */
	void __iomem* baseVAddr; /**< VA - mapped address */
} bar_info_t;

extern bar_info_t      mu2e_pcie_bar_info[MU2E_MAX_NUM_DTCS];
extern struct pci_dev* mu2e_pci_dev[MU2E_MAX_NUM_DTCS];

int  mu2e_mmap(struct file* file, struct vm_area_struct* vma);
int  alloc_mem(int dtc);
void free_mem(int dtc);

#endif  // MU2E_FS_H
