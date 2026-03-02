
#include "TRACE/tracemf.h"

// #define TRACE_NAME "DTCSoftwareCFO"
// #define TRACE_NAME (strstr(&__FILE__[0], "/srcs/") ? strstr(&__FILE__[0], "/srcs/") + 6 : __FILE__)  /* TOO LONG */
#define TRACE_NAME &std::string(__FILE__).substr(std::string(__FILE__).rfind('/', std::string(__FILE__).rfind('/') - 1) + 1)[0]
#include <iostream>
#include "DTCSoftwareCFO.h"

#define Q(X) #X
#define QUOTE(X) Q(X)
#define VAL(X) QUOTE(X) << " = " << X

#define TLVL_SendRequestsForTimestamp TLVL_DEBUG + 5
#define TLVL_SendRequestsForTimestamp2 TLVL_DEBUG + 6
#define TLVL_SendRequestsForRange TLVL_DEBUG + 7
#define TLVL_SendRequestsForRange2 TLVL_DEBUG + 8
#define TLVL_SendRequestsForRangeImpl TLVL_DEBUG + 9

DTCLib::DTCSoftwareCFO::DTCSoftwareCFO(DTC* dtc, bool useCFOEmulator, uint16_t debugPacketCount,
									   DTC_DebugType debugType, bool stickyDebugType, bool quiet, bool asyncRR,
									   bool forceNoDebug, bool useCFODRP)
	: useCFOEmulator_(useCFOEmulator), debugPacketCount_(debugPacketCount), debugType_(debugType), stickyDebugType_(stickyDebugType), quiet_(quiet), asyncRR_(asyncRR), forceNoDebug_(forceNoDebug), theThread_(nullptr), requestsSent_(false), abort_(false)
{
	theDTC_ = dtc;
	for (auto link : DTC_ROC_Links)
	{
		linkMode_[link] = theDTC_->ReadLinkEnabled(link);
	}

	if (useCFODRP)
	{
		theDTC_->DisableAutogenDRP();
		theDTC_->EnableCFOEmulatorDRP();
	}
	else
	{
		theDTC_->DisableCFOEmulatorDRP();
		theDTC_->EnableAutogenDRP();
	}
	theDTC_->SetAllEventModeWords(1U);
	theDTC_->SetEventModeWord(0, 0U);
}

DTCLib::DTCSoftwareCFO::~DTCSoftwareCFO()
{
	TLOG(TLVL_TRACE) << "~DTCSoftwareCFO BEGIN";
	// theDTC_->DisableAutogenDRP();
	// theDTC_->DisableCFOEmulatorDRP();
	abort_ = true;
	if (theThread_ && theThread_->joinable()) theThread_->join();
	TLOG(TLVL_TRACE) << "~DTCSoftwareCFO END";
}

void DTCLib::DTCSoftwareCFO::WaitForRequestsToBeSent() const
{
	while (!requestsSent_)
	{
		usleep(1000);
	}
}

void DTCLib::DTCSoftwareCFO::SendRequestForTimestamp(DTC_EventWindowTag ts, uint32_t heartbeatsAfter, bool sendHeartbeats /* = true */)
{
	if (theDTC_->IsDetectorEmulatorInUse())
	{
		TLOG(TLVL_SendRequestsForTimestamp) << "Enabling Detector Emulator for 1 DMA";
		// theDTC_->SoftReset();
		theDTC_->DisableDetectorEmulator();
		theDTC_->SetDetectorEmulationDMACount(1);
		theDTC_->EnableDetectorEmulator();
		return;
	}
	if (!useCFOEmulator_)
	{
		theDTC_->EnableSoftwareDRP();
		for (auto link : DTC_ROC_Links)
		{
			if (linkMode_[link].TransmitEnable)
			{
				if (sendHeartbeats)
				{
					TLOG(TLVL_SendRequestsForTimestamp2) << "SendRequestForTimestamp before SendHeartbeatPacket";
					theDTC_->SendHeartbeatPacket(link, ts, quiet_);
				}

				TLOG(TLVL_SendRequestsForTimestamp2) << "SendRequestForTimestamp before DTC_DataRequestPacket req";
				DTC_DataRequestPacket req(link, ts, !forceNoDebug_, debugPacketCount_, debugType_);
				if (debugType_ == DTC_DebugType_ExternalSerialWithReset && !stickyDebugType_)
				{
					debugType_ = DTC_DebugType_ExternalSerial;
				}
				TLOG(TLVL_SendRequestsForTimestamp2) << "SendRequestForTimestamp before WriteDMADAQPacket - DTC_DataRequestPacket";
				if (!quiet_) std::cout << req.toJSON() << std::endl;
				theDTC_->WriteDMAPacket(req);
				TLOG(TLVL_SendRequestsForTimestamp2) << "SendRequestForTimestamp after  WriteDMADAQPacket - DTC_DataRequestPacket";

				for (uint32_t ii = 1; sendHeartbeats && ii <= heartbeatsAfter; ++ii)
				{
					theDTC_->SendHeartbeatPacket(link, ts + ii, quiet_);
					usleep(1000);
				}
				// usleep(2000);
			}
		}
	}
	else
	{
		TLOG(TLVL_SendRequestsForTimestamp2) << "SendRequestForTimestamp setting up DTC CFO Emulator";
		theDTC_->SetCFOEmulationMode();
		theDTC_->DisableCFOEmulation();
		theDTC_->SetCFOEmulationTimestamp(ts);
		for (auto link : DTC_ROC_Links)
		{
			if (linkMode_[link].TransmitEnable)
			{
				theDTC_->SetROCEmulationNumPackets(link, debugPacketCount_);
			}
		}
		theDTC_->SetCFOEmulationNumHeartbeats(1);
		theDTC_->SetCFOEmulationEventWindowInterval(20000);
		//theDTC_->SetCFOEmulationDebugType(debugType_);
		theDTC_->SetCFOEmulationModeByte(5, 1);
		theDTC_->SetCFOEmulationNumNullHeartbeats(heartbeatsAfter);
		//if (!forceNoDebug_)
		//	theDTC_->EnableDebugPacketMode();
		//else
		//	theDTC_->DisableDebugPacketMode();
		TLOG(TLVL_SendRequestsForTimestamp2) << "SendRequestForTimestamp enabling DTC CFO Emulator";
		theDTC_->EnableCFOEmulation();
		TLOG(TLVL_SendRequestsForTimestamp2) << "SendRequestForTimestamp done";
	}
	requestsSent_ = true;
}

void DTCLib::DTCSoftwareCFO::SendRequestsForRange(int count, DTC_EventWindowTag start, bool increment,
												  uint32_t delayBetweenDataRequests, int requestsAhead,
												  uint32_t heartbeatsAfter, bool sendHeartbeats /* = true */)
{
	if (theDTC_->IsDetectorEmulatorInUse())
	{
		TLOG(TLVL_SendRequestsForRange) << "Enabling Detector Emulator for " << count << " DMAs";

		// theDTC_->SoftReset();
		theDTC_->DisableDetectorEmulator();
		theDTC_->SetDetectorEmulationDMACount(count + 1);
		theDTC_->EnableDetectorEmulator();
		return;
	}

	// the minimal heartbeat interval is 0x20, or 800 ns
	if (delayBetweenDataRequests < 0x20)
		delayBetweenDataRequests = 0x20;

	TLOG(TLVL_SendRequestsForRange2) << VAL(count);
	TLOG(TLVL_SendRequestsForRange2) << VAL(delayBetweenDataRequests);
	TLOG(TLVL_SendRequestsForRange2) << VAL(requestsAhead);
	TLOG(TLVL_SendRequestsForRange2) << VAL(heartbeatsAfter);

	if (!useCFOEmulator_)
	{
		requestsSent_ = false;
		if (asyncRR_)
		{
			if (theThread_ && theThread_->joinable()) theThread_->join();
			theThread_.reset(new std::thread(&DTCSoftwareCFO::SendRequestsForRangeImplAsync, this, start, count, increment,
											 delayBetweenDataRequests, heartbeatsAfter, sendHeartbeats));
		}
		else
		{
			if (theThread_ && theThread_->joinable()) theThread_->join();
			theThread_.reset(new std::thread(&DTCSoftwareCFO::SendRequestsForRangeImplSync, this, start, count, increment,
											 delayBetweenDataRequests, requestsAhead, heartbeatsAfter, sendHeartbeats));
		}
		WaitForRequestsToBeSent();
	}
	else
	{
		TLOG(TLVL_SendRequestsForRange) << "SendRequestsForRange setting up DTC CFO Emulator";

		theDTC_->SetCFOEmulationMode();
		theDTC_->DisableCFOEmulation();
		theDTC_->SetCFOEmulationTimestamp(start);
		for (auto link : DTC_ROC_Links)
		{
			if (linkMode_[link].TransmitEnable)
			{
				theDTC_->SetROCEmulationNumPackets(link, debugPacketCount_);
			}
		}
		theDTC_->SetCFOEmulationNumHeartbeats(count);
		//theDTC_->SetCFOEmulationDebugType(debugType_);
		theDTC_->SetCFOEmulationModeByte(5, 1);
		theDTC_->SetCFOEmulationNumNullHeartbeats(heartbeatsAfter);
		//if (!forceNoDebug_)
		//	theDTC_->EnableDebugPacketMode();
		//else
		//	theDTC_->DisableDebugPacketMode();
		theDTC_->SetCFOEmulationEventWindowInterval(delayBetweenDataRequests);
		TLOG(TLVL_SendRequestsForRange) << "SendRequestsForRange enabling DTC CFO Emulator";
		theDTC_->EnableCFOEmulation();
		TLOG(TLVL_SendRequestsForRange) << "SendRequestsForRange done";
	}
}

void DTCLib::DTCSoftwareCFO::SendRequestsForList(std::set<DTC_EventWindowTag> timestamps,
												 uint32_t delayBetweenDataRequests,
												 uint32_t heartbeatsAfter)
{
	if (theDTC_->IsDetectorEmulatorInUse())
	{
		TLOG(TLVL_SendRequestsForRangeImpl) << "Enabling Detector Emulator for " << timestamps.size() << " DMAs";
		// theDTC_->SoftReset();
		theDTC_->DisableDetectorEmulator();
		theDTC_->SetDetectorEmulationDMACount(timestamps.size() + 1);
		theDTC_->EnableDetectorEmulator();
		return;
	}

	if (theThread_ && theThread_->joinable()) theThread_->join();
	theThread_.reset(
		new std::thread(&DTCSoftwareCFO::SendRequestsForListImplAsync, this, timestamps, delayBetweenDataRequests, heartbeatsAfter));
}

void DTCLib::DTCSoftwareCFO::SendRequestsForListImplAsync(std::set<DTC_EventWindowTag> timestamps,
														  uint32_t delayBetweenDataRequests,
														  uint32_t heartbeatsAfter)
{
	//-----------------------------------------------------------------------------
	// P.Murat: according to Ryan , who referred to Rick K., the minimal heartbeat
	// interval is 0x20, or 800 ns
	//-----------------------------------------------------------------------------
	if (delayBetweenDataRequests < 0x20)
	{
		delayBetweenDataRequests = 0x20;
	}

	auto ii = timestamps.begin();
	while (ii != timestamps.end() && !abort_)
	{
		TLOG(TLVL_SendRequestsForRangeImpl) << "Setting up CFO Emulator for next entry in list";
		auto thisTimestamp = *ii;
		++ii;
		DTC_EventWindowTag nextTimestamp = thisTimestamp + 5;  // Generate 5 nulls at the end of the list
		if (ii != timestamps.end())
		{
			nextTimestamp = *ii;
		}

		theDTC_->SetCFOEmulationMode();
		theDTC_->DisableCFOEmulation();
		theDTC_->SetCFOEmulationTimestamp(thisTimestamp);
		for (auto link : DTC_ROC_Links)
		{
			if (linkMode_[link].TransmitEnable)
			{
				theDTC_->SetROCEmulationNumPackets(link, debugPacketCount_);
			}
		}
		theDTC_->SetCFOEmulationNumHeartbeats(1);
		theDTC_->SetCFOEmulationNumNullHeartbeats(heartbeatsAfter);
		//theDTC_->SetCFOEmulationDebugType(debugType_);
		theDTC_->SetCFOEmulationModeByte(5, 1);
		//if (!forceNoDebug_)
		//	theDTC_->EnableDebugPacketMode();
		//else
		//	theDTC_->DisableDebugPacketMode();
		theDTC_->SetCFOEmulationEventWindowInterval(delayBetweenDataRequests);
		TLOG(TLVL_SendRequestsForRangeImpl) << "SendRequestsForRange enabling DTC CFO Emulator";
		theDTC_->EnableCFOEmulation();
		TLOG(TLVL_SendRequestsForRangeImpl) << "SendRequestsForRange done";
	}
}

void DTCLib::DTCSoftwareCFO::SendRequestsForRangeImplSync(DTC_EventWindowTag start, int count, bool increment,
														  uint32_t delayBetweenDataRequests, int requestsAhead,
														  uint32_t heartbeatsAfter, bool sendHeartbeats /* = true */)
{
	try
	{
		TLOG(TLVL_SendRequestsForRangeImpl) << "SendRequestsForRangeImplSync Start";
		for (auto ii = 0; ii < count; ++ii)
		{
			auto ts = start + (increment ? ii : 0);

			SendRequestForTimestamp(ts, heartbeatsAfter, sendHeartbeats);
			if (ii >= requestsAhead || ii == count - 1)
			{
				requestsSent_ = true;
			}

			usleep(delayBetweenDataRequests);
			if (abort_) return;
		}
	}
	catch (const std::exception& e)
	{
		TLOG(TLVL_ERROR) << e.what() << '\n';
	}
}

void DTCLib::DTCSoftwareCFO::SendRequestsForRangeImplAsync(DTC_EventWindowTag start, int count, bool increment,
														   uint32_t delayBetweenDataRequests, uint32_t heartbeatsAfter,
														   bool sendHeartbeats /* = true */)
{
	try
	{
		TLOG(TLVL_SendRequestsForRangeImpl) << "SendRequestsForRangeImplAsync Start";

		// Send Readout Requests first
		for (auto ii = 0; ii < count; ++ii)
		{
			auto ts = start + (increment ? ii : 0);
			for (auto link : DTC_ROC_Links)
			{
				if (linkMode_[link].TransmitEnable && sendHeartbeats)
				{
					TLOG(TLVL_SendRequestsForRangeImpl) << "SendRequestsForRangeImpl before SendHeartbeatPacket";
					theDTC_->SendHeartbeatPacket(link, ts, quiet_);
				}
				if (abort_) return;
			}
		}
		TLOG(TLVL_SendRequestsForRangeImpl) << "SendRequestsForRangeImpl setting RequestsSent flag";
		requestsSent_ = true;

		// Now do the DataRequests, sleeping for the required delay between each.
		for (auto ii = 0; ii < count; ++ii)
		{
			auto ts = start + (increment ? ii : 0);
			for (auto link : DTC_ROC_Links)
			{
				if (linkMode_[link].TransmitEnable)
				{
					TLOG(TLVL_SendRequestsForRangeImpl) << "SendRequestsForRangeImpl before DTC_DataRequestPacket req";
					DTC_DataRequestPacket req(link, ts, !forceNoDebug_, static_cast<uint16_t>(debugPacketCount_), debugType_);
					if (debugType_ == DTC_DebugType_ExternalSerialWithReset && !stickyDebugType_)
					{
						debugType_ = DTC_DebugType_ExternalSerial;
					}
					TLOG(TLVL_SendRequestsForRangeImpl) << "SendRequestsForRangeImpl before WriteDMADAQPacket - DTC_DataRequestPacket";
					if (!quiet_) std::cout << req.toJSON() << std::endl;
					theDTC_->WriteDMAPacket(req);
					TLOG(TLVL_SendRequestsForRangeImpl) << "SendRequestsForRangeImpl after  WriteDMADAQPacket - DTC_DataRequestPacket";
					if (abort_) return;
				}
				if (abort_) return;
			}
			usleep(delayBetweenDataRequests);
		}

		for (uint32_t ii = 0; ii < heartbeatsAfter; ++ii)
		{
			auto ts = start + (increment ? count + ii : 0);
			for (auto link : DTC_ROC_Links)
			{
				if (linkMode_[link].TransmitEnable && sendHeartbeats)
				{
					TLOG(TLVL_SendRequestsForRangeImpl) << "SendRequestsForRangeImpl before SendHeartbeatPacket";
					theDTC_->SendHeartbeatPacket(link, ts, quiet_);
				}
				if (abort_) return;
			}
		}
	}
	catch (const std::exception& e)
	{
		TLOG(TLVL_ERROR) << e.what() << '\n';
	}
}