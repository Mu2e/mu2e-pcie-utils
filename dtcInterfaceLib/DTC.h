#ifndef DTC_H
#define DTC_H

#include <list>
#include <memory>
#include <vector>

// #include "artdaq-core-mu2e/Overlays/DTC_Packets.h"
#include "artdaq-core-mu2e/Overlays/DTC_Packets/DTC_DCSReplyPacket.h"
#include "artdaq-core-mu2e/Overlays/DTC_Packets/DTC_DCSRequestPacket.h"
#include "artdaq-core-mu2e/Overlays/DTC_Packets/DTC_DMAPacket.h"
#include "artdaq-core-mu2e/Overlays/DTC_Packets/DTC_DataBlock.h"
#include "artdaq-core-mu2e/Overlays/DTC_Packets/DTC_DataHeaderPacket.h"
#include "artdaq-core-mu2e/Overlays/DTC_Packets/DTC_DataPacket.h"
#include "artdaq-core-mu2e/Overlays/DTC_Packets/DTC_DataRequestPacket.h"
#include "artdaq-core-mu2e/Overlays/DTC_Packets/DTC_DataStatus.h"
#include "artdaq-core-mu2e/Overlays/DTC_Packets/DTC_Event.h"
#include "artdaq-core-mu2e/Overlays/DTC_Packets/DTC_EventHeader.h"
#include "artdaq-core-mu2e/Overlays/DTC_Packets/DTC_HeartbeatPacket.h"
#include "artdaq-core-mu2e/Overlays/DTC_Packets/DTC_PacketType.h"
#include "artdaq-core-mu2e/Overlays/DTC_Packets/DTC_SubEvent.h"
#include "artdaq-core-mu2e/Overlays/DTC_Packets/DTC_SubEventHeader.h"

#include "CFOandDTC_DMAs.h"
#include "DTC_Registers.h"

// #include "artdaq-core-mu2e/Overlays/DTC_Types.h"
#include "artdaq-core-mu2e/Overlays/DTC_Types/DTC_CharacterNotInTableError.h"
#include "artdaq-core-mu2e/Overlays/DTC_Types/DTC_DCSOperationType.h"
#include "artdaq-core-mu2e/Overlays/DTC_Types/DTC_DDRFlags.h"
#include "artdaq-core-mu2e/Overlays/DTC_Types/DTC_DebugType.h"
#include "artdaq-core-mu2e/Overlays/DTC_Types/DTC_EVBStatsType.h"
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
#include "artdaq-core-mu2e/Overlays/DTC_Types/DTC_SERDESLoopbackMode.h"
#include "artdaq-core-mu2e/Overlays/DTC_Types/DTC_SERDESRXDisparityError.h"
#include "artdaq-core-mu2e/Overlays/DTC_Types/DTC_SerdesClockSpeed.h"
#include "artdaq-core-mu2e/Overlays/DTC_Types/DTC_SimMode.h"
#include "artdaq-core-mu2e/Overlays/DTC_Types/DTC_Subsystem.h"
#include "artdaq-core-mu2e/Overlays/DTC_Types/Exceptions.h"
#include "artdaq-core-mu2e/Overlays/DTC_Types/Utilities.h"

namespace DTCLib
{

typedef uint16_t roc_data_t;
typedef uint16_t roc_address_t;

/// <summary>
/// The DTC class implements the data transfers to the DTC card. It derives from DTC_Registers, the class representing
/// the DTC register space.
/// </summary>
class DTC : public DTC_Registers
{
  public:
	/// <summary>
	/// Construct an instance of the DTC class
	/// </summary>
	/// <param name="mode">The desired simulation mode for the DTC (Default: Disabled)</param>
	/// <param name="dtc">The DTC card to use (default: -1: Use environment variable or 0 if env. var. unset)</param>
	/// <param name="rocMask">Which ROCs should be active. Each hex digit corresponds to a Link, and the number indicates
	/// how many ROCs are active on that link. (Default: 0x1)</param> <param name="expectedDesignVersion">Expected DTC
	/// Firmware Design Version. If set, will throw an exception if the DTC firmware does not match (Default: "")</param>
	/// <param name="skipInit">Whether to skip full initialization of the DTC</param>
	/// <param name="simMemoryFile">Name of the simulated DDR memory file if mu2esim is used</param>
	explicit DTC(DTC_SimMode mode = DTC_SimMode_Disabled, int dtc = -1, unsigned rocMask = 0x1, std::string expectedDesignVersion = "", bool skipInit = false, std::string simMemoryFile = "mu2esim.bin", const std::string& uid = "");
	virtual ~DTC();

	//
	// DMA Functions
	//
	// Data read-out
	/// <summary>
	/// Reads data from the DTC, and returns all data blocks with the same event window tag. If event window tag is specified, will look
	/// for data with that event window tag.
	/// </summary>
	/// <param name="when">Desired event window tag for readout. Default means use whatever event window tag is next</param>
	/// <returns>A vector of DTC_Event objects, but only one DTC_Event is expected</returns>
	std::vector<std::unique_ptr<DTC_Event>> GetData(DTC_EventWindowTag when = DTC_EventWindowTag(), bool matchEventWindowTag = false);

	/// <summary>
	/// Reads data from the DTC, and returns the first Sub Event found. If event window tag is specified, will look
	/// to match the found Sub Event with the specified event window tag.
	/// </summary>
	/// <param name="when">Desired event window tag for readout. Default means use whatever event window tag is next</param>
	/// <returns>A vector of DTC_SubEvent objects, but only one DTC_SubEvent is expected</returns>
	std::vector<std::unique_ptr<DTC_SubEvent>> GetSubEventData(DTC_EventWindowTag when = DTC_EventWindowTag(), bool matchEventWindowTag = false);
	std::vector<std::unique_ptr<DTC_SubEvent>> GetSubEventData2(DTC_EventWindowTag when = DTC_EventWindowTag(), bool matchEventWindowTag = false);


	/// <summary>
	/// Read a file into the DTC memory. Will truncate the file so that it fits in the DTC memory.
	/// </summary>
	/// <param name="file">File name to read into DTC memory</param>
	/// <param name="goForever">Whether readout should loop through the file</param>
	/// <param name="overwriteEnvrionment">Whether to use file instead of DTCLIB_SIM_FILE</param>
	/// <param name="outputFileName">Name of binary file to write expected output (Default: "", no file created)</param>
	/// <param name="skipVerify">Skip the verify stage of WriteSimFileToDTC</param>
	void WriteSimFileToDTC(std::string file, bool goForever, bool overwriteEnvrionment = false, std::string outputFileName = "", bool skipVerify = false);
	/// <summary>
	/// Read the DTC memory and determine whether the file was written correctly.
	/// </summary>
	/// <param name="file">File to verify against</param>
	/// <param name="rawOutputFilename">Default: "". If set, file to write data retrieved from the DTC to</param>
	/// <returns>True if file is in DTC memory without errors</returns>
	bool VerifySimFileInDTC(std::string file, std::string rawOutputFilename = "");

	// DCS Register R/W
	/// <summary>
	/// Sends a DCS Request Packet with fields filled in such that the given ROC register will be read out.
	/// This function reads from the main ROC register space, use ReadExtROCRegister to access other firmware blocks'
	/// register spaces.
	/// </summary>
	/// <param name="link">Link of the ROC to read</param>
	/// <param name="address">Address of the register</param>
	/// <param name="tmo_ms">Timeout, in milliseconds, for read (will retry until timeout is expired or data received)</param>
	/// <returns>Value of the ROC register from a DCS Reply packet</returns>
	roc_data_t ReadROCRegister(const DTC_Link_ID& link, const roc_address_t address, int tmo_ms);
	/// <summary>
	/// Sends a DCS Request Packet with the fields filled in such that the given ROC register will be written.
	/// This function writes to the main ROC register space, use WriteExtROCRegister to access other firmware blocks'
	/// register spaces.
	/// </summary>
	/// <param name="link">Link of the ROC to write to</param>
	/// <param name="address">Address of the register</param>
	/// <param name="data">Value to write</param>
	/// <param name="requestAck">Whether to request acknowledement of this operation</param>
	/// <param name="ack_tmo_ms">Timeout, in milliseconds, for ack (will retry until timeout is expired or ack received)</param>
	bool WriteROCRegister(const DTC_Link_ID& link, const roc_address_t address, const roc_data_t data, bool requestAck, int ack_tmo_ms);

	/// <summary>
	/// Perform a "double operation" read of ROC registers
	/// </summary>
	/// <param name="link">Link of the ROC to read</param>
	/// <param name="address1">First address to read</param>
	/// <param name="address2">Second address to read</param>
	/// <param name="tmo_ms">Timeout, in milliseconds, for read (will retry until timeout is expired or data received)</param>
	/// <returns>Pair of register values, first is from the first address, second from the second</returns>
	std::pair<roc_data_t, roc_data_t> ReadROCRegisters(const DTC_Link_ID& link, const roc_address_t address1, const roc_address_t address2, int tmo_ms);
	/// <summary>
	/// Perform a "double operation" write to ROC registers
	/// </summary>
	/// <param name="link">Link of the ROC to write to</param>
	/// <param name="address1">First address to write</param>
	/// <param name="data1">Value to write to first register</param>
	/// <param name="address2">Second address to write</param>
	/// <param name="data2">Value to write to second register</param>
	/// <param name="requestAck">Whether to request acknowledement of this operation</param>
	/// <param name="ack_tmo_ms">Timeout, in milliseconds, for ack (will retry until timeout is expired or ack received)</param>
	bool WriteROCRegisters(const DTC_Link_ID& link, const roc_address_t address1, const roc_data_t data1, const roc_address_t address2, const roc_data_t data2, bool requestAck, int ack_tmo_ms);
	/// <summary>
	/// Perform a ROC block read
	/// </summary>
	/// <param name="data">vector of data by reference to read</param>
	/// <param name="link">Link of the ROC to read</param>
	/// <param name="address">Address of the block</param>
	/// <param name="wordCount">Number of words to read</param>
	/// <returns>Vector of words returned by block read</returns>
	/// <param name="incrementAddress">Whether to increment the address pointer for block reads/writes</param>
	/// <param name="tmo_ms">Timeout, in milliseconds, for read (will retry until timeout is expired or data received)</param>
	void ReadROCBlock(std::vector<roc_data_t>& data, const DTC_Link_ID& link, const roc_address_t address, const uint16_t wordCount, bool incrementAddress, int tmo_ms);
	/// <summary>
	/// Perform a ROC block write
	/// </summary>
	/// <param name="link">Link of the ROC to write</param>
	/// <param name="address">Address of the block</param>
	/// <param name="blockData">Vector of words to write</param>
	/// <param name="requestAck">Whether to request acknowledement of this operation</param>
	/// <param name="incrementAddress">Whether to increment the address pointer for block reads/writes</param>
	/// <param name="ack_tmo_ms">Timeout, in milliseconds, for ack (will retry until timeout is expired or ack received)</param>
	bool WriteROCBlock(const DTC_Link_ID& link, const roc_address_t address, const std::vector<roc_data_t>& blockData, bool requestAck, bool incrementAddress, int ack_tmo_ms);

	/// <summary>
	/// Sends a DCS Request Packet with fields filled in such that the given ROC firmware block register will be read out.
	/// This funcion reads from firmware blocks' register spaces.
	/// </summary>
	/// <param name="link">Link of the ROC to read</param>
	/// <param name="block">Block ID to read from</param>
	/// <param name="address">Address of the register</param>
	/// <param name="tmo_ms">Timeout, in milliseconds, for read (will retry until timeout is expired or data received)</param>
	/// <returns>Value of the ROC register from a DCS Reply packet</returns>
	uint16_t ReadExtROCRegister(const DTC_Link_ID& link, const roc_address_t block, const roc_address_t address, int tmo_ms = 0);
	/// <summary>
	/// Sends a DCS Request Packet with fields filled in such that the given ROC firmware block register will be written.
	/// This funcion writes to firmware blocks' register spaces.
	/// </summary>
	/// <param name="link">Link of the ROC to write to</param>
	/// <param name="block">Block ID to write to</param>
	/// <param name="address">Address of the register</param>
	/// <param name="data">Value to write</param>
	/// <param name="requestAck">Whether to request acknowledement of this operation</param>
	/// <param name="ack_tmo_ms">Timeout, in milliseconds, for ack (will retry until timeout is expired or ack received)</param>
	bool WriteExtROCRegister(const DTC_Link_ID& link, const roc_address_t block, const roc_address_t address, const roc_data_t data, bool requestAck, int ack_tmo_ms);
	/// <summary>
	/// Dump all known registers from the given ROC, via DCS Request packets.
	/// </summary>
	/// <param name="link">Link of the ROC</param>
	/// <returns>JSON-formatted register dump</returns>
	std::string ROCRegDump(const DTC_Link_ID& link);

	// Broadcast Readout
	/// <summary>
	/// DEPRECATED
	/// Sends a Readout Request broadcast to given Link.
	/// </summary>
	/// <param name="link">Link to send to</param>
	/// <param name="when">Timestamp for the Readout Request</param>
	/// <param name="quiet">Whether to not print the JSON representation of the Readout Request (Default: true, no JSON
	/// printed)</param>
	void SendHeartbeatPacket(const DTC_Link_ID& link, const DTC_EventWindowTag& when, bool quiet = true);
	/// <summary>
	/// Send a DCS Request Packet to the given ROC. Use the Read/Write ROC Register functions for more convinient register
	/// access.
	/// </summary>
	/// <param name="link">Link of the ROC</param>
	/// <param name="type">Operation to perform</param>
	/// <param name="address">Target address</param>
	/// <param name="data">Data to write, if operation is write</param>
	/// <param name="address2">Second Target address</param>
	/// <param name="data2">Data to write to second address, if operation is write</param>
	/// <param name="quiet">Whether to not print the JSON representation of the Readout Request (Default: true, no JSON
	/// printed)</param>
	/// <param name="requestAck">Whether to request acknowledement of this operation</param>
	void SendDCSRequestPacket(const DTC_Link_ID& link, const DTC_DCSOperationType type, const roc_address_t address, const roc_data_t data = 0x0, const roc_address_t address2 = 0x0, const roc_data_t data2 = 0, bool quiet = true, bool requestAck = false);

	/// <summary>
	/// Writes a packet to the DTC on the DCS channel
	/// </summary>
	/// <param name="packet">Packet to write</param>
	void WriteDMAPacket(const DTC_DMAPacket& packet);
	/// <summary>
	/// Writes the given data buffer to the DTC's DDR memory, via the DAQ channel.
	/// </summary>
	/// <param name="buf">DMA buffer to write. Must have an inclusive 64-bit byte count at the beginning, followed by an
	/// exclusive 64-bit block count.</param> <param name="sz">Size ofthe data in the buffer</param>
	void WriteDetectorEmulatorData(mu2e_databuff_t* buf, size_t sz);
	/**
	 * @brief Read the next DMA from the DAQ channel. If no data is present, will return nullptr
	 * @param tmo_ms Timeout
	 * @return A DTC_Event representing the data in a single DMA, or nullptr if no data/timeout
	 */
	std::unique_ptr<DTC_Event> ReadNextDAQDMA(int tmo_ms);
	/**
	 * @brief Read SubEvents from the next DMA buffer(s) from the DAQ channel. Multiple subevents may be returned
	 * from a single buffer. If a subevent spans multiple DMA buffers, this function blocks until complete.
	 * @param output Vector to append extracted SubEvents to
	 * @param tmo_ms Timeout
	 * @return True if one or more SubEvents were extracted, false if no data/timeout
	 */
	bool ReadNextDAQSubEventDMA(std::vector<std::unique_ptr<DTC_SubEvent>>& output, int tmo_ms);
	/// <summary>
	/// DCS packets are read one-at-a-time, this function reads the next one from the DTC
	/// </summary>
	/// <param name="tmo_ms">Timeout, in milliseconds, for read (will retry until timeout is expired or data received)</param>
	/// <returns>Pointer to read DCSReplyPacket. Will be nullptr if no data available.</returns>
	std::unique_ptr<DTC_DCSReplyPacket> ReadNextDCSPacket(int tmo_ms);

	/// <summary>
	//	------------
	/// As of May 2024 RAR, force user to decide which type of buffers this process owns
	//		DCS buffers is generally safer for multiple processes to access because of the lockout
	//		DAQ/DATA buffers should only be released by the 'owner' of the DMA channel.
	// -------------
	//		Releases all buffers to the hardware, from both the DAQ and DCS channels
	/// </summary>
	// void ReleaseAllBuffers()
	// {
	// 	ReleaseAllBuffers(DTC_DMA_Engine_DAQ);
	// 	ReleaseAllBuffers(DTC_DMA_Engine_DCS);
	// }

	/// <summary>
	/// Release all buffers to the hardware on the given channel
	/// </summary>
	/// <param name="channel">Channel to release</param>
	void ReleaseAllBuffers(const DTC_DMA_Engine& channel);

  private:
	std::unique_ptr<DTC_DataPacket> ReadNextPacket(const DTC_DMA_Engine& channel, int tmo_ms);
	int                             ReadBuffer(const DTC_DMA_Engine& channel, int retries = 10);
	/// <summary>
	/// This function releases all buffers except for the one containing currentReadPtr. Should only be called when done
	/// with data in other buffers!
	/// </summary>
	/// <param name="channel">Channel to release</param>
	void ReleaseBuffers(const DTC_DMA_Engine& channel);
	void WriteDataPacket(const DTC_DataPacket& packet);

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
	CFOandDTC_DMAs::DMAInfo dcsDMAInfo_;

	DTC_SubEventHeader lastGoodSubEventHeader_{};  ///< Persists across GetSubEventData calls for diagnostics on exception
	bool               hasLastGoodSubEventHeader_{false};

	// State for GetSubEventData2: cross-buffer pending subevent assembly
	std::vector<uint8_t> pendingSubEventBytes_{};       ///< Partial subevent bytes carried over from the previous DMA buffer
	size_t               pendingSubEventTotalBytes_{0}; ///< Expected total byte count of the pending subevent (0 = header not yet complete)
	bool                 lastDMABufferWasFull_{false};  ///< True when the last DMA buffer was completely full (dmaBytes==sizeof(mu2e_databuff_t)); next buffer is a raw continuation with no framing prefix

	uint8_t lastDTCErrorBitsValue_ = 0;
};
}  // namespace DTCLib
#endif
