#ifndef CFO_H
#define CFO_H

#include <list>
#include <memory>
#include <vector>

// #include "artdaq-core-mu2e/Overlays/CFO_Packets.h"
#include "artdaq-core-mu2e/Overlays/CFO_Packets/CFO_DataPacket.h"
#include "artdaq-core-mu2e/Overlays/CFO_Packets/CFO_DMAPacket.h"
#include "artdaq-core-mu2e/Overlays/CFO_Packets/CFO_Event.h"
#include "artdaq-core-mu2e/Overlays/CFO_Packets/CFO_EventRecord.h"
#include "artdaq-core-mu2e/Overlays/CFO_Packets/CFO_PacketType.h"

#include "CFO_Registers.h"
#include "dtcInterfaceLib/CFOandDTC_DMAs.h"

// #include "artdaq-core-mu2e/Overlays/DTC_Types.h"
#include "artdaq-core-mu2e/Overlays/DTC_Types/DTC_CharacterNotInTableError.h"
#include "artdaq-core-mu2e/Overlays/DTC_Types/DTC_DCSOperationType.h"
#include "artdaq-core-mu2e/Overlays/DTC_Types/DTC_DDRFlags.h"
#include "artdaq-core-mu2e/Overlays/DTC_Types/DTC_DebugType.h"
#include "artdaq-core-mu2e/Overlays/DTC_Types/DTC_EVBStatus.h"
#include "artdaq-core-mu2e/Overlays/DTC_Types/DTC_EventMode.h"
#include "artdaq-core-mu2e/Overlays/DTC_Types/DTC_EventWindowTag.h"
#include "artdaq-core-mu2e/Overlays/DTC_Types/DTC_FIFOFullErrorFlags.h"
#include "artdaq-core-mu2e/Overlays/DTC_Types/DTC_IICDDRBusAddress.h"
#include "artdaq-core-mu2e/Overlays/DTC_Types/DTC_IICSERDESBusAddress.h"
#include "artdaq-core-mu2e/Overlays/DTC_Types/DTC_LinkEnableMode.h"
#include "artdaq-core-mu2e/Overlays/DTC_Types/DTC_LinkStatus.h"
#include "artdaq-core-mu2e/Overlays/DTC_Types/DTC_Link_ID.h"
#include "artdaq-core-mu2e/Overlays/DTC_Types/DTC_OscillatorType.h"
#include "artdaq-core-mu2e/Overlays/DTC_Types/DTC_PLL_ID.h"
#include "artdaq-core-mu2e/Overlays/DTC_Types/DTC_PRBSMode.h"
#include "artdaq-core-mu2e/Overlays/DTC_Types/DTC_ROC_Emulation_Type.h"
#include "artdaq-core-mu2e/Overlays/DTC_Types/DTC_RXBufferStatus.h"
#include "artdaq-core-mu2e/Overlays/DTC_Types/DTC_RXStatus.h"
#include "artdaq-core-mu2e/Overlays/DTC_Types/DTC_SerdesClockSpeed.h"
#include "artdaq-core-mu2e/Overlays/DTC_Types/DTC_SERDESLoopbackMode.h"
#include "artdaq-core-mu2e/Overlays/DTC_Types/DTC_SERDESRXDisparityError.h"
#include "artdaq-core-mu2e/Overlays/DTC_Types/DTC_SimMode.h"
#include "artdaq-core-mu2e/Overlays/DTC_Types/DTC_Subsystem.h"
#include "artdaq-core-mu2e/Overlays/DTC_Types/Exceptions.h"
#include "artdaq-core-mu2e/Overlays/DTC_Types/Utilities.h"

using namespace DTCLib;

namespace CFOLib {
/// <summary>
/// The CFO class implements the data transfers to the CFO card. It derives from CFO_Registers, the class representing
/// the CFO register space.
/// </summary>
class CFO : public CFO_Registers
{
public:
	/// <summary>
	/// Construct an instance of the CFO class
	/// </summary>
	explicit CFO(DTC_SimMode mode = DTC_SimMode_Disabled, int cfo = -1,
				 std::string expectedDesignVersion = "", bool skipInit = false, const std::string& uid = "");
	virtual ~CFO();

	// //
	// // DMA Functions
	// //
	// // Data read-out
	// /// <summary>
	// /// Reads data from the DTC, and returns all data blocks with the same event window tag. If event window tag is specified, will look
	// /// for data with that event window tag.
	// /// </summary>
	// /// <param name="when">Desired event window tag for readout. Default means use whatever event window tag is next</param>
	// /// <returns>A vector of DTC_Event objects</returns>
	bool GetData(std::vector<std::unique_ptr<CFO_Event>>& output, DTC_EventWindowTag when = DTC_EventWindowTag(), bool matchEventWindowTag = false);

	/**
	 * @brief Read the next DMA from the DAQ channel. If no data is present, will return nullptr
	 * @param tmo_ms Timeout
	 * @return A CFO_Event representing the data in a single DMA, or nullptr if no data/timeout
	 */
	bool ReadNextCFORecordDMA(std::vector<std::unique_ptr<CFO_Event>>& output, int tmo_ms);

	/// <summary>
	/// Release all buffers to the hardware on the given channel
	/// </summary>
	/// <param name="channel">Channel to release</param>
	void ReleaseAllBuffers(const DTC_DMA_Engine& channel);

private:
	std::unique_ptr<CFO_DataPacket> ReadNextPacket(const DTC_DMA_Engine& channel, int tmo_ms);
	int ReadBuffer(const DTC_DMA_Engine& channel, int tmo_ms);
	// /// <summary>
	// /// This function releases all buffers except for the one containing currentReadPtr. Should only be called when done
	// /// with data in other buffers!
	// /// </summary>
	// /// <param name="channel">Channel to release</param>
	void ReleaseBuffers(const DTC_DMA_Engine& channel);
	// void WriteDataPacket(const DTC_DataPacket& packet, bool alreadyHaveDCSTransactionLock);

	// struct DMAInfo
	// {
	// 	std::deque<mu2e_databuff_t*> buffer;
	// 	uint32_t bufferIndex;
	// 	void* currentReadPtr;
	// 	void* lastReadPtr;
	// 	DMAInfo()
	// 		: buffer(), bufferIndex(0), currentReadPtr(nullptr), lastReadPtr(nullptr) {}
	// 	~DMAInfo()
	// 	{
	// 		buffer.clear();
	// 		currentReadPtr = nullptr;
	// 		lastReadPtr = nullptr;
	// 	}
	// };
	// int GetCurrentBuffer(DMAInfo* info);
	// uint16_t GetBufferByteCount(DMAInfo* info, size_t index);
	CFOandDTC_DMAs::DMAInfo daqDMAInfo_;
	// DMAInfo dcsDMAInfo_;

	// uint8_t lastDTCErrorBitsValue_ = 0;
};
}  // namespace CFOLib
#endif
