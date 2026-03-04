// This file (mu2e_proto_globals.h) was created by Ron Rechenmacher <ron@fnal.gov> on
// Feb 12, 2014. "TERMS AND CONDITIONS" governing this file are in the README
// or COPYING file. If you do not have such a file, one can be obtained by
// contacting Ron or Fermi Lab in Batavia IL, 60510, phone: 630-840-3000.
// $RCSfile: .emacs.gnu,v $
// rev="$Revision: 1.23 $$Date: 2012/01/23 15:32:40 $";
#ifndef MU2E_PROTO_GLOBALS_H
#define MU2E_PROTO_GLOBALS_H

#include "mu2e_mmap_ioctl.h"  // m_ioc_get_info_t
#include "xdma_hw.h"          // mu2e_buffdesc_C2S_t

/* At 10 Gigabits-per-second... (approx. 1 GigaByte/s)
   60*65536 ~= 3.9 MB / 1000 MB/s ~= 3.9 ms.
   250 Hz should work.
 */
#define MU2E_NUM_RECV_BUFFS     100 /*4*/ /*20*/
#define MU2E_NUM_RECV_CHANNELS  2
#define MU2E_RECV_INTER_ENABLED 1

extern mu2e_databuff_t*     mu2e_recv_databuff_rings[MU2E_MAX_NUM_DTCS][MU2E_NUM_RECV_CHANNELS];
extern mu2e_buffdesc_C2S_t* mu2e_recv_buffdesc_rings[MU2E_MAX_NUM_DTCS][MU2E_NUM_RECV_CHANNELS];

#define MU2E_NUM_SEND_BUFFS    60 /*20*/ /*60*/ /*Maximum value: ~60*/
#define MU2E_NUM_SEND_CHANNELS 2
extern mu2e_databuff_t*     mu2e_send_databuff_rings[MU2E_MAX_NUM_DTCS][MU2E_NUM_SEND_CHANNELS];
extern mu2e_buffdesc_S2C_t* mu2e_send_buffdesc_rings[MU2E_MAX_NUM_DTCS][MU2E_NUM_SEND_CHANNELS];

/// <summary>
/// PCI Sending interface data
/// </summary>
typedef struct
{
	mu2e_databuff_t*     databuffs;          ///< Data buffers for transmitting
	dma_addr_t           databuffs_dma;      ///< DMA address for buffers for transmitting
	mu2e_buffdesc_S2C_t* buffdesc_ring;      ///< Descriptors for transmitting
	dma_addr_t           buffdesc_ring_dma;  ///< DMA address for descriptors for transmitting
} pci_sender_t;

extern pci_sender_t mu2e_pci_sender[MU2E_MAX_NUM_DTCS][MU2E_NUM_SEND_CHANNELS];

/// <summary>
/// PCI Receiving interface data
/// </summary>
typedef struct
{
	mu2e_databuff_t**     databuffs;          ///< pointers to data buffers for receiving
	dma_addr_t*           databuffs_dma;      ///< DMA address of data buffers for receiving
	mu2e_buffdesc_C2S_t** buffdesc_ring;      ///< Pointers to buffer descriptors for receiving
	dma_addr_t*           buffdesc_ring_dma;  ///< DMA Address of buffer descriptor ring for receiving
} pci_recver_t;

extern pci_recver_t mu2e_pci_recver[MU2E_MAX_NUM_DTCS][MU2E_NUM_RECV_CHANNELS];

extern volatile void* mu2e_mmap_ptrs[MU2E_MAX_NUM_DTCS][MU2E_MAX_CHANNELS][2][2];

// This variable name is used in a macro that expects the same
// variable name in the user-space "library"
extern m_ioc_get_info_t mu2e_channel_info_[MU2E_MAX_NUM_DTCS][MU2E_MAX_CHANNELS][2];

#endif  // MU2E_PROTO_GLOBALS_H
