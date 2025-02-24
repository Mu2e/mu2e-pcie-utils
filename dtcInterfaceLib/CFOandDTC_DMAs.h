#ifndef CFO_AND_DTC_DMAS_H
#define CFO_AND_DTC_DMAS_H

#include <list>
#include <memory>
#include <vector>

// #include "artdaq-core-mu2e/Overlays/DTC_Packets.h"
#include "artdaq-core-mu2e/Overlays/DTC_Packets/DTC_DataBlock.h"
#include "artdaq-core-mu2e/Overlays/DTC_Packets/DTC_DataHeaderPacket.h"
#include "artdaq-core-mu2e/Overlays/DTC_Packets/DTC_DataPacket.h"
#include "artdaq-core-mu2e/Overlays/DTC_Packets/DTC_DataRequestPacket.h"
#include "artdaq-core-mu2e/Overlays/DTC_Packets/DTC_DataStatus.h"
#include "artdaq-core-mu2e/Overlays/DTC_Packets/DTC_DCSReplyPacket.h"
#include "artdaq-core-mu2e/Overlays/DTC_Packets/DTC_DCSRequestPacket.h"
#include "artdaq-core-mu2e/Overlays/DTC_Packets/DTC_DMAPacket.h"
#include "artdaq-core-mu2e/Overlays/DTC_Packets/DTC_Event.h"
#include "artdaq-core-mu2e/Overlays/DTC_Packets/DTC_EventHeader.h"
#include "artdaq-core-mu2e/Overlays/DTC_Packets/DTC_HeartbeatPacket.h"
#include "artdaq-core-mu2e/Overlays/DTC_Packets/DTC_PacketType.h"
#include "artdaq-core-mu2e/Overlays/DTC_Packets/DTC_SubEvent.h"
#include "artdaq-core-mu2e/Overlays/DTC_Packets/DTC_SubEventHeader.h"

#include "DTC_Registers.h"

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

namespace DTCLib {

/// <summary>
/// This class implements common DMA functionality for the CFO and DTC
/// </summary>
class CFOandDTC_DMAs
{
public:
	/// <summary>

	/// <summary>
	/// Release all buffers to the hardware on the given channel
	/// </summary>
	/// <param name="channel">Channel to release</param>
	// void ReleaseAllBuffers(const DTC_DMA_Engine& channel);

	// int ReadBuffer(const DTC_DMA_Engine& channel, int retries = 10);
	// /// <summary>
	// /// This function releases all buffers except for the one containing currentReadPtr. Should only be called when done
	// /// with data in other buffers!
	// /// </summary>
	// /// <param name="channel">Channel to release</param>
	// void ReleaseBuffers(const DTC_DMA_Engine& channel);
	// void WriteDataPacket(const DTC_DataPacket& packet);

	struct DMAInfo
	{
		std::deque<mu2e_databuff_t*> buffer;
		uint32_t bufferIndex;
		void* currentReadPtr;
		void* lastReadPtr;
		DMAInfo()
			: buffer(), bufferIndex(0), currentReadPtr(nullptr), lastReadPtr(nullptr) {}
		~DMAInfo()
		{
			buffer.clear();
			currentReadPtr = nullptr;
			lastReadPtr = nullptr;
		}
	};
	static int GetCurrentBuffer(DMAInfo* info);
	static uint16_t GetBufferByteCount(DMAInfo* info, size_t index);
};
}  // namespace DTCLib
#endif  // end CFO_AND_DTC_DMAS_H
