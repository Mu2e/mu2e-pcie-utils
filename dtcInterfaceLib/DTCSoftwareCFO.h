///////////////////////////////////////////////////////////////////////////////
// Author: Eric Flumerfelt, FNAL RSI
// Date: 8/13/2015
///////////////////////////////////////////////////////////////////////////////

#ifndef DTCSOFTWARECFO_H
#define DTCSOFTWARECFO_H 1

#include "DTC.h"
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

#include <atomic>
#include <set>
#include <thread>

namespace DTCLib {
/// <summary>
/// The DTCSoftwareCFO class is responsible for sending ReadoutRequest and
/// DataRequest packets to the DTC in the absence of a functioning CFO.
/// Requests are sent asynchronously, and the overall behaviour is meant to
/// emulate a system with a CFO as closely as possible.
/// </summary>
class DTCSoftwareCFO
{
public:
	/// <summary>
	/// Construct an instance of the DTCSoftwareCFO class
	/// </summary>
	/// <param name="dtc">Pointer to DTC object to use for sending requests</param>
	/// <param name="useCFOEmulator">Whether to use the DTC CFO emulator</param>
	/// <param name="debugPacketCount">Number of debug packets to request</param>
	/// <param name="debugType">Flag for the debug packets</param>
	/// <param name="stickyDebugType">Whether the debug type should stay as the flag or revert to default</param>
	/// <param name="quiet">When true, don't print debug information to screen</param>
	/// <param name="asyncRR">Whether to send ReadoutRequests asynchronously</param>
	/// <param name="forceNoDebugMode">Do NOT set the Debug flag in Data Request</param>
	/// <param name="useCFODRP">Send DRPs from the CFO Emulator</param>
	DTCSoftwareCFO(DTC* dtc, bool useCFOEmulator, uint16_t debugPacketCount = 0,
				   DTC_DebugType debugType = DTC_DebugType_ExternalSerialWithReset, bool stickyDebugType = false,
				   bool quiet = false, bool asyncRR = false, bool forceNoDebugMode = false, bool useCFODRP = false);
	/// <summary>
	/// DTCSoftwareCFO Destructor
	/// </summary>
	~DTCSoftwareCFO();

	/// <summary>
	/// Send a Heartbeat Packet and data request for a given timestamp
	/// </summary>
	/// <param name="ts">Timestamp for requests</param>
	/// <param name="heartbeatsAfter">Number of heartbeats after reqeust to drive DTC state machine</param>
	void SendRequestForTimestamp(DTC_EventWindowTag ts = DTC_EventWindowTag(static_cast<uint64_t>(0)), uint32_t heartbeatsAfter = 16, bool sendHeartbeats = true);
	/// <summary>
	/// Send Heartbeat Packets and Data Requests for a range of timestamps.
	/// </summary>
	/// <param name="count">Number of requests</param>
	/// <param name="start">Starting timestamp (Default: 0)</param>
	/// <param name="increment">Whether to increment the timestamp for each request (Default: true)</param>
	/// <param name="delayBetweenDataRequests">Number of microseconds to wait between requests</param>
	/// <param name="requestsAhead">Number of Heartbeat Packets to send ahead of data requests</param>
	/// <param name="heartbeatsAfter">How many Heartbeat Packets to send after all Data Requests have been sent to flush the system</param>
	void SendRequestsForRange(int count, DTC_EventWindowTag start = DTC_EventWindowTag(static_cast<uint64_t>(0)),
							  bool increment = true, uint32_t delayBetweenDataRequests = 0, int requestsAhead = 1, uint32_t heartbeatsAfter = 16, bool sendHeartbeats = true);

	/// <summary>
	/// Send requests for a list of timestamps.
	/// </summary>
	/// <param name="timestamps">List of timestamps to send</param>
	/// <param name="delayBetweenDataRequests">Number of microseconds to wait between requests</param>
	/// <param name="heartbeatsAfter">Number of heartbeats after reqeust to drive DTC state machine</param>
	void SendRequestsForList(std::set<DTC_EventWindowTag> timestamps, uint32_t delayBetweenDataRequests = 0, uint32_t heartbeatsAfter = 16);

	/// <summary>
	/// Enable quiet mode.
	///
	/// When in quiet mode, DTCSoftwareCFO will not print ReadoutRequest packets or DataRequest packets to screen.
	/// </summary>
	/// <param name="quiet">Whether to enable quiet mode.</param>
	void setQuiet(bool quiet) { quiet_ = quiet; }

	/// <summary>
	/// Set the number of packets requested
	/// </summary>
	/// <param name="dbc">Packet count</param>
	void setDebugPacketCount(uint16_t dbc) { debugPacketCount_ = dbc; }

	/// <summary>
	/// Blocks until all ReadoutRequest packets have been sent
	/// </summary>
	void WaitForRequestsToBeSent() const;

	/// <summary>
	/// Get a pointer to the DTC instance
	/// </summary>
	/// <returns>Pointer to the DTC instance</returns>
	DTC* GetDTC() const { return theDTC_; }

private:
	void SendRequestsForRangeImplAsync(DTC_EventWindowTag start, int count, bool increment = true,
									   uint32_t delayBetweenDataRequests = 0, uint32_t heartbeatsAfter = 16, bool sendHeartbeats = true);
	void SendRequestsForRangeImplSync(DTC_EventWindowTag start, int count, bool increment = true,
									  uint32_t delayBetweenDataRequests = 0, int requestsAhead = 1, uint32_t heartbeatsAfter = 16, bool sendHeartbeats = true);

	void SendRequestsForListImplAsync(std::set<DTC_EventWindowTag> timestamps, uint32_t delayBetweenDataRequests = 0, uint32_t heartbeatsAfter = 16);

	// Request Parameters
	bool useCFOEmulator_;
	uint16_t debugPacketCount_;
	DTC_DebugType debugType_;
	bool stickyDebugType_;
	bool quiet_;  // Don't print as much
	bool asyncRR_;
	bool forceNoDebug_;

	// Object basic properties (not accessible)
	DTC* theDTC_;
	DTC_LinkEnableMode linkMode_[6];
	std::unique_ptr<std::thread> theThread_;
	std::atomic<bool> requestsSent_;
	std::atomic<bool> abort_;
};
}  // namespace DTCLib
#endif  // ifndef DTCSOFTWARECFO_H
