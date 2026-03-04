#ifndef XDMA_HW_H /* prevent circular inclusions */
#define XDMA_HW_H /* by using protection macros */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __KERNEL__
#include <asm/io.h>
#include <linux/types.h>
#else
typedef unsigned char      u8;
typedef unsigned short     u16;
typedef unsigned int       u32;
typedef unsigned long long u64;
#endif

/* NWL DMA design is little-endian, so values need not be swapped.
 */
#define XIo_In32(addr)                                              \
	({                                                              \
		u32 xx;                                                     \
		TRACE(TLVL_DEBUG + 40, "read: Start of %p", (void*)(addr)); \
		xx = readl((unsigned int*)(addr));                          \
		TRACE(TLVL_DEBUG + 41, "read: 0x%x=%p", xx, (void*)(addr)); \
		xx;                                                         \
	})

#define XIo_Out32(addr, data)                                         \
	do                                                                \
	{                                                                 \
		TRACE(TLVL_DEBUG + 42, "write %p=0x%x", (void*)(addr), data); \
		writel((data), (unsigned int*)(addr));                        \
		TRACE(TLVL_DEBUG + 43, "write done");                         \
	} while(0)

// #define MAX_DMA_ENGINES         64      /**< Maximum number of DMA engines */
#define MAX_DMA_ENGINES 64 /* FNAL DEVEL */

/** Engine bitmask is 64-bit because there are 64 engines */
#define DMA_ENGINE_PER_SIZE 0x100 /**< Separation between engine regs */
#define DMA_OFFSET          0     /**< Starting register offset */
                                  /**< Size of DMA engine reg space */
#define DMA_SIZE (MAX_DMA_ENGINES * DMA_ENGINE_PER_SIZE)

#pragma pack(push, 1)
/// <summary>
/// Server to Card Buffer Descriptor
/// </summary>
typedef struct
{
	u32 ByteCount : 20;   ///< Size of the Descriptor, as probided by the operating system
	u32 Rsvd : 4;         ///< Reserved bits
	u32 Complete : 1;     ///< Set when the descriptor completes without error
	u32 Short : 1;        ///< Set when the descriptor completed with a byte count less than the requested byte count
	u32 b26 : 1;          ///< Reserved bit
	u32 b27 : 1;          ///< Reserved bit
	u32 Error : 1;        ///< Set when the descriptor completes due to an error
	u32 b29 : 1;          ///< Reserved bit
	u32 b30 : 1;          ///< Reserved bit
	u32 b31 : 1;          ///< Reserved bit
	u64 UserControl;      ///< Contains application-specific control information
	u32 CardAddress;      ///< Card starting address for this descriptor in card memory
	u32 ByteCnt : 20;     ///< Control and Status: Control: Byte count for descriptor, status: Byte count transferred by DMA
	                      ///< engine
	u32 Rsvd_ : 4;        ///< Reserved bits
	u32 IrqComplete : 1;  ///< Generate an interrupt when this descriptor completes without error
	u32 IrqError : 1;     ///< Generate an interrupt when this descriptor completes with error
	u32 b26_ : 1;         ///< Reserved bit
	u32 b27_ : 1;         ///< Reserved bit
	u32 b28_ : 1;         ///< Reserved bit
	u32 b29_ : 1;         ///< Reserved bit
	u32 StartOfPkt : 1;   ///< Set if this descriptor contains the start of a packet
	u32 EndOfPkt : 1;     ///< Set if this descriptor contains the end of a packet
	u64 SystemAddress;    ///< System starting address for trhis descriptor
	u32 NextDescPtr;      ///< 32-byte address aligned pointer to the next descriptor in the chain, or 0x0 if this is the last
	                      ///< descriptor
} mu2e_buffdesc_S2C_t;

/// <summary>
/// Card to Server Buffer descriptor
/// </summary>
typedef struct
{
	u32 ByteCount : 20;    ///< Size of the Descriptor, as probided by the operating system
	u32 Rsvd : 4;          ///< Reserved bits
	u32 Complete : 1;      ///< Set when the descriptor completes without an error
	u32 Short : 1;         ///< Set when the descriptor completed with a byte count less than the requested byte count
	u32 Lo0 : 1;           ///< Set if C2SDescUserStatus[31:0]==0
	u32 Hi0 : 1;           ///< Set if C2SDescUserStatus[63:32]==0
	u32 Error : 1;         ///< Set when the descriptor completes due to an error
	u32 b29 : 1;           ///< Reserved bit
	u32 b30 : 1;           ///< Reserved bit
	u32 b31 : 1;           ///< Reserved bit
	u64 UserStatus;        ///< Contains application-specific status information
	u32 CardAddress;       ///< Card starting address for this descriptor in card memory
	u32 RsvdByteCnt : 20;  ///< The number of bytes the DMA engine wrote into the Descriptor
	u32 Rsvd_ : 4;         ///< Reserved bits
	u32 IrqComplete : 1;   ///< Flag to indicate that the IRQ has been serviced
	u32 IrqError : 1;      ///< Flag to indicate that there was an IRQ error
	u32 b26_ : 1;          ///< Reserved bit
	u32 b27_ : 1;          ///< Reserved bit
	u32 b28_ : 1;          ///< Reserved bit
	u32 b29_ : 1;          ///< Reserved bit
	u32 b30_ : 1;          ///< Reserved bit
	u32 b31_ : 1;          ///< Reserved bit
	u64 SystemAddress;     ///< System starting address for trhis descriptor
	u32 NextDescPtr;       ///< 32-byte address aligned pointer to the next descriptor in the chain, or 0x0 if this is the last
	                       ///< descriptor
} mu2e_buffdesc_C2S_t;
#pragma pack(pop)

#ifdef __KERNEL__
/*                                    [ch][dir]    */
static unsigned long mu2e_ch_reg_offset[2][2] = {{0x2000, 0x0}, {0x2100, 0x100}};

#define Dma_mReadChnReg(dtc, chn, dir, reg) \
	Dma_mReadReg((unsigned long)mu2e_pcie_bar_info[dtc].baseVAddr + mu2e_ch_reg_offset[chn][dir], reg)

#define Dma_mWriteChnReg(dtc, chn, dir, reg, val) \
	Dma_mWriteReg((unsigned long)mu2e_pcie_bar_info[dtc].baseVAddr + mu2e_ch_reg_offset[chn][dir], reg, val)
#endif

#define descDmaAdr2idx(regval, dtc, chn, dir, hnt)                                                                                                                                                                         \
	(dir == C2S) ? ({                                                                                                                                                                                                      \
		u32 ii = hnt % MU2E_NUM_RECV_BUFFS, lc = 0;                                                                                                                                                                        \
		do                                                                                                                                                                                                                 \
		{                                                                                                                                                                                                                  \
			TRACE(TLVL_DEBUG + 44, "descDmaAdr2idx: regval=%x buffdesc_ring_dma[%d/%d]=%x, dtc=%d, chn=%d, dir=%d", regval, ii, MU2E_NUM_RECV_BUFFS, (u32)mu2e_pci_recver[dtc][chn].buffdesc_ring_dma[ii], dtc, chn, dir); \
			if(regval == mu2e_pci_recver[dtc][chn].buffdesc_ring_dma[ii])                                                                                                                                                  \
				break;                                                                                                                                                                                                     \
			ii = (ii + 1) % MU2E_NUM_RECV_BUFFS;                                                                                                                                                                           \
			++lc;                                                                                                                                                                                                          \
		} while(lc < MU2E_NUM_RECV_BUFFS);                                                                                                                                                                                 \
		ii;                                                                                                                                                                                                                \
	})                                                                                                                                                                                                                     \
	             : ((u32)(regval - mu2e_pci_sender[dtc][chn].buffdesc_ring_dma) / (u32)sizeof(mu2e_buffdesc_S2C_t))

#define idx2descDmaAdr(idx, dtc, chn, dir)                                 \
	(dir == C2S) ? ((u32)mu2e_pci_recver[dtc][chn].buffdesc_ring_dma[idx]) \
	             : ((u32)mu2e_pci_sender[dtc][chn].buffdesc_ring_dma + (u32)sizeof(mu2e_buffdesc_S2C_t) * idx)

#define idx2descVirtAdr(idx, dtc, chn, dir)                                      \
	((dir == C2S) ? (void*)((ulong)mu2e_pci_recver[dtc][chn].buffdesc_ring[idx]) \
	              : (void*)((ulong)mu2e_pci_sender[dtc][chn].buffdesc_ring + sizeof(mu2e_buffdesc_S2C_t) * idx))

/** @name Device register offset definitions. Register access is 32-bit.
 *  @{
 */

/* Common DMA registers */
#define REG_DMA_CTRL_STATUS 0x4000 /**< DMA Common Ctrl & Status */

/* These engine registers are applicable to both S2C and C2S channels.
 * Register field mask and shift definitions are later in this file.
 */

#define REG_DMA_ENG_CAP         0x00000000 /**< DMA Engine Capabilities */
#define REG_DMA_ENG_CTRL_STATUS 0x00000004 /**< DMA Engine Control */
#define REG_HW_NEXT_BD          0x00000008 /**< HW Next desc pointer */
#define REG_SW_NEXT_BD          0x0000000C /**< SW Next desc pointer */
#define REG_HW_CMPLT_BD         0x00000010 /**< HW Last completed pointer */
#define REG_DMA_ENG_ACTIVE_TIME 0x00000014 /**< DMA Engine Active Time */
#define REG_DMA_ENG_WAIT_TIME   0x00000018 /**< DMA Engine Wait Time */
#define REG_DMA_ENG_COMP_BYTES  0x0000001C /**< DMA Engine Completed Bytes */

/*@}*/

/* Register masks. The following constants define bit locations of various
 * control bits in the registers. For further information on the meaning of
 * the various bit masks, refer to the hardware spec.
 *
 * Masks have been written assuming HW bits 0-31 correspond to SW bits 0-31
 */

/** @name Bitmasks of REG_DMA_CTRL_STATUS register.
 * @{
 */
#define DMA_INT_ENABLE           0x00000001 /**< Enable global interrupts */
#define DMA_INT_DISABLE          0x00000000 /**< Disable interrupts */
#define DMA_INT_ACTIVE_MASK      0x00000002 /**< Interrupt active? */
#define DMA_INT_PENDING_MASK     0x00000004 /**< Engine interrupt pending */
#define DMA_INT_MSI_MODE         0x00000008 /**< MSI or Legacy mode? */
#define DMA_USER_INT_ENABLE      0x00000010 /**< Enable user interrupts */
#define DMA_USER_INT_ACTIVE_MASK 0x00000020 /**< Int - user interrupt */
#define DMA_USER_INT_ACK         0x00000020 /**< Acknowledge */
#define DMA_MPS_USED             0x00000700 /**< MPS Used */
#define DMA_MRRS_USED            0x00007000 /**< MRRS Used */
#define DMA_S2C_ENG_INT_VAL      0x00FF0000 /**< IRQ value of 1st 8 S2Cs */
#define DMA_C2S_ENG_INT_VAL      0xFF000000 /**< IRQ value of 1st 8 C2Ss */

/** @name Bitmasks of REG_DMA_ENG_CAP register.
 * @{
 */
/* DMA engine characteristics */
#define DMA_ENG_PRESENT_MASK   0x00000001 /**< DMA engine present? */
#define DMA_ENG_DIRECTION_MASK 0x00000002 /**< DMA engine direction */
#define DMA_ENG_C2S            0x00000002 /**< DMA engine - C2S */
#define DMA_ENG_S2C            0x00000000 /**< DMA engine - S2C */
#define DMA_ENG_TYPE_MASK      0x00000010 /**< DMA engine type */
#define DMA_ENG_BLOCK          0x00000000 /**< DMA engine - Block type */
#define DMA_ENG_PACKET         0x00000010 /**< DMA engine - Packet type */
#define DMA_ENG_NUMBER         0x0000FF00 /**< DMA engine number */
#define DMA_ENG_BD_MAX_BC      0x3F000000 /**< DMA engine max buffer size */

/* Shift constants for selected masks */
#define DMA_ENG_NUMBER_SHIFT    8
#define DMA_ENG_BD_MAX_BC_SHIFT 24

/*@}*/

/** @name Bitmasks of REG_DMA_ENG_CTRL_STATUS register.
 * @{
 */
/* Interrupt activity and acknowledgement bits */
#define DMA_ENG_INT_ENABLE       0x00000001 /**< Enable interrupts */
#define DMA_ENG_INT_DISABLE      0x00000000 /**< Disable interrupts */
#define DMA_ENG_INT_ACTIVE_MASK  0x00000002 /**< Interrupt active? */
#define DMA_ENG_INT_ACK          0x00000002 /**< Interrupt ack */
#define DMA_ENG_INT_BDCOMP       0x00000004 /**< Int - BD completion */
#define DMA_ENG_INT_BDCOMP_ACK   0x00000004 /**< Acknowledge */
#define DMA_ENG_INT_ALERR        0x00000008 /**< Int - BD align error */
#define DMA_ENG_INT_ALERR_ACK    0x00000008 /**< Acknowledge */
#define DMA_ENG_INT_FETERR       0x00000010 /**< Int - BD fetch error */
#define DMA_ENG_INT_FETERR_ACK   0x00000010 /**< Acknowledge */
#define DMA_ENG_INT_ABORTERR     0x00000020 /**< Int - DMA abort error */
#define DMA_ENG_INT_ABORTERR_ACK 0x00000020 /**< Acknowledge */
#define DMA_ENG_INT_CHAINEND     0x00000080 /**< Int - BD chain ended */
#define DMA_ENG_INT_CHAINEND_ACK 0x00000080 /**< Acknowledge */

/* DMA engine control */
#define DMA_ENG_ENABLE_MASK  0x00000100 /**< DMA enabled? */
#define DMA_ENG_ENABLE       0x00000100 /**< Enable DMA */
#define DMA_ENG_DISABLE      0x00000000 /**< Disable DMA */
#define DMA_ENG_STATE_MASK   0x00000C00 /**< Current DMA state? */
#define DMA_ENG_RUNNING      0x00000400 /**< DMA running */
#define DMA_ENG_IDLE         0x00000000 /**< DMA idle */
#define DMA_ENG_WAITING      0x00000800 /**< DMA waiting */
#define DMA_ENG_STATE_WAITED 0x00001000 /**< DMA waited earlier */
#define DMA_ENG_WAITED_ACK   0x00001000 /**< Acknowledge */
#define DMA_ENG_USER_RESET   0x00004000 /**< Reset only user logic */
#define DMA_ENG_RESET        0x00008000 /**< Reset DMA engine + user */

#define DMA_ENG_ALLINT_MASK 0x000000BE /**< To get only int events */

/*@}*/

/** @name Bitmasks of performance registers.
 * @{
 */

#define REG_DMA_SAMPLE_CTR_MASK 0x00000003 /**< Sync counter for regs */

/* Shift constants for performance registers */
#define REG_DMA_ENG_ACTIVE_TIME_SHIFT 2
#define REG_DMA_ENG_WAIT_TIME_SHIFT   2
#define REG_DMA_ENG_COMP_BYTES_SHIFT  2

/*@}*/

/**************************** Type Definitions *******************************/

/*****************************************************************************/
/**
 *
 * Read the given register.
 *
 * @param    BaseAddress is the base address of the device
 * @param    RegOffset is the register offset to be read
 *
 * @return   The 32-bit value of the register
 *
 * @note
 * C-style signature:
 *    u32 Dma_mReadReg(u32 BaseAddress, u32 RegOffset)
 *
 ******************************************************************************/
#define Dma_mReadReg(BaseAddress, RegOffset) XIo_In32((BaseAddress) + (RegOffset))

/*****************************************************************************/
/**
 *
 * Write the given register.
 *
 * @param    BaseAddress is the base address of the device
 * @param    RegOffset is the register offset to be written
 * @param    Data is the 32-bit value to write to the register
 *
 * @return   None.
 *
 * @note
 * C-style signature:
 *    void Dma_mWriteReg(u32 BaseAddress, u32 RegOffset, u32 Data)
 *
 ******************************************************************************/
#define Dma_mWriteReg(BaseAddress, RegOffset, Data) XIo_Out32((BaseAddress) + (RegOffset), (Data))

/****************************************************************************/
/**
 * Enable global interrupt bits. This operation will read-modify-write
 * the REG_DMA_CTRL_STATUS register.
 *
 * @param  BaseAddress is the BAR0 address.
 *
 * @note
 * C-style signature:
 *    void Dma_mIntEnable(u32 BaseAddress)
 *
 *****************************************************************************/
#define Dma_mIntEnable(BaseAddress)                                                                                                                                                        \
	{                                                                                                                                                                                      \
		u32 Reg = Dma_mReadReg(BaseAddress, REG_DMA_CTRL_STATUS);                                                                                                                          \
		Reg |= (DMA_INT_ENABLE | DMA_USER_INT_ENABLE);                                                                                                                                     \
		TRACE(TLVL_DEBUG + 46, "xdma.h macro Dma_mIntEnable(Base=%p): OR'd in 0x%x, writing REG_DMA_CTRL_STATUS 0x%08x", (void*)BaseAddress, (DMA_INT_ENABLE | DMA_USER_INT_ENABLE), Reg); \
		Dma_mWriteReg(BaseAddress, REG_DMA_CTRL_STATUS, Reg);                                                                                                                              \
	}

/****************************************************************************
 * Clear global interrupt enable bits. This operation will read-modify-write
 * the REG_DMA_CTRL_STATUS register.
 *
 * @param  BaseAddress is the BAR0 address.
 *
 * @note
 * C-style signature:
 *    void Dma_mIntDisable(u32 BaseAddress)
 *****************************************************************************/
#define Dma_mIntDisable(BaseAddress)                                                                                                                                                           \
	do                                                                                                                                                                                         \
	{                                                                                                                                                                                          \
		u32 Reg = Dma_mReadReg(BaseAddress, REG_DMA_CTRL_STATUS);                                                                                                                              \
		Reg &= ~(DMA_INT_ENABLE | DMA_USER_INT_ENABLE);                                                                                                                                        \
		TRACE(TLVL_DEBUG + 47, "xdma.h macro Dma_mIntDisable(Base=%p): masked off 0x%x, writing REG_DMA_CTRL_STATUS 0x%08x", (void*)BaseAddress, (DMA_INT_ENABLE | DMA_USER_INT_ENABLE), Reg); \
		Dma_mWriteReg(BaseAddress, REG_DMA_CTRL_STATUS, Reg);                                                                                                                                  \
	} while(0)

/****************************************************************************
 * Acknowledge asserted global interrupts.
 *
 * @param  BaseAddress is the base address of the device
 * @param  Mask has the interrupt signals to be acknowledged and is made
 *         by the caller OR'ing one or more of the INT_*_ACK bits.
 *
 * @note
 * C-style signature:
 *    u32 Dma_mIntAck(u32 BaseAddress, u32 Mask)
 *****************************************************************************/
/* Currently implemented like this. May have a performance hit. In
 * that case, will re-implement to avoid the extra read. !!!!
 */
#define Dma_mIntAck(BaseAddress, Mask)                                                                                                               \
	{                                                                                                                                                \
		u32 Reg0 = Dma_mReadReg(BaseAddress, REG_DMA_CTRL_STATUS);                                                                                   \
		u32 Reg1 = Reg0 | Mask;                                                                                                                      \
		TRACE(TLVL_DEBUG + 45, "xdma.h macro Dma_mIntAck(Base=%p,Mask=0x%08x): writing REG_DMA_CTRL_STATUS 0x%08x", (void*)BaseAddress, Mask, Reg1); \
		Dma_mWriteReg(BaseAddress, REG_DMA_CTRL_STATUS, Reg1);                                                                                       \
	}

/****************************************************************************/
/**
 * Set DMA enable bit for an instance. This operation will read-modify-write
 * the REG_DMA_ENG_CTRL_STATUS register.
 *
 * @param  BaseAddress is the base address of the device
 *
 * @note
 * C-style signature:
 *    void Dma_mEnable(u32 BaseAddress)
 *
 *****************************************************************************/
#define Dma_mEnable(BaseAddress)                                      \
	{                                                                 \
		u32 val = Dma_mReadReg(BaseAddress, REG_DMA_ENG_CTRL_STATUS); \
		val |= DMA_ENG_ENABLE;                                        \
		Dma_mWriteReg(BaseAddress, REG_DMA_ENG_CTRL_STATUS, val);     \
	}

#ifdef __cplusplus
}
#endif

#endif /* end of protection macro */
