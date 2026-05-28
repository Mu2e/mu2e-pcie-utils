
#include "TRACE/tracemf.h"
#define TRACE_NAME "DTC.cpp"

#include "DTC.h"
#define TLVL_GetData TLVL_DEBUG + 5
#define TLVL_GetJSONData TLVL_DEBUG + 6
#define TLVL_ReadBuffer TLVL_DEBUG + 7
#define TLVL_ReadNextDAQPacket TLVL_DEBUG + 8
#define TLVL_ReadNextDCSPacket TLVL_DEBUG + 9
#define TLVL_SendDCSRequestPacket TLVL_DEBUG + 10
#define TLVL_SendHeartbeatPacket TLVL_DEBUG + 11
#define TLVL_VerifySimFileInDTC TLVL_DEBUG + 12
#define TLVL_VerifySimFileInDTC2 TLVL_DEBUG + 13
#define TLVL_VerifySimFileInDTC3 TLVL_DEBUG + 14
#define TLVL_WriteSimFileToDTC TLVL_DEBUG + 15
#define TLVL_Crossover TLVL_DEBUG + 30
#define TLVL_Crossover1 TLVL_DEBUG + 31
#define TLVL_WriteSimFileToDTC2 TLVL_DEBUG + 16
#define TLVL_WriteSimFileToDTC3 TLVL_DEBUG + 17
#define TLVL_WriteDetectorEmulatorData TLVL_DEBUG + 18
#define TLVL_WriteDataPacket TLVL_DEBUG + 19
#define TLVL_ReleaseBuffers TLVL_DEBUG + 20
#define TLVL_GetCurrentBuffer TLVL_DEBUG + 21

#include "dtcInterfaceLib/otsStyleCoutMacros.h"

#define DTC_TLOG(lvl) TLOG(lvl) << "DTC " << device_.getDeviceUID() << ": "
#undef __COUT_HDR__
#define __COUT_HDR__ "DTC " << device_.getDeviceUID() << ": "

#include <unistd.h>
#include <fstream>
#include <iostream>
#include <sstream>  // Convert uint to hex string

DTCLib::DTC::DTC(DTC_SimMode mode, int dtc, unsigned rocMask, std::string expectedDesignVersion, bool skipInit, std::string simMemoryFile, const std::string& uid)
	: DTC_Registers(mode, dtc, simMemoryFile, rocMask, expectedDesignVersion, skipInit, uid), daqDMAInfo_(), dcsDMAInfo_()
{
	__COUT__ << "CONSTRUCTOR";
}

DTCLib::DTC::~DTC()
{
	__COUT__ << "DESTRUCTOR";
	// TLOG_ENTEX(-6);
	TRACE_EXIT
	{
		__COUT__ << "DESTRUCTOR exit";
	};

	// do not want to release all from destructor, in case this instance of the device is not the 'owner' of a DMA-channel
	//	so do not call ReleaseAllBuffers(DTC_DMA_Engine_DAQ) or ReleaseAllBuffers(DTC_DMA_Engine_DCS);

	// assume the destructor is destructive (could be in response to exceptions), so force ending of dcs lock
	try
	{
		device_.end_dcs_transaction();
	}
	catch (...)
	{
		__COUT_WARN__ << "Ignoring exception caught in DESTRUCTOR while ending of dcs lock";
	}
}

//
// DMA Functions -- This if for HW event building
//
std::vector<std::unique_ptr<DTCLib::DTC_Event>> DTCLib::DTC::GetData(DTC_EventWindowTag when, bool matchEventWindowTag)
{
	DTC_TLOG(TLVL_GetData) << "GetData begin EventWindowTag=" << when.GetEventWindowTag(true) << ", matching=" << (matchEventWindowTag ? "true" : "false");
	std::vector<std::unique_ptr<DTC_Event>> output;
	std::unique_ptr<DTC_Event> packet = nullptr;

	// Release read buffers here "I am done with everything I read before" (because the return is pointers to the raw data, not copies)
	ReleaseBuffers(DTC_DMA_Engine_DAQ);  // Currently race condition because GetCurrentBuffer(info) is used inside to decide how many buffers to release.

	try
	{
		// Read the next DTC_Event
		auto tries = 0;
		while (packet == nullptr && tries < 3)
		{
			DTC_TLOG(TLVL_GetData) << "GetData before ReadNextDAQPacket, tries=" << tries;
			packet = ReadNextDAQDMA(100);
			if (packet != nullptr)
			{
				DTC_TLOG(TLVL_GetData) << "GetData after ReadDMADAQPacket, ts=0x" << std::hex
									   << packet->GetEventWindowTag().GetEventWindowTag(true);
			}
			tries++;
			// if (packet == nullptr) usleep(5000);
		}
		if (packet == nullptr)
		{
			DTC_TLOG(TLVL_GetData) << "GetData: Timeout Occurred! (DTC_Event is nullptr after retries)";
			return output;
		}

		if (packet->GetEventWindowTag() != when && matchEventWindowTag)
		{
			DTC_TLOG(TLVL_ERROR) << "GetData: Error: DTC_Event has wrong Event Window Tag! 0x" << std::hex << when.GetEventWindowTag(true)
								 << "(expected) != 0x" << std::hex << packet->GetEventWindowTag().GetEventWindowTag(true);
			packet.reset(nullptr);
			daqDMAInfo_.currentReadPtr = daqDMAInfo_.lastReadPtr;
			return output;
		}

		when = packet->GetEventWindowTag();

		DTC_TLOG(TLVL_GetData) << "GetData: Adding DTC_Event " << (void*)daqDMAInfo_.lastReadPtr << " to the list (first)";
		output.push_back(std::move(packet));

		auto done = false;
		while (!done)
		{
			DTC_TLOG(TLVL_GetData) << "GetData: Reading next DAQ Packet";
			packet = ReadNextDAQDMA(0);
			if (packet == nullptr)  // End of Data
			{
				DTC_TLOG(TLVL_GetData) << "GetData: Next packet is nullptr; we're done";
				done = true;
				daqDMAInfo_.currentReadPtr = nullptr;
			}
			else if (packet->GetEventWindowTag() != when)
			{
				DTC_TLOG(TLVL_GetData) << "GetData: Next packet has ts=0x" << std::hex << packet->GetEventWindowTag().GetEventWindowTag(true)
									   << ", not 0x" << std::hex << when.GetEventWindowTag(true) << "; we're done";
				done = true;
				daqDMAInfo_.currentReadPtr = daqDMAInfo_.lastReadPtr;
			}
			else
			{
				DTC_TLOG(TLVL_GetData) << "GetData: Next packet has same ts=0x" << std::hex
									   << packet->GetEventWindowTag().GetEventWindowTag(true) << ", continuing (bc=0x" << std::hex
									   << packet->GetEventByteCount() << ")";
			}

			if (!done)
			{
				DTC_TLOG(TLVL_GetData) << "GetData: Adding pointer " << (void*)daqDMAInfo_.lastReadPtr << " to the list";
				output.push_back(std::move(packet));
			}
		}
	}
	catch (DTC_WrongPacketTypeException& ex)
	{
		DTC_TLOG(TLVL_WARNING) << "GetData: Bad omen: Wrong packet type at the current read position";
		daqDMAInfo_.currentReadPtr = nullptr;
	}
	catch (DTC_IOErrorException& ex)
	{
		daqDMAInfo_.currentReadPtr = nullptr;
		DTC_TLOG(TLVL_WARNING) << "GetData: IO Exception Occurred!";
	}
	catch (DTC_DataCorruptionException& ex)
	{
		daqDMAInfo_.currentReadPtr = nullptr;
		DTC_TLOG(TLVL_WARNING) << "GetData: Data Corruption Exception Occurred!";
	}

	DTC_TLOG(TLVL_GetData) << "GetData RETURN";
	return output;
}  // GetData

//
// GetSubEventData v3 -- extract subevents into Events from the current data stream
std::vector<std::shared_ptr<DTCLib::DTC_Event>> DTCLib::DTC::GetSubEventDataAsEvents(
	DTC_EventWindowTag when, bool matchEventWindowTag,
	const size_t vectorBundleTarget /* = 1 */, const size_t retries /* = 3 */)
{
	(void)when;                 // not yet used; filtering can be added once basic flow works
	(void)matchEventWindowTag;  // not yet used; filtering can be added once basic flow works

	// Save up to 8 trailing QWs of the current DMA buffer for diagnostic dumps on later
	// exceptions. Takes its inputs as parameters so it doesn't capture variables that
	// aren't in scope yet at the lambda's definition site.
	auto saveBufferTail = [this](const uint8_t* dmaBufferStartPtr, size_t dmaBytes) {
		const size_t tailStart = (dmaBytes >= 64) ? dmaBytes - 64 : 0;
		lastBufferTailCount_ = 0;
		for (size_t i = tailStart; i + 8 <= dmaBytes && lastBufferTailCount_ < 8; i += 8)
			lastBufferTailQwords_[lastBufferTailCount_++] =
				*reinterpret_cast<const uint64_t*>(dmaBufferStartPtr + i);
	};

	// Returns the last good event window tag as a string, or "-1" if unavailable.
	auto ewtTag = [&]() -> std::string {
		if (!hasLastGoodSubEventHeader_) return "-1";
		return std::to_string(lastGoodSubEventHeader_.event_tag_low |
							  (static_cast<uint64_t>(lastGoodSubEventHeader_.event_tag_high) << 32));
	};

	// Returns the ring-buffer slot index of a DMA buffer pointer as a 2-digit string
	// (e.g. "07", "42"), or "??" when the pointer is outside the mmap'd region.
	auto bufferIndexStr = [this](const void* ptr) -> std::string {
		const int idx = GetDevice()->GetBufferIndex(DTC_DMA_Engine_DAQ, ptr);
		if (idx < 0) return "??";
		std::ostringstream oss;
		oss << std::setw(2) << std::setfill('0') << idx;
		return oss.str();
	};

	if (needToFinishEvent_)
	{
		auto inProgress = std::move(extractedEvents_.back());
		extractedEvents_.clear();
		extractedEvents_.push_back(std::move(inProgress));
	}
	else
	{
		extractedEvents_.clear();
		currentEventSize_ = 0;
		subEventByteCount_ = 0;
		extractedSubeventBytes_ = 0;
	}
	extractedEvents_.reserve(vectorBundleTarget + 1);
	size_t eventsParsedThisTime = 0;

	// Release the previous buffer NOW — we are done with it.
	ReleaseBuffers(DTC_DMA_Engine_DAQ);

	// ------------------------------------------------------------------
	do  // primary loop to read buffers until target event count is reached or timeout
	{
		DTC_TLOG(TLVL_GetCurrentBuffer) << "GetSubEventData BEGIN [last EWT=" << ewtTag()
										<< "] eventsParsedThisTime=" << eventsParsedThisTime
										<< " totalEventsParsed_=" << totalEventsParsed_;

		// Read next buffer
		int sts;
		{
			int retry = retries;
			do {
				sts = ReadBuffer(DTC_DMA_Engine_DAQ, 100 /*ms*/);
			} while (sts <= 0 && --retry > 0);
		}
		if (sts <= 0)
		{
			DTC_TLOG(TLVL_GetCurrentBuffer) << "GetSubEventData: ReadBuffer returned " << sts << ", no data";
			{
				std::vector<std::shared_ptr<DTC_Event>> result;
				size_t cc = extractedEvents_.size() - (needToFinishEvent_ ? 1 : 0);
				result.reserve(cc);
				for (size_t i = 0; i < cc; ++i)
					result.push_back(extractedEvents_[i]);
				return result;
			}
		}

		if (lastBufferTailCount_ == 8)
		{
			DTC_TLOG(TLVL_GetData) << "GetSubEventData: Last dmaBufferIndex #" << lastDMABufferIndex_ << " tail qwords: "
								   << std::hex
								   << lastBufferTailQwords_[0] << " "
								   << lastBufferTailQwords_[1] << " "
								   << lastBufferTailQwords_[2] << " "
								   << lastBufferTailQwords_[3] << " | "
								   << lastBufferTailQwords_[4] << " "
								   << lastBufferTailQwords_[5] << " "
								   << lastBufferTailQwords_[6] << " "
								   << lastBufferTailQwords_[7];
		}

		// sts = number of bytes the DMA engine transferred into this buffer
		const size_t dmaBytes = static_cast<size_t>(sts);

		const bool prevBufferWasFull = lastDMABufferWasFull_;
		const bool prevBufferWasMax = lastDMABufferWasMax_;
		const size_t MAX_TRANSFER_SIZE = 0xffe9;
		lastDMABufferWasFull_ = (dmaBytes == MAX_TRANSFER_SIZE);
		lastDMABufferWasMax_ = (dmaBytes == sizeof(mu2e_databuff_t));

		const uint8_t* dmaBufferStartPtr = reinterpret_cast<const uint8_t*>(&daqDMAInfo_.buffer.back()[0]);
		{
			const uint64_t* qw = reinterpret_cast<const uint64_t*>(dmaBufferStartPtr);
			DTC_TLOG(TLVL_GetData) << "GetSubEventData: first 8 QWs of dmaBufferIndex #" << bufferIndexStr(dmaBufferStartPtr) << ": " << std::hex
								   << qw[0] << " " << qw[1] << " " << qw[2] << " " << qw[3] << " | "
								   << qw[4] << " " << qw[5] << " " << qw[6] << " " << qw[7] << std::dec;
		}
		saveBufferTail(dmaBufferStartPtr, dmaBytes);
		lastDMABufferIndex_ = bufferIndexStr(dmaBufferStartPtr);
		if (lastBufferTailCount_ == 8)
		{
			DTC_TLOG(TLVL_GetData) << "GetSubEventData: This dmaBufferIndex #" << lastDMABufferIndex_ << " tail qwords: "
								   << std::hex
								   << lastBufferTailQwords_[0] << " "
								   << lastBufferTailQwords_[1] << " "
								   << lastBufferTailQwords_[2] << " "
								   << lastBufferTailQwords_[3] << " | "
								   << lastBufferTailQwords_[4] << " "
								   << lastBufferTailQwords_[5] << " "
								   << lastBufferTailQwords_[6] << " "
								   << lastBufferTailQwords_[7];
		}
		{
			const uint64_t* qw = reinterpret_cast<const uint64_t*>(dmaBufferStartPtr);
			if (qw[0] == 0xdeadbeef)
			{
				__SS__ << "GetSubEventData: Detected 0xdeadbeef at start of dmaBufferIndex #" << bufferIndexStr(dmaBufferStartPtr) << " -- this buffer is likely not filled by DMA yet! Is kernel driver and DTC out of sync? Try sending a finite number (e.g. 2) of events through the system TWICE. If that doesn't work, try resetting the PCIe.\nSpy output follows:\n"
					   << __E__;
				device_.spy(DTC_DMA_Engine_DAQ, 3 /* for once */ | 8 /* for wide view */ | (1 << 28) /* to force spy */, ss);
				__SS_THROW__;
				// continue;  // skip processing this buffer, try to read the next one
			}
		}

		// The DMA descriptor byte count (sts) is the number of bytes actually filled.
		// The last byte is the AXI tlast flag byte when the buffer is not completely full.
		// When it IS completely full (dmaBytes == sizeof(mu2e_databuff_t)) there is no tlast byte.
		const size_t payloadBytes = (dmaBytes < sizeof(mu2e_databuff_t)) ? dmaBytes - 1 : dmaBytes;

		DTC_TLOG(TLVL_GetData) << "GetSubEventData: new buffer dmaBufferIndex #" << bufferIndexStr(dmaBufferStartPtr)
							   << " dmaBytes=" << dmaBytes << "(0x" << std::hex << dmaBytes << std::dec << ")"
							   << " bufferTotalBytes=" << payloadBytes << "(0x" << std::hex << payloadBytes << std::dec << ")"
							   << " maxBufferBytes=" << sizeof(mu2e_databuff_t) << "(0x" << std::hex << sizeof(mu2e_databuff_t) << std::dec << ")"
							   << " prevBufferWasFull=" << (prevBufferWasFull ? "true" : "false")
							   << " prevBufferWasMax=" << (prevBufferWasMax ? "true" : "false")
							   << " thisBufferIsFull=" << (lastDMABufferWasFull_ ? "true" : "false")
							   << " thisBufferIsMax=" << (lastDMABufferWasMax_ ? "true" : "false");

		if (dmaBytes > sizeof(mu2e_databuff_t))
		{
			__SS__ << "GetSubEventData: Detected overflow transfer size at dmaBufferIndex #" << bufferIndexStr(dmaBufferStartPtr)
				   << " [0x" << std::hex << dmaBytes << " > sizeof(mu2e_databuff_t)=0x" << sizeof(mu2e_databuff_t) << std::dec
				   << "]\nSpy output follows:\n"
				   << __E__;
			device_.spy(DTC_DMA_Engine_DAQ, 3 | 8 | (1 << 28), ss);
			__SS_THROW__;
		}

		size_t dmaBufferOffset = 0;

		if (needToFinishEvent_ && !prevBufferWasMax && payloadBytes >= sizeof(uint64_t))
		{
			const size_t remainingBytes = subEventByteCount_ - extractedSubeventBytes_;
			const size_t continuationPrefix = static_cast<size_t>(*reinterpret_cast<const uint64_t*>(dmaBufferStartPtr) & 0xFFFFFFULL);
			if (continuationPrefix == remainingBytes + 8)
			{
				DTC_TLOG(TLVL_GetData) << "GetSubEventData: continuation prefix validated (final) at dmaBufferIndex #" << bufferIndexStr(dmaBufferStartPtr)
									   << " prefixVal=0x" << std::hex << continuationPrefix << std::dec
									   << " == remainingBytes(" << remainingBytes << ") + 8"
									   << "; skipping 8-byte prefix";
				dmaBufferOffset = sizeof(uint64_t);
			}
			else if (continuationPrefix == MAX_TRANSFER_SIZE - 1)
			{
				DTC_TLOG(TLVL_GetData) << "GetSubEventData: continuation prefix validated (intermediate, another max sub-transfer) at dmaBufferIndex #" << bufferIndexStr(dmaBufferStartPtr)
									   << " prefixVal=0x" << std::hex << continuationPrefix << std::dec
									   << " == MAX_TRANSFER_SIZE-1=0x" << (MAX_TRANSFER_SIZE - 1) << std::dec
									   << " remainingBytes=" << remainingBytes
									   << "; skipping 8-byte prefix";
				dmaBufferOffset = sizeof(uint64_t);
			}
			else
			{
				__SS__ << "GetSubEventData: continuation prefix mismatch at dmaBufferIndex #" << bufferIndexStr(dmaBufferStartPtr)
					   << " prefixVal=" << continuationPrefix << "(0x" << std::hex << continuationPrefix << std::dec << ")"
					   << " expected " << (remainingBytes + 8) << "(0x" << std::hex << (remainingBytes + 8) << std::dec << ")"
					   << " or MAX_TRANSFER_SIZE-1=0x" << std::hex << (MAX_TRANSFER_SIZE - 1) << std::dec
					   << " remainingBytes=" << remainingBytes
					   << " extractedSubeventBytes_=" << extractedSubeventBytes_ << "/" << subEventByteCount_
					   << " prevBufferWasFull=" << (prevBufferWasFull ? "true" : "false")
					   << " prevBufferWasMax=" << (prevBufferWasMax ? "true" : "false");
				__SS_THROW__;
			}
		}
		else if (needToFinishEvent_ && prevBufferWasMax)
		{
			DTC_TLOG(TLVL_GetData) << "GetSubEventData: raw continuation (prevBufferWasMax) at dmaBufferIndex #" << bufferIndexStr(dmaBufferStartPtr)
								   << " -- no prefix, starting at offset 0";
		}

		while (dmaBufferOffset < payloadBytes)
		{
			DTC_TLOG(TLVL_GetData) << "GetSubEventData: top of buffer processing loop, dmaBufferIndex #" << bufferIndexStr(dmaBufferStartPtr) << " dmaBufferOffset= " << dmaBufferOffset
								   << "/" << payloadBytes
								   << " eventsParsedThisTime=" << eventsParsedThisTime
								   << " totalEventsParsed_=" << totalEventsParsed_
								   << " needToFinishEvent_=" << (needToFinishEvent_ ? "true" : "false")
								   << " subEventHeaderQwsFilled_=" << subEventHeaderQwsFilled_
								   << " extractedSubeventBytes_=" << extractedSubeventBytes_ << "/" << subEventByteCount_;

			// ---------------------------------------------------------------
			// Assemble the DTC_SubEventHeader one QW at a time into the standing
			// buffer (subEventHeaderBuf_). The header is 6 QWs (48 B); it may
			// span DMA buffer boundaries, so state persists in DTC class members.
			// As soon as the first 2 QWs are in, we know the event size and can
			// emplace_back the DTC_Event. Once all 6 QWs are in, the assembled
			// header is memcpy'd into the Event's owned buffer.
			// ---------------------------------------------------------------
			if (currentEventSize_ == 0)  // haven't yet determined the event size from the first 2 header QWs
			{
				// Fill QWs from the current buffer until we have the first 2 (or the buffer runs out)
				while (subEventHeaderQwsFilled_ < 2 && (payloadBytes - dmaBufferOffset) >= sizeof(uint64_t))
				{
					subEventHeaderBuf_[subEventHeaderQwsFilled_++] =
						*reinterpret_cast<const uint64_t*>(dmaBufferStartPtr + dmaBufferOffset);
					dmaBufferOffset += sizeof(uint64_t);
				}
				if (subEventHeaderQwsFilled_ < 2)
				{
					// Header straddles DMA buffer boundary; resume on next ReadBuffer
					DTC_TLOG(TLVL_GetData) << "GetSubEventData: only " << subEventHeaderQwsFilled_
										   << " header QW(s) filled, need 2 before sizing; breaking to fetch next buffer";
					break;
				}

				// QW0: per-sub-transfer framing prefix -> bits[0:23] = transfer byte count (24 bits)
				//      When == MAX_TRANSFER_SIZE - 1, the event is larger than one sub-transfer (multi-buffer event).
				// QW1: DTC_SubEventHeader -> bits[0:24] = inclusive_subevent_byte_count (25 bits)
				const size_t potentialEventTransferPrefix = static_cast<size_t>(subEventHeaderBuf_[0] & 0xFFFFFFULL);
				subEventByteCount_ = static_cast<size_t>(subEventHeaderBuf_[1] & 0x1FFFFFFULL);

				if (potentialEventTransferPrefix == MAX_TRANSFER_SIZE - 1)
				{
					DTC_TLOG(TLVL_GetData) << "GetSubEventData: multi-buffer event detected"
										   << " potentialEventTransferPrefix=0x" << std::hex << potentialEventTransferPrefix
										   << " == MAX_TRANSFER_SIZE-1=0x" << (MAX_TRANSFER_SIZE - 1) << std::dec
										   << " subEventByteCount_=" << subEventByteCount_
										   << " (event spans multiple DMA buffers)";
				}
				else if (potentialEventTransferPrefix != subEventByteCount_ + 8)
				{
					__SS__ << "GetSubEventData: Header sanity check FAILED: potentialEventTransferPrefix=" << potentialEventTransferPrefix << "(0x" << std::hex << potentialEventTransferPrefix << std::dec << ")"
						   << " subEventByteCount_=" << subEventByteCount_ << "(0x" << std::hex << subEventByteCount_ << std::dec << ")"
						   << " [expected potentialEventTransferPrefix == 0x" << std::hex << (MAX_TRANSFER_SIZE - 1) << std::dec << " (multi-buffer) or potentialEventTransferPrefix - 8 == subEventByteCount_ (single-buffer), diff="
						   << std::dec << (static_cast<int64_t>(potentialEventTransferPrefix) - 8 - static_cast<int64_t>(subEventByteCount_))
						   << "] at dmaBufferIndex #" << bufferIndexStr(dmaBufferStartPtr) << " dmaBufferOffset=" << dmaBufferOffset
						   << " payloadBytes=" << payloadBytes
						   << " last EWT=" << ewtTag() << " eventsParsedThisTime=" << eventsParsedThisTime
						   << " totalEventsParsed_=" << totalEventsParsed_;

					__SS_THROW__;
				}

				// currentEventSize_ = destination buffer size: struct EventHeader + full subevent
				currentEventSize_ = sizeof(DTC_EventHeader) + subEventByteCount_;
				DTC_TLOG(TLVL_GetData) << "GetSubEventData: sized event, potentialEventTransferPrefix=" << potentialEventTransferPrefix << "(0x" << std::hex << potentialEventTransferPrefix << std::dec << ")"
									   << " subEventByteCount_=" << subEventByteCount_ << "(0x" << std::hex << subEventByteCount_ << std::dec << ")"
									   << " currentEventSize_=" << currentEventSize_ << "(0x" << std::hex << currentEventSize_ << std::dec << ")"
									   << " at dmaBufferOffset=" << dmaBufferOffset;
			}

			// fill subevent header cache
			if (!needToFinishEvent_ && currentEventSize_ > 0 && subEventHeaderQwsFilled_ < kSubEventHeaderQws)
			{
				// We have a partially filled header from the previous buffer(s); try to fill the rest before doing anything else
				DTC_TLOG(TLVL_GetData) << "GetSubEventData: Continuing to fill subevent header, currently have "
									   << subEventHeaderQwsFilled_ << "/" << kSubEventHeaderQws << " QWs filled";
				while (subEventHeaderQwsFilled_ < kSubEventHeaderQws && (payloadBytes - dmaBufferOffset) >= sizeof(uint64_t))
				{
					subEventHeaderBuf_[subEventHeaderQwsFilled_++] =
						*reinterpret_cast<const uint64_t*>(dmaBufferStartPtr + dmaBufferOffset);
					dmaBufferOffset += sizeof(uint64_t);
					DTC_TLOG(TLVL_GetData) << "GetSubEventData: filled QW " << subEventHeaderQwsFilled_ << "/" << kSubEventHeaderQws;
				}
				if (subEventHeaderQwsFilled_ < kSubEventHeaderQws)
				{
					// Still don't have the full header; wait for the next buffer
					DTC_TLOG(TLVL_GetData) << "GetSubEventData: still only have " << subEventHeaderQwsFilled_
										   << " header QW(s) filled, need " << kSubEventHeaderQws
										   << "; breaking to fetch next buffer";
					break;
				}

				DTC_TLOG(TLVL_GetData) << "GetSubEventData: assembled subevent header: "
									   << reinterpret_cast<const DTC_SubEventHeader*>(subEventHeaderBuf_.data() + 1)->toJson();
			}  // end fill subevent header cache

			// start a new event and finish copying its subevent header
			if (!needToFinishEvent_ && currentEventSize_ > 0 && subEventHeaderQwsFilled_ == kSubEventHeaderQws)
			{
				extractedEvents_.push_back(std::make_shared<DTC_Event>(currentEventSize_));

				DTCLib::DTC_EventHeader evtHdr;
				evtHdr.inclusive_event_byte_count = currentEventSize_;
				evtHdr.num_dtcs = 1;
				evtHdr.event_tag_low = reinterpret_cast<const DTC_SubEventHeader*>(subEventHeaderBuf_.data() + 1)->event_tag_low;
				evtHdr.event_tag_high = reinterpret_cast<const DTC_SubEventHeader*>(subEventHeaderBuf_.data() + 1)->event_tag_high;
				uint8_t* eventBuf = static_cast<uint8_t*>(const_cast<void*>(extractedEvents_.back()->GetRawBufferPointer()));
				memcpy(eventBuf, &evtHdr, sizeof(DTCLib::DTC_EventHeader));
				*extractedEvents_.back()->GetHeader() = evtHdr;
				extractedSubeventBytes_ = 0;

				memcpy(eventBuf + sizeof(DTCLib::DTC_EventHeader) + extractedSubeventBytes_, subEventHeaderBuf_.data() + 1, sizeof(DTC_SubEventHeader));
				extractedSubeventBytes_ += sizeof(DTC_SubEventHeader);

				subEventHeaderQwsFilled_ = 0;
				needToFinishEvent_ = true;

				DTC_TLOG(TLVL_GetData) << "GetSubEventData: copied " << sizeof(DTC_SubEventHeader)
									   << "-byte subevent header from dmaBufferIndex #" << bufferIndexStr(dmaBufferStartPtr) << " into Event vector position #" << extractedEvents_.size()
									   << " EWT=" << extractedEvents_.back()->GetEventWindowTag() << "(0x" << std::hex << extractedEvents_.back()->GetEventWindowTag() << std::dec << ")"
									   << " (extractedSubeventBytes_=" << extractedSubeventBytes_
									   << "/" << subEventByteCount_ << ")";
			}

			if (needToFinishEvent_)
			{
				size_t bytesToCopy = std::min(payloadBytes - dmaBufferOffset, subEventByteCount_ - extractedSubeventBytes_);

				DTC_TLOG(TLVL_GetData) << "GetSubEventData: copying " << bytesToCopy << " bytes from dmaBufferIndex #" << bufferIndexStr(dmaBufferStartPtr) << " into Event vector position #" << extractedEvents_.size()
									   << " EWT=" << extractedEvents_.back()->GetEventWindowTag() << "(0x" << std::hex << extractedEvents_.back()->GetEventWindowTag() << std::dec << ")"
									   << " (before extractedSubeventBytes_=" << extractedSubeventBytes_
									   << "/" << subEventByteCount_ << ")";

				uint8_t* destPtr = static_cast<uint8_t*>(const_cast<void*>(extractedEvents_.back()->GetRawBufferPointer())) + sizeof(DTC_EventHeader) + extractedSubeventBytes_;
				memcpy(destPtr, dmaBufferStartPtr + dmaBufferOffset, bytesToCopy);
				dmaBufferOffset += bytesToCopy;
				extractedSubeventBytes_ += bytesToCopy;

				DTC_TLOG(TLVL_GetData) << "GetSubEventData: copied " << bytesToCopy
									   << "-bytes of subevent payload from dmaBufferIndex #" << bufferIndexStr(dmaBufferStartPtr) << " into Event vector position #" << extractedEvents_.size()
									   << " EWT=" << extractedEvents_.back()->GetEventWindowTag() << "(0x" << std::hex << extractedEvents_.back()->GetEventWindowTag() << std::dec << ")"
									   << " (extractedSubeventBytes_=" << extractedSubeventBytes_
									   << "/" << subEventByteCount_ << ")";

				if (extractedSubeventBytes_ == subEventByteCount_)
				{
					DTC_TLOG(TLVL_GetData) << "GetSubEventData: finished assembling Event vector position #" << extractedEvents_.size()
										   << " EWT=" << extractedEvents_.back()->GetEventWindowTag() << "(0x" << std::hex << extractedEvents_.back()->GetEventWindowTag() << std::dec << ")"
										   << " (total extractedSubeventBytes_=" << extractedSubeventBytes_
										   << "/" << subEventByteCount_ << ", eventBytes=" << currentEventSize_ << "); resetting for next event"
										   << " eventsParsedThisTime=" << eventsParsedThisTime
										   << " totalEventsParsed_=" << totalEventsParsed_;
					currentEventSize_ = 0;
					subEventByteCount_ = 0;
					extractedSubeventBytes_ = 0;
					needToFinishEvent_ = false;
					eventsParsedThisTime++;
					totalEventsParsed_++;
					subEventHeaderQwsFilled_ = 0;

					extractedEvents_.back()->SetupEvent();
				}
				else
				{
					DTC_TLOG(TLVL_GetData) << "GetSubEventData: still need to copy "
										   << (subEventByteCount_ - extractedSubeventBytes_)
										   << " bytes to finish Event vector position #" << extractedEvents_.size()
										   << " EWT=" << extractedEvents_.back()->GetEventWindowTag() << "(0x" << std::hex << extractedEvents_.back()->GetEventWindowTag() << std::dec << ")"
										   << "; breaking to fetch next buffer";
					break;
				}
			}
		}  // end buffer processing loop

		ReleaseBuffers(DTC_DMA_Engine_DAQ);

	} while (eventsParsedThisTime < vectorBundleTarget);
	// ------------------------------------------------------------------

	size_t completedCount = extractedEvents_.size() - (needToFinishEvent_ ? 1 : 0);
	DTC_TLOG(TLVL_GetData) << "GetSubEventData END [last EWT=" << ewtTag()
						   << "] eventsParsedThisTime=" << eventsParsedThisTime
						   << " totalEventsParsed_=" << totalEventsParsed_
						   << " completedCount=" << completedCount
						   << " needToFinishEvent_=" << (needToFinishEvent_ ? "true" : "false");

	std::vector<std::shared_ptr<DTC_Event>> result;
	result.reserve(completedCount);
	for (size_t i = 0; i < completedCount; ++i)
		result.push_back(extractedEvents_[i]);
	return result;
}  // end GetSubEventDataAsEvents()

// ---------------------------------------------------------------------------
// GetSubEventData v2 -- simplified, one-buffer-per-call subevent extractor
// ---------------------------------------------------------------------------
// Design rules:
//   • Each call reads exactly ONE new DMA buffer from hardware (or completes a
//     pending cross-buffer subevent from the previous call) then returns.
//   • All complete subevents found inside that buffer are returned immediately.
//   • If a subevent straddles the end of the buffer, we save the partial bytes
//     into pendingSubEventBytes_ and return.  On the next call we finish it
//     before touching the new buffer.
//   • We never hold more than 2 DMA buffers at once (the previous one, still
//     referenced by pending data, plus the new one).  As soon as we are done
//     with a buffer we release it via read_release.
// ---------------------------------------------------------------------------
std::vector<std::unique_ptr<DTCLib::DTC_SubEvent>> DTCLib::DTC::GetSubEventData(
	DTC_EventWindowTag when, bool matchEventWindowTag)
{
	(void)matchEventWindowTag;  // not yet used; filtering can be added once basic flow works
	std::vector<std::unique_ptr<DTC_SubEvent>> output;

	// ------------------------------------------------------------------
	// Step 1: get the next DMA buffer from hardware
	// ------------------------------------------------------------------
	DTC_TLOG(TLVL_GetData) << "GetSubEventData BEGIN EWT=" << when.GetEventWindowTag(true)
						   << " pendingBytes=" << pendingSubEventBytes_.size()
						   << " pendingTotal=" << pendingSubEventTotalBytes_
						   << " pendingPrefixConsumed=" << pendingPrefixConsumed_
						   << " daqBufs=" << daqDMAInfo_.buffer.size();

	// Release the previous buffer NOW — we are done with it.
	// (Pending bytes have already been copied out of it into pendingSubEventBytes_.)
	if (daqDMAInfo_.buffer.size() > 0)
	{
		DTC_TLOG(TLVL_GetData) << "GetSubEventData: releasing " << daqDMAInfo_.buffer.size() << " previous buffer(s)";
		ReleaseBuffers(DTC_DMA_Engine_DAQ);
	}

	// Read next buffer
	int sts;
	{
		int retry = 3;
		do {
			sts = ReadBuffer(DTC_DMA_Engine_DAQ, 100 /*ms*/);
		} while (sts <= 0 && --retry > 0);
	}
	if (sts <= 0)
	{
		DTC_TLOG(TLVL_GetData) << "GetSubEventData: ReadBuffer returned " << sts << ", no data";
		return output;
	}

	// sts = number of bytes the DMA engine transferred into this buffer
	const size_t dmaBytes = static_cast<size_t>(sts);

	// Track whether PREVIOUS buffer was full (for diagnostics) and update for this buffer.
	const bool prevBufferWasFull = lastDMABufferWasFull_;
	lastDMABufferWasFull_ = (dmaBytes == sizeof(mu2e_databuff_t));

	const uint8_t* bufStart = reinterpret_cast<const uint8_t*>(&daqDMAInfo_.buffer.back()[0]);
	// The DMA descriptor byte count (sts) is the number of bytes actually filled.
	// The last byte is the AXI tlast flag byte when the buffer is not completely full.
	// When it IS completely full (dmaBytes == sizeof(mu2e_databuff_t)) there is no tlast byte.
	const size_t payloadBytes = (dmaBytes < sizeof(mu2e_databuff_t)) ? dmaBytes - 1 : dmaBytes;

	DTC_TLOG(TLVL_GetData) << "GetSubEventData: new buffer bufStart=" << (void*)bufStart
						   << " dmaBytes=" << dmaBytes
						   << " payloadBytes=" << payloadBytes
						   << " sizeof(mu2e_databuff_t)=" << sizeof(mu2e_databuff_t);

	// Print first and last 8 quad-words of buffer for orientation / continuity checking
	if (TTEST(TLVL_GetData - TLVL_DEBUG))
	{
		{
			std::stringstream ss;
			// TLOG_SCOPED(TLVL_GetData) <<
			ss << "GetSubEventData: buffer first 8 qwords: ";
			for (int i = 0; i < 8 && (size_t)(i * 8) < payloadBytes; ++i)
				// TLOG_ADD
				ss << std::hex << std::setw(16) << std::setfill('0')
				   << *reinterpret_cast<const uint64_t*>(bufStart + i * 8) << " ";
			DTC_TLOG(TLVL_GetData) << ss.str();
		}

		{
			std::stringstream ss;
			// TLOG_SCOPED(TLVL_GetData) <<
			ss << "GetSubEventData: buffer last  8 qwords: ";
			const size_t lastStart = (payloadBytes >= 64) ? payloadBytes - 64 : 0;
			for (size_t i = lastStart; i + 8 <= payloadBytes; i += 8)
				// TLOG_ADD
				ss << std::hex << std::setw(16) << std::setfill('0')
				   << *reinterpret_cast<const uint64_t*>(bufStart + i) << " ";
			DTC_TLOG(TLVL_GetData) << ss.str();
		}
	}

	// Capture this buffer's tail (last up to 8 qwords) for the next call's diagnostic printout.
	// Called at every return path so lastBufferTailQwords_ always reflects the most recently
	// processed buffer by the time the next call enters Step 2.
	auto saveBufferTail = [&]() {
		const size_t tailStart = (payloadBytes >= 64) ? payloadBytes - 64 : 0;
		lastBufferTailCount_ = 0;
		for (size_t i = tailStart; i + 8 <= payloadBytes && lastBufferTailCount_ < 8; i += 8)
			lastBufferTailQwords_[lastBufferTailCount_++] =
				*reinterpret_cast<const uint64_t*>(bufStart + i);
	};

	// Returns the last good event window tag as a string, or "-1" if unavailable.
	auto ewtTag = [&]() -> std::string {
		if (!hasLastGoodSubEventHeader_) return "-1";
		return std::to_string(lastGoodSubEventHeader_.event_tag_low |
							  (static_cast<uint64_t>(lastGoodSubEventHeader_.event_tag_high) << 32));
	};

	// ------------------------------------------------------------------
	// Step 2: if we have a pending (cross-buffer) subevent, finish it
	//         using data from the new buffer before parsing the rest
	// ------------------------------------------------------------------
	size_t bufOffset = 0;  // our walk position within this buffer

	if (!pendingSubEventBytes_.empty() || pendingPrefixConsumed_)
	{
		// Prefix handling for continuation buffers:
		//
		// When the previous buffer was completely full (prevBufferWasFull == true),
		// the hardware split mid-sub-transfer.  This buffer is a raw continuation —
		// NO framing prefix at offset 0.  bufOffset = 0.
		//
		// When the previous buffer was NOT full (had tlast, prevBufferWasFull == false),
		// the sub-transfer completed.  This buffer starts a NEW sub-transfer with an
		// 8-byte per-sub-transfer byte count prefix.  bufOffset = 8.
		//
		// When pendingPrefixConsumed_ is true, the prefix was already consumed in the
		// previous buffer's Step 3.  This buffer starts at offset 0.

		// Use TLVL_Crossover only for the interesting cases (header/prefix crossover).
		const bool isHeaderOrPrefixCrossover = (pendingSubEventTotalBytes_ == 0) || pendingPrefixConsumed_;
		const auto crossoverLogLevel = isHeaderOrPrefixCrossover ? TLVL_Crossover : TLVL_GetData;

		if (pendingPrefixConsumed_)
		{
			// Prefix was already consumed in previous buffer's Step 3
			bufOffset = 0;
		}
		else if (prevBufferWasFull)
		{
			// Previous buffer was full — raw continuation, no prefix
			bufOffset = 0;
		}
		else
		{
			// Previous buffer was NOT full — new sub-transfer with 8-byte prefix
			if (payloadBytes < sizeof(uint64_t))
			{
				DTC_TLOG(TLVL_ERROR) << "GetSubEventData: continuation buffer too small ("
									 << payloadBytes << " bytes) to hold prefix -- dropping"
									 << " [last EWT=" << ewtTag() << " totalParsed=" << totalSubEventsParsed_ << "]";
				pendingSubEventBytes_.clear();
				pendingSubEventTotalBytes_ = 0;
				pendingPrefixConsumed_ = false;
				saveBufferTail();
				ReleaseBuffers(DTC_DMA_Engine_DAQ);
				return output;
			}
			bufOffset = sizeof(uint64_t);
		}

		DTC_TLOG(crossoverLogLevel) << "===== SUBEVENT CROSSOVER: continuation buffer ====="
									<< " pendingBytes=" << pendingSubEventBytes_.size()
									<< " pendingTotal=" << pendingSubEventTotalBytes_
									<< " pendingPrefixConsumed=" << pendingPrefixConsumed_
									<< " prevBufferWasFull=" << prevBufferWasFull
									<< " bufOffset=" << bufOffset
									<< " payloadBytes=" << payloadBytes
									<< " firstQword=0x" << std::hex
									<< *reinterpret_cast<const uint64_t*>(bufStart) << std::dec;
		// Dump first 8 qwords of continuation buffer so we can verify what's there
		{
			std::stringstream ss;
			ss << "===== CROSSOVER: continuation buffer first 8 qwords: ";
			for (int i = 0; i < 8 && (size_t)(i * 8) < payloadBytes; ++i)
				ss << std::hex << std::setw(16) << std::setfill('0')
				   << *reinterpret_cast<const uint64_t*>(bufStart + i * 8) << " ";
			DTC_TLOG(crossoverLogLevel) << ss.str();
		}

		pendingPrefixConsumed_ = false;  // consumed

		DTC_TLOG(TLVL_GetData) << "GetSubEventData: completing pending subevent"
							   << " have=" << pendingSubEventBytes_.size()
							   << " pendingTotal=" << pendingSubEventTotalBytes_;

		// If we stored a partial header last time (pendingSubEventTotalBytes_==0),
		// try to complete the header first using bytes from the new buffer.
		if (pendingSubEventTotalBytes_ == 0)
		{
			const size_t have = pendingSubEventBytes_.size();
			const size_t headerBytesNeeded = sizeof(DTC_SubEventHeader) - have;
			const size_t avail = payloadBytes - bufOffset;
			if (avail < headerBytesNeeded)
			{
				// Still not enough for header — absorb and wait
				DTC_TLOG(TLVL_GetData) << "GetSubEventData: still can't complete header,"
									   << " have=" << have << " need " << headerBytesNeeded
									   << " more but only " << avail << " available";
				pendingSubEventBytes_.insert(pendingSubEventBytes_.end(),
											 bufStart + bufOffset, bufStart + payloadBytes);
				saveBufferTail();
				ReleaseBuffers(DTC_DMA_Engine_DAQ);
				return output;
			}
			// Complete the header, extract total byte count
			pendingSubEventBytes_.insert(pendingSubEventBytes_.end(),
										 bufStart + bufOffset, bufStart + bufOffset + headerBytesNeeded);
			pendingSubEventTotalBytes_ = static_cast<size_t>(
				*reinterpret_cast<const uint32_t*>(pendingSubEventBytes_.data()) & 0x1FFFFFF);
			bufOffset += headerBytesNeeded;
			DTC_TLOG(TLVL_Crossover) << "===== CROSSOVER: header assembled from partial bytes ====="
									 << " subEventByteCount=" << pendingSubEventTotalBytes_
									 << " (0x" << std::hex << pendingSubEventTotalBytes_ << std::dec << ")"
									 << " bufOffset=" << bufOffset;
			// Dump assembled header for verification
			{
				std::stringstream ss;
				ss << "===== CROSSOVER: assembled header (" << pendingSubEventBytes_.size() << " bytes): ";
				for (size_t i = 0; i < pendingSubEventBytes_.size() && i < sizeof(DTC_SubEventHeader); i += 4)
					ss << std::hex << std::setw(8) << std::setfill('0')
					   << *reinterpret_cast<const uint32_t*>(&pendingSubEventBytes_[i]) << " ";
				// Show format version field location for quick visual check
				if (pendingSubEventBytes_.size() >= sizeof(DTC_SubEventHeader))
				{
					auto* hdr = reinterpret_cast<const DTC_SubEventHeader*>(pendingSubEventBytes_.data());
					ss << " | fmt_ver=" << hdr->subevent_format_version
					   << " num_rocs=" << hdr->num_rocs
					   << " tag=" << (hdr->event_tag_low | (static_cast<uint64_t>(hdr->event_tag_high) << 32));
				}
				DTC_TLOG(TLVL_Crossover) << ss.str();
			}

			// Sanity check: the inclusive byte count must be at least the header size
			if (pendingSubEventTotalBytes_ < sizeof(DTC_SubEventHeader))
			{
				DTC_TLOG(TLVL_ERROR) << "GetSubEventData: completed pending header has invalid"
									 << " subEventByteCount=" << pendingSubEventTotalBytes_
									 << " < sizeof(DTC_SubEventHeader)=" << sizeof(DTC_SubEventHeader)
									 << " -- discarding corrupt pending data"
									 << " [last EWT=" << ewtTag() << " totalParsed=" << totalSubEventsParsed_ << "]";
				pendingSubEventBytes_.clear();
				pendingSubEventTotalBytes_ = 0;
				// Fall through to Step 3 to continue parsing the rest of this buffer
			}
		}

		// If the sanity check above cleared pendingSubEventBytes_, skip assembly
		if (!pendingSubEventBytes_.empty())
		{
			const size_t have = pendingSubEventBytes_.size();
			const size_t need = pendingSubEventTotalBytes_;
			const size_t still = (need > have) ? need - have : 0;

			if (still > (payloadBytes - bufOffset))
			{
				// Still not enough — absorb rest of new buffer and keep pending
				const size_t absorb = payloadBytes - bufOffset;
				DTC_TLOG(TLVL_GetData) << "GetSubEventData: pending subevent still incomplete after new buffer"
									   << " still=" << still << " absorbing=" << absorb;
				pendingSubEventBytes_.insert(pendingSubEventBytes_.end(),
											 bufStart + bufOffset, bufStart + bufOffset + absorb);
				// Release this buffer immediately — data is copied
				saveBufferTail();
				ReleaseBuffers(DTC_DMA_Engine_DAQ);
				return output;  // come back next call
			}

			// We have enough — copy the remainder from this buffer
			pendingSubEventBytes_.insert(pendingSubEventBytes_.end(),
										 bufStart + bufOffset, bufStart + bufOffset + still);
			bufOffset += still;

			DTC_TLOG(crossoverLogLevel) << "===== CROSSOVER: pending subevent fully assembled ====="
										<< " totalBytes=" << need
										<< " (0x" << std::hex << need << std::dec << ")"
										<< " bufOffset now=" << bufOffset;
			// Dump assembled subevent header for final verification before construction
			{
				std::stringstream ss;
				ss << "===== CROSSOVER: assembled subevent header: ";
				size_t dumpLen = std::min(need, sizeof(DTC_SubEventHeader));
				for (size_t i = 0; i < dumpLen; i += 4)
					ss << std::hex << std::setw(8) << std::setfill('0')
					   << *reinterpret_cast<const uint32_t*>(&pendingSubEventBytes_[i]) << " ";
				if (need >= sizeof(DTC_SubEventHeader))
				{
					auto* hdr = reinterpret_cast<const DTC_SubEventHeader*>(pendingSubEventBytes_.data());
					ss << " | fmt_ver=" << hdr->subevent_format_version
					   << " num_rocs=" << hdr->num_rocs
					   << " tag=" << (hdr->event_tag_low | (static_cast<uint64_t>(hdr->event_tag_high) << 32));
				}
				DTC_TLOG(crossoverLogLevel) << ss.str();
			}

			// Snapshot assembled header for diagnostics on failure
			std::vector<uint8_t> assembledSnapshot(pendingSubEventBytes_.begin(),
												   pendingSubEventBytes_.begin() + std::min(pendingSubEventBytes_.size(), sizeof(DTC_SubEventHeader)));

			// Construct the completed subevent from the assembled bytes
			auto inmem = std::make_unique<DTC_SubEvent>(need);
			memcpy(const_cast<void*>(inmem->GetRawBufferPointer()),
				   pendingSubEventBytes_.data(), need);
			pendingSubEventBytes_.clear();
			pendingSubEventTotalBytes_ = 0;

			try
			{
				std::string errs;
				if (!inmem->SetupSubEvent(errs))
				{
					DTC_TLOG(TLVL_ERROR) << "GetSubEventData: corrupt pending subevent: " << errs << " [last EWT=" << ewtTag() << " totalParsed=" << totalSubEventsParsed_ << "]";
					// don't throw — log and continue parsing rest of buffer
				}
				else
				{
					// Track last good subevent header for diagnostics
					lastGoodSubEventHeader_ = *inmem->GetHeader();
					hasLastGoodSubEventHeader_ = true;
					++totalSubEventsParsed_;

					DTC_TLOG(TLVL_GetData) << "GetSubEventData: completed pending subevent tag="
										   << inmem->GetEventWindowTag().GetEventWindowTag(true);
					output.push_back(std::move(inmem));
				}
			}
			catch (...)
			{
				// Dump full diagnostic context before re-throwing
				DTC_TLOG(TLVL_ERROR) << "GetSubEventData: EXCEPTION setting up pending (cross-buffer) subevent!"
									 << " assembled totalBytes=" << need << " (0x" << std::hex << need << std::dec << ")"
									 << " prevBufferWasFull=" << prevBufferWasFull
									 << " pendingPrefixConsumed(was)=" << pendingPrefixConsumed_
									 << " bufOffset=" << bufOffset
									 << " payloadBytes=" << payloadBytes
									 << " firstQword=0x" << std::hex
									 << *reinterpret_cast<const uint64_t*>(bufStart) << std::dec << " [last EWT=" << ewtTag() << " totalParsed=" << totalSubEventsParsed_ << "]";
				// Dump assembled header bytes
				{
					std::stringstream ss;
					ss << "  Assembled header bytes: ";
					for (size_t i = 0; i < assembledSnapshot.size(); i += 4)
						ss << std::hex << std::setw(8) << std::setfill('0')
						   << *reinterpret_cast<const uint32_t*>(&assembledSnapshot[i]) << " ";
					DTC_TLOG(TLVL_ERROR) << ss.str() << " [last EWT=" << ewtTag() << " totalParsed=" << totalSubEventsParsed_ << "]";
				}
				// Dump last good subevent header if available
				if (hasLastGoodSubEventHeader_)
				{
					std::stringstream ss;
					ss << "  Last GOOD subevent header: ";
					auto* p = reinterpret_cast<const uint32_t*>(&lastGoodSubEventHeader_);
					for (size_t i = 0; i < sizeof(DTC_SubEventHeader) / 4; ++i)
						ss << std::hex << std::setw(8) << std::setfill('0') << p[i] << " ";
					ss << " | fmt_ver=" << lastGoodSubEventHeader_.subevent_format_version
					   << " num_rocs=" << lastGoodSubEventHeader_.num_rocs
					   << " tag=" << (lastGoodSubEventHeader_.event_tag_low | (static_cast<uint64_t>(lastGoodSubEventHeader_.event_tag_high) << 32));
					DTC_TLOG(TLVL_ERROR) << ss.str() << " [last EWT=" << ewtTag() << " totalParsed=" << totalSubEventsParsed_ << "]";
				}
				else
					DTC_TLOG(TLVL_ERROR) << "  No last good subevent header available";
				// Dump previous buffer tail (last up to 8 qwords before the crossover boundary)
				if (lastBufferTailCount_ > 0)
				{
					std::stringstream ss;
					ss << "  Previous buffer last " << lastBufferTailCount_ << " qwords: ";
					for (size_t i = 0; i < lastBufferTailCount_; ++i)
						ss << std::hex << std::setw(16) << std::setfill('0') << lastBufferTailQwords_[i] << " ";
					DTC_TLOG(TLVL_ERROR) << ss.str() << " [last EWT=" << ewtTag() << " totalParsed=" << totalSubEventsParsed_ << "]";
				}
				else
					DTC_TLOG(TLVL_ERROR) << "  No previous buffer tail available";
				// Dump continuation buffer context around the boundary
				{
					std::stringstream ss;
					ss << "  Continuation buffer first 16 qwords (raw): ";
					for (int i = 0; i < 16 && (size_t)(i * 8) < payloadBytes; ++i)
						ss << std::hex << std::setw(16) << std::setfill('0')
						   << *reinterpret_cast<const uint64_t*>(bufStart + i * 8) << " ";
					DTC_TLOG(TLVL_ERROR) << ss.str() << " [last EWT=" << ewtTag() << " totalParsed=" << totalSubEventsParsed_ << "]";
				}
				// Dump DMA buffers via spy for full hardware state
				DTC_TLOG(TLVL_ERROR) << "  Calling spy() for DMA buffer dump..."
									 << " [last EWT=" << ewtTag() << " totalParsed=" << totalSubEventsParsed_ << "]";
				device_.spy(DTC_DMA_Engine_DAQ, 3 /* for once */ | 8 /* for wide view */ | 16 /* stack trace */);
				// Re-throw; if it's a DTC_WrongPacketTypeException, augment with last good event tag
				try
				{
					throw;
				}
				catch (const DTC_WrongPacketTypeException& e)
				{
					std::string augMsg = std::string(e.what());
					if (hasLastGoodSubEventHeader_)
						augMsg += " [last good EWT=" + std::to_string(lastGoodSubEventHeader_.event_tag_low | (static_cast<uint64_t>(lastGoodSubEventHeader_.event_tag_high) << 32)) + "]";
					throw std::runtime_error(augMsg);
				}
				catch (...)
				{
					throw;
				}  // all other exception types: propagate unchanged
			}
		}  // end if pendingSubEventBytes_ not empty after header validation
	}

	// ------------------------------------------------------------------
	// Step 3: walk the rest of this buffer extracting complete subevents
	// ------------------------------------------------------------------
	// Buffer layout (repeating):
	//   [8 bytes]  DTC per-sub-transfer framing prefix  -- skipped, value NOT used
	//   [N bytes]  subevent: header (48 bytes) + ROC payload
	// Traversal is driven by two things only:
	//   1. payloadBytes (from sts) -- authoritative outer bound
	//   2. subevent header byte count (bits 24:0 of header word 0) -- advance within buffer

	while (bufOffset + sizeof(uint64_t) <= payloadBytes)
	{
		// Skip the 8-byte DTC framing prefix — do not use its value
		DTC_TLOG(TLVL_GetData) << "GetSubEventData: skipping 8-byte prefix @ bufOffset=" << bufOffset
							   << " prefixVal=" << *reinterpret_cast<const uint64_t*>(bufStart + bufOffset)
							   << " remaining=(" << payloadBytes - bufOffset << ")";
		bufOffset += sizeof(uint64_t);

		// The subevent inclusive byte count lives in the first 4 bytes of the header
		// (bits 24:0 = inclusive byte count incl. the header itself)
		const uint8_t* sePtr = bufStart + bufOffset;
		const size_t seAvail = payloadBytes - bufOffset;  // bytes remaining in this buffer

		// Print first 8 qwords at this subevent boundary for diagnostics
		if (TTEST(TLVL_GetData - TLVL_DEBUG))
		{
			std::stringstream ss;
			ss << "GetSubEventData: subevent boundary @ bufOffset=" << bufOffset
			   << " seAvail(dist to payload end)=" << seAvail
			   << " distToMaxBufEnd=" << (sizeof(mu2e_databuff_t) - bufOffset)
			   << " first 8 qwords: ";
			for (int i = 0; i < 8 && (size_t)(i * 8) < seAvail; ++i)
				ss << std::hex << std::setw(16) << std::setfill('0')
				   << *reinterpret_cast<const uint64_t*>(sePtr + i * 8) << " ";
			DTC_TLOG(TLVL_GetData) << ss.str();
		}

		size_t subEventByteCount = 0;
		if (seAvail >= sizeof(DTC_SubEventHeader))
		{
			subEventByteCount = static_cast<size_t>(
				*reinterpret_cast<const uint32_t*>(sePtr) & 0x1FFFFFF);

			// Consistency check: the 8-byte framing prefix we just skipped is an
			// inclusive byte count for this sub-transfer (prefix + header + payload),
			// while the first 4 bytes of the SubEvent header are the subevent inclusive
			// byte count (header + payload, NO prefix).  Therefore
			//     prefix_low_32 - subEventByteCount == 8
			// for any well-formed pair.  If this fails the parser is desynced and
			// continuing would absorb hundreds of KB of unrelated data into a bogus
			// "pending" subevent (see the 0x230023 incident on 2026-05-18).  Throw
			// immediately so the caller learns at first sight of corruption.
			const uint32_t prefixLow32 =
				*reinterpret_cast<const uint32_t*>(bufStart + bufOffset - sizeof(uint64_t));
			if (prefixLow32 < 8 || (prefixLow32 - 8) != subEventByteCount)
			{
				std::ostringstream oss;
				oss << "GetSubEventData: subevent header consistency check FAILED"
					<< " -- prefix_low32=0x" << std::hex << prefixLow32
					<< " subEventByteCount=0x" << subEventByteCount << std::dec
					<< " (expected prefix - byteCount == 8)"
					<< " at bufOffset=" << bufOffset
					<< " payloadBytes=" << payloadBytes
					<< " [last EWT=" << ewtTag()
					<< " totalParsed=" << totalSubEventsParsed_ << "]";
				DTC_TLOG(TLVL_ERROR) << oss.str();
				// Dump 16 qwords of context around the boundary so post-mortem is possible
				{
					std::stringstream ss;
					ss << "  Context (prefix-8 .. +16 qwords): ";
					const size_t ctxStart =
						(bufOffset >= sizeof(uint64_t)) ? bufOffset - sizeof(uint64_t) : 0;
					for (size_t i = ctxStart;
						 i + sizeof(uint64_t) <= payloadBytes && i < ctxStart + 17 * sizeof(uint64_t);
						 i += sizeof(uint64_t))
						ss << std::hex << std::setw(16) << std::setfill('0')
						   << *reinterpret_cast<const uint64_t*>(bufStart + i) << " ";
					DTC_TLOG(TLVL_ERROR) << ss.str();
				}
				// Dump buffer start (first 16 qwords) to check for the 0xdeadbeef
				// release_all marker at qword[0].
				{
					std::stringstream ss;
					ss << "  Buffer FIRST 16 qwords (from bufStart, check qword[0] for 0xdeadbeef): ";
					for (int i = 0; i < 16 && (size_t)(i * 8) < payloadBytes; ++i)
						ss << std::hex << std::setw(16) << std::setfill('0')
						   << *reinterpret_cast<const uint64_t*>(bufStart + i * 8) << " ";
					DTC_TLOG(TLVL_ERROR) << ss.str();
				}
				device_.spy(DTC_DMA_Engine_DAQ,
							3 /* once */ | 8 /* wide */ | 16 /* stack trace */);
				throw std::runtime_error(oss.str());
			}
		}
		else if (seAvail > 0)
		{
			// Partial header in this buffer — need to read from next buffer
			// We have fewer than 48 header bytes.  Store what we have and return.
			DTC_TLOG(TLVL_Crossover) << "===== SUBEVENT HEADER CROSSOVER DETECTED ====="
									 << " partial header at bufOffset=" << bufOffset
									 << " seAvail=" << seAvail << " < sizeof(DTC_SubEventHeader)=" << sizeof(DTC_SubEventHeader)
									 << " -- saving " << seAvail << " partial bytes, will complete on next buffer";
			// Dump the partial header bytes being cached
			{
				std::stringstream ss;
				ss << "===== CROSSOVER: caching partial header bytes: ";
				for (size_t i = 0; i < seAvail; i += 4)
				{
					if (i + 4 <= seAvail)
						ss << std::hex << std::setw(8) << std::setfill('0')
						   << *reinterpret_cast<const uint32_t*>(sePtr + i) << " ";
					else
						for (size_t j = i; j < seAvail; ++j)
							ss << std::hex << std::setw(2) << std::setfill('0')
							   << static_cast<unsigned>(sePtr[j]) << " ";
				}
				DTC_TLOG(TLVL_Crossover) << ss.str();
			}
			pendingSubEventBytes_.assign(sePtr, sePtr + seAvail);

			// If we have at least 4 bytes, we can extract and validate the byte count early
			if (seAvail >= sizeof(uint32_t))
			{
				pendingSubEventTotalBytes_ = static_cast<size_t>(
					*reinterpret_cast<const uint32_t*>(sePtr) & 0x1FFFFFF);
				if (pendingSubEventTotalBytes_ < sizeof(DTC_SubEventHeader))
				{
					DTC_TLOG(TLVL_ERROR) << "GetSubEventData: partial header has invalid"
										 << " subEventByteCount=" << pendingSubEventTotalBytes_
										 << " < sizeof(DTC_SubEventHeader)=" << sizeof(DTC_SubEventHeader)
										 << " -- discarding corrupt partial header"
										 << " [last EWT=" << ewtTag() << " totalParsed=" << totalSubEventsParsed_ << "]";
					pendingSubEventBytes_.clear();
					pendingSubEventTotalBytes_ = 0;
					break;
				}

				// Same prefix/byteCount consistency check as the full-header path:
				// prefix_low_32 - subEventByteCount must equal 8.  This is the only place
				// we get a chance to validate before storing pendingSubEventTotalBytes_
				// and letting Step 2 absorb potentially many KB of unrelated data on the
				// next calls.  Throw immediately if it fails.
				const uint32_t prefixLow32 =
					*reinterpret_cast<const uint32_t*>(bufStart + bufOffset - sizeof(uint64_t));
				if (prefixLow32 < 8 || (prefixLow32 - 8) != pendingSubEventTotalBytes_)
				{
					std::ostringstream oss;
					oss << "GetSubEventData: partial-header consistency check FAILED"
						<< " -- prefix_low32=0x" << std::hex << prefixLow32
						<< " pendingSubEventTotalBytes=0x" << pendingSubEventTotalBytes_ << std::dec
						<< " (expected prefix - byteCount == 8)"
						<< " at bufOffset=" << bufOffset
						<< " seAvail=" << seAvail
						<< " payloadBytes=" << payloadBytes
						<< " [last EWT=" << ewtTag()
						<< " totalParsed=" << totalSubEventsParsed_ << "]";
					DTC_TLOG(TLVL_ERROR) << oss.str();
					// Dump context around the boundary (prefix + whatever partial bytes we have)
					{
						std::stringstream ss;
						ss << "  Context (prefix-8 .. end-of-buffer, up to 17 qwords): ";
						const size_t ctxStart =
							(bufOffset >= sizeof(uint64_t)) ? bufOffset - sizeof(uint64_t) : 0;
						for (size_t i = ctxStart;
							 i + sizeof(uint64_t) <= payloadBytes && i < ctxStart + 17 * sizeof(uint64_t);
							 i += sizeof(uint64_t))
							ss << std::hex << std::setw(16) << std::setfill('0')
							   << *reinterpret_cast<const uint64_t*>(bufStart + i) << " ";
						DTC_TLOG(TLVL_ERROR) << ss.str();
					}
					// Dump buffer start (first 16 qwords) to check for the 0xdeadbeef
					// release_all marker at qword[0].
					{
						std::stringstream ss;
						ss << "  Buffer FIRST 16 qwords (from bufStart, check qword[0] for 0xdeadbeef): ";
						for (int i = 0; i < 16 && (size_t)(i * 8) < payloadBytes; ++i)
							ss << std::hex << std::setw(16) << std::setfill('0')
							   << *reinterpret_cast<const uint64_t*>(bufStart + i * 8) << " ";
						DTC_TLOG(TLVL_ERROR) << ss.str();
					}
					device_.spy(DTC_DMA_Engine_DAQ,
								3 /* once */ | 8 /* wide */ | 16 /* stack trace */);
					// Clear the pending state so we do not absorb anything on the next call
					pendingSubEventBytes_.clear();
					pendingSubEventTotalBytes_ = 0;
					throw std::runtime_error(oss.str());
				}

				DTC_TLOG(TLVL_GetData) << "GetSubEventData: partial header early byteCount="
									   << pendingSubEventTotalBytes_;
				// Note: pendingSubEventTotalBytes_ is set but header is still incomplete
				// (pendingSubEventBytes_.size() < sizeof(DTC_SubEventHeader)).
				// Step 2 on next call will complete the header before assembling payload.
			}
			else
			{
				// Fewer than 4 bytes — can't extract byte count yet
				pendingSubEventTotalBytes_ = 0;
			}
			break;  // done with this buffer
		}
		else
		{
			// Exactly 0 bytes remain after the prefix — subevent fully in next buffer.
			// The prefix was consumed here, so the next buffer is a continuation buffer
			// that starts with raw subevent header data at offset 0 (no prefix).
			// Mark this so the next call's Step 2 knows to treat it as a continuation.
			DTC_TLOG(TLVL_Crossover) << "===== SUBEVENT PREFIX-ONLY CROSSOVER DETECTED ====="
									 << " prefix at end of buffer, 0 subevent bytes here"
									 << " -- next buffer is a continuation with no prefix"
									 << " [last EWT=" << ewtTag() << " totalParsed=" << totalSubEventsParsed_ << "]";
			pendingSubEventBytes_.resize(0);  // empty but see note below
			pendingSubEventTotalBytes_ = 0;
			// Set pendingPrefixConsumed_ so the next call enters Step 2
			// (continuation path) instead of Step 3 (which would incorrectly skip a prefix).
			pendingPrefixConsumed_ = true;
			break;
		}

		if (subEventByteCount < sizeof(DTC_SubEventHeader))
		{
			DTC_TLOG(TLVL_ERROR) << "GetSubEventData: subEventByteCount=" << subEventByteCount
								 << " < sizeof(DTC_SubEventHeader)=" << sizeof(DTC_SubEventHeader)
								 << " at bufOffset=" << bufOffset << " -- stopping parse"
								 << " [last EWT=" << ewtTag() << " totalParsed=" << totalSubEventsParsed_ << "]";
			break;
		}

		DTC_TLOG(TLVL_GetData) << "GetSubEventData: subevent @ bufOffset=" << bufOffset
							   << " subEventByteCount=" << subEventByteCount
							   << " seAvail=" << seAvail;

		if (subEventByteCount <= seAvail)
		{
			// Entire subevent fits in this buffer — copy it out so the DMA buffer
			// can be safely released without use-after-free.  (The pointer-based
			// DTC_SubEvent(sePtr) constructor stores a raw pointer into the DMA
			// buffer, which becomes dangling after ReleaseBuffers.)
			try
			{
				auto res = std::make_unique<DTC_SubEvent>(subEventByteCount);
				memcpy(const_cast<void*>(res->GetRawBufferPointer()), sePtr, subEventByteCount);
				std::string errs;
				if (!res->SetupSubEvent(errs))
				{
					DTC_TLOG(TLVL_ERROR) << "GetSubEventData: corrupt subevent at bufOffset=" << bufOffset
										 << " errs=" << errs << " [last EWT=" << ewtTag() << " totalParsed=" << totalSubEventsParsed_ << "]";
				}
				else
				{
					// Track last good subevent header for diagnostics
					lastGoodSubEventHeader_ = *res->GetHeader();
					hasLastGoodSubEventHeader_ = true;
					++totalSubEventsParsed_;

					DTC_TLOG(TLVL_GetData) << "GetSubEventData: subevent OK tag="
										   << res->GetEventWindowTag().GetEventWindowTag(true)
										   << " bytes=" << subEventByteCount;
					output.push_back(std::move(res));
				}
			}
			catch (...)
			{
				DTC_TLOG(TLVL_ERROR) << "GetSubEventData: EXCEPTION constructing/setting up in-place subevent!"
									 << " bufOffset=" << bufOffset
									 << " subEventByteCount=" << subEventByteCount
									 << " (0x" << std::hex << subEventByteCount << std::dec << ")"
									 << " seAvail=" << seAvail
									 << " prevBufferWasFull=" << prevBufferWasFull
									 << " payloadBytes=" << payloadBytes
									 << " firstQword=0x" << std::hex
									 << *reinterpret_cast<const uint64_t*>(bufStart) << std::dec << " [last EWT=" << ewtTag() << " totalParsed=" << totalSubEventsParsed_ << "]";
				// Dump the subevent header bytes at this location
				{
					std::stringstream ss;
					ss << "  Subevent header bytes at bufOffset=" << bufOffset << ": ";
					size_t dumpLen = std::min(seAvail, sizeof(DTC_SubEventHeader));
					for (size_t i = 0; i < dumpLen; i += 4)
						ss << std::hex << std::setw(8) << std::setfill('0')
						   << *reinterpret_cast<const uint32_t*>(sePtr + i) << " ";
					DTC_TLOG(TLVL_ERROR) << ss.str() << " [last EWT=" << ewtTag() << " totalParsed=" << totalSubEventsParsed_ << "]";
				}
				// Dump context: 8 qwords before and 8 qwords after the subevent start
				{
					std::stringstream ss;
					ss << "  Context around bufOffset=" << bufOffset << ": BEFORE: ";
					size_t beforeStart = (bufOffset >= 64) ? bufOffset - 64 : 0;
					for (size_t i = beforeStart; i + 8 <= bufOffset; i += 8)
						ss << std::hex << std::setw(16) << std::setfill('0')
						   << *reinterpret_cast<const uint64_t*>(bufStart + i) << " ";
					ss << " | AFTER: ";
					for (size_t i = bufOffset; i + 8 <= payloadBytes && i < bufOffset + 64; i += 8)
						ss << std::hex << std::setw(16) << std::setfill('0')
						   << *reinterpret_cast<const uint64_t*>(bufStart + i) << " ";
					DTC_TLOG(TLVL_ERROR) << ss.str() << " [last EWT=" << ewtTag() << " totalParsed=" << totalSubEventsParsed_ << "]";
				}
				// Dump buffer start (first 16 qwords) so we can check for the 0xdeadbeef
				// release_all marker at qword[0] -- if it's still there, hardware never
				// wrote this buffer since the last ReleaseAllBuffers.
				{
					std::stringstream ss;
					ss << "  Buffer FIRST 16 qwords (from bufStart, check qword[0] for 0xdeadbeef): ";
					for (int i = 0; i < 16 && (size_t)(i * 8) < payloadBytes; ++i)
						ss << std::hex << std::setw(16) << std::setfill('0')
						   << *reinterpret_cast<const uint64_t*>(bufStart + i * 8) << " ";
					DTC_TLOG(TLVL_ERROR) << ss.str() << " [last EWT=" << ewtTag() << " totalParsed=" << totalSubEventsParsed_ << "]";
				}
				// Dump last good subevent header if available
				if (hasLastGoodSubEventHeader_)
				{
					std::stringstream ss;
					ss << "  Last GOOD subevent header: ";
					auto* p = reinterpret_cast<const uint32_t*>(&lastGoodSubEventHeader_);
					for (size_t i = 0; i < sizeof(DTC_SubEventHeader) / 4; ++i)
						ss << std::hex << std::setw(8) << std::setfill('0') << p[i] << " ";
					ss << " | fmt_ver=" << lastGoodSubEventHeader_.subevent_format_version
					   << " num_rocs=" << lastGoodSubEventHeader_.num_rocs
					   << " tag=" << (lastGoodSubEventHeader_.event_tag_low | (static_cast<uint64_t>(lastGoodSubEventHeader_.event_tag_high) << 32));
					DTC_TLOG(TLVL_ERROR) << ss.str() << " [last EWT=" << ewtTag() << " totalParsed=" << totalSubEventsParsed_ << "]";
				}
				else
					DTC_TLOG(TLVL_ERROR) << "  No last good subevent header available";
				// Dump previous buffer tail (last up to 8 qwords from the buffer before this one)
				if (lastBufferTailCount_ > 0)
				{
					std::stringstream ss;
					ss << "  Previous buffer last " << lastBufferTailCount_ << " qwords: ";
					for (size_t i = 0; i < lastBufferTailCount_; ++i)
						ss << std::hex << std::setw(16) << std::setfill('0') << lastBufferTailQwords_[i] << " ";
					DTC_TLOG(TLVL_ERROR) << ss.str() << " [last EWT=" << ewtTag() << " totalParsed=" << totalSubEventsParsed_ << "]";
				}
				else
					DTC_TLOG(TLVL_ERROR) << "  No previous buffer tail available";
				// Dump DMA buffers via spy for full hardware state
				DTC_TLOG(TLVL_ERROR) << "  Calling spy() for DMA buffer dump..."
									 << " [last EWT=" << ewtTag() << " totalParsed=" << totalSubEventsParsed_ << "]";
				device_.spy(DTC_DMA_Engine_DAQ, 3 /* for once */ | 8 /* for wide view */ | 16 /* stack trace */);
				// Re-throw; if it's a DTC_WrongPacketTypeException, augment with last good event tag
				try
				{
					throw;
				}
				catch (const DTC_WrongPacketTypeException& e)
				{
					std::string augMsg = std::string(e.what());
					if (hasLastGoodSubEventHeader_)
						augMsg += " [last good EWT=" + std::to_string(lastGoodSubEventHeader_.event_tag_low | (static_cast<uint64_t>(lastGoodSubEventHeader_.event_tag_high) << 32)) + "]";
					throw std::runtime_error(augMsg);
				}
				catch (...)
				{
					throw;
				}  // all other exception types: propagate unchanged
			}
			bufOffset += subEventByteCount;
		}
		else
		{
			// Subevent crosses buffer boundary — save what we have and return
			DTC_TLOG(TLVL_GetData) << "===== SUBEVENT BODY CROSSOVER DETECTED ====="
								   << " have=" << seAvail
								   << " need=" << subEventByteCount
								   << " (0x" << std::hex << subEventByteCount << std::dec << ")"
								   << " missing=" << (subEventByteCount - seAvail)
								   << " -- saving partial subevent, will continue on next buffer";
			pendingSubEventBytes_.assign(sePtr, sePtr + seAvail);
			pendingSubEventTotalBytes_ = subEventByteCount;
			break;  // done with this buffer
		}
	}

	// ------------------------------------------------------------------
	// Step 3b: check for leftover bytes that didn't fit in the while-loop
	// ------------------------------------------------------------------
	// The while-loop requires at least 8 bytes (sizeof(uint64_t)) to enter,
	// since every subevent is preceded by an 8-byte framing prefix.  If fewer
	// than 8 bytes remain after the last complete subevent (or partial-header
	// caching), they would otherwise be silently dropped.
	if (pendingSubEventBytes_.empty() && !pendingPrefixConsumed_ && bufOffset < payloadBytes)
	{
		const size_t leftover = payloadBytes - bufOffset;
		// For non-full buffers, leftover bytes < 8 are most likely a partial
		// framing prefix or alignment padding — the next buffer starts fresh.
		// Log at WARNING level so this is visible in diagnostics.
		DTC_TLOG(TLVL_WARNING) << "GetSubEventData: " << leftover
							   << " leftover byte(s) at end of buffer (bufOffset=" << bufOffset
							   << " payloadBytes=" << payloadBytes
							   << " bufferFull=" << lastDMABufferWasFull_ << ")"
							   << " [last EWT=" << ewtTag() << " totalParsed=" << totalSubEventsParsed_ << "]";
		// Dump the first 8 qwords of the leftover region so we can see what stopped the parse
		{
			std::stringstream ss;
			ss << "GetSubEventData: leftover region first 8 qwords @ bufOffset=" << bufOffset << ": ";
			for (int i = 0; i < 8 && bufOffset + (size_t)(i * 8) + 8 <= payloadBytes; ++i)
				ss << std::hex << std::setw(16) << std::setfill('0')
				   << *reinterpret_cast<const uint64_t*>(bufStart + bufOffset + i * 8) << " ";
			// Also show the raw 4-byte value that was (or would be) parsed as subEventByteCount
			if (leftover >= sizeof(uint32_t))
				ss << " | raw_byteCount_field=0x" << std::hex
				   << (*reinterpret_cast<const uint32_t*>(bufStart + bufOffset) & 0x1FFFFFF)
				   << std::dec;
			DTC_TLOG(TLVL_WARNING) << ss.str();
		}
	}

	// ------------------------------------------------------------------
	// Step 4: report and return
	// ------------------------------------------------------------------
	DTC_TLOG(TLVL_GetData) << "GetSubEventData RETURN output.size()=" << output.size()
						   << " pendingBytes=" << pendingSubEventBytes_.size()
						   << " pendingTotal=" << pendingSubEventTotalBytes_
						   << " pendingPrefixConsumed=" << pendingPrefixConsumed_;
	// Always refresh lastGoodSubEventHeader_ from the last successfully returned subevent
	// so the member is current even if the inline saves were skipped (e.g. early-exit paths).
	if (!output.empty() && output.back() && output.back()->GetHeader())
	{
		lastGoodSubEventHeader_ = *output.back()->GetHeader();
		hasLastGoodSubEventHeader_ = true;
	}
	saveBufferTail();
	return output;
}  // endGetSubEventData() v2

void DTCLib::DTC::WriteSimFileToDTC(std::string file, bool /*goForever*/, bool overwriteEnvironment,
									std::string outputFileName, bool skipVerify)
{
	bool success = false;
	int retryCount = 0;
	while (!success && retryCount < 5)
	{
		DTC_TLOG(TLVL_WriteSimFileToDTC) << "WriteSimFileToDTC BEGIN";
		auto writeOutput = outputFileName != "";
		std::ofstream outputStream;
		if (writeOutput)
		{
			outputStream.open(outputFileName, std::ios::out | std::ios::binary);
		}
		auto sim = getenv("DTCLIB_SIM_FILE");
		if (!overwriteEnvironment && sim != nullptr)
		{
			file = std::string(sim);
		}

		DTC_TLOG(TLVL_WriteSimFileToDTC) << "WriteSimFileToDTC file is " << file << ", Setting up DTC";
		DisableDetectorEmulator();
		DisableDetectorEmulatorMode();
		// ResetDDR();  // this can take about a second
		ResetDDRWriteAddress();
		ResetDDRReadAddress();
		SetDDRDataLocalStartAddress(0x0);
		SetDDRDataLocalEndAddress(0xFFFFFFFF);
		EnableDetectorEmulatorMode();
		SetDetectorEmulationDMACount(1);
		SetDetectorEmulationDMADelayCount(250);  // 1 microseconds
		uint64_t totalSize = 0;
		auto n = 0;

		auto sizeCheck = true;
		DTC_TLOG(TLVL_WriteSimFileToDTC) << "WriteSimFileToDTC Opening file";
		std::ifstream is(file, std::ifstream::binary);
		DTC_TLOG(TLVL_WriteSimFileToDTC) << "WriteSimFileToDTC Reading file";
		while (is && is.good() && sizeCheck)
		{
			DTC_TLOG(TLVL_WriteSimFileToDTC2) << "WriteSimFileToDTC Reading a DMA from file..." << file;
			auto buf = reinterpret_cast<mu2e_databuff_t*>(new char[0x10000]);
			is.read(reinterpret_cast<char*>(buf), sizeof(uint64_t));
			if (is.eof())
			{
				DTC_TLOG(TLVL_WriteSimFileToDTC2) << "WriteSimFileToDTC End of file reached.";
				delete[] buf;
				break;
			}
			auto sz = *reinterpret_cast<uint64_t*>(buf);
			is.read(reinterpret_cast<char*>(buf) + 8, sz - sizeof(uint64_t));
			if (sz < 80 && sz > 0)
			{
				auto oldSize = sz;
				sz = 80;
				memcpy(buf, &sz, sizeof(uint64_t));
				uint64_t sixtyFour = 64;
				memcpy(reinterpret_cast<uint64_t*>(buf) + 1, &sixtyFour, sizeof(uint64_t));
				bzero(reinterpret_cast<uint64_t*>(buf) + 2, sz - oldSize);
			}
			// is.read((char*)buf + 8, sz - sizeof(uint64_t));
			if (sz > 0 && (sz + totalSize < 0xFFFFFFFF || simMode_ == DTC_SimMode_LargeFile))
			{
				DTC_TLOG(TLVL_WriteSimFileToDTC2) << "WriteSimFileToDTC Size is " << sz << ", writing to device";
				if (writeOutput)
				{
					DTC_TLOG(TLVL_WriteSimFileToDTC3)
						<< "WriteSimFileToDTC: Stripping off DMA header words and writing to binary file";
					outputStream.write(reinterpret_cast<char*>(buf) + 16, sz - 16);
				}

				auto dmaByteCount = *(reinterpret_cast<uint64_t*>(buf) + 1);
				DTC_TLOG(TLVL_WriteSimFileToDTC3) << "WriteSimFileToDTC: Inclusive write byte count: " << sz
												  << ", DMA Byte count: " << dmaByteCount;
				if (sz - 8 != dmaByteCount)
				{
					DTC_TLOG(TLVL_ERROR) << "WriteSimFileToDTC: ERROR: Inclusive write Byte count " << sz
										 << " is inconsistent with DMA byte count " << dmaByteCount << " for DMA at 0x"
										 << std::hex << totalSize << " (0x" << sz - 16 << " != 0x" << dmaByteCount << ")";
					sizeCheck = false;
				}

				totalSize += sz - 8;
				n++;
				DTC_TLOG(TLVL_WriteSimFileToDTC3) << "WriteSimFileToDTC: totalSize is now " << totalSize << ", n is now " << n;
				WriteDetectorEmulatorData(buf, static_cast<size_t>(sz));
			}
			else if (sz > 0)
			{
				DTC_TLOG(TLVL_WriteSimFileToDTC2) << "WriteSimFileToDTC DTC memory is now full. Closing file.";
				sizeCheck = false;
			}
			delete[] buf;
		}

		DTC_TLOG(TLVL_WriteSimFileToDTC) << "WriteSimFileToDTC Closing file. sizecheck=" << sizeCheck << ", eof=" << is.eof()
										 << ", fail=" << is.fail() << ", bad=" << is.bad();
		is.close();
		if (writeOutput) outputStream.close();
		SetDDRDataLocalEndAddress(static_cast<uint32_t>(totalSize - 1));
		success = skipVerify || VerifySimFileInDTC(file, outputFileName);
		retryCount++;
	}

	if (retryCount == 5)
	{
		DTC_TLOG(TLVL_ERROR) << "WriteSimFileToDTC FAILED after 5 attempts! ABORTING!";
		exit(4);
	}
	else
	{
		__COUT_INFO__ << "WriteSimFileToDTC Took " << retryCount << " attempts to write file";
	}

	SetDetectorEmulatorInUse();
	DTC_TLOG(TLVL_WriteSimFileToDTC) << "WriteSimFileToDTC END";
}

bool DTCLib::DTC::VerifySimFileInDTC(std::string file, std::string rawOutputFilename)
{
	uint64_t totalSize = 0;
	auto n = 0;
	auto sizeCheck = true;

	auto writeOutput = rawOutputFilename != "";
	std::ofstream outputStream;
	if (writeOutput)
	{
		outputStream.open(rawOutputFilename + ".verify", std::ios::out | std::ios::binary);
	}

	auto sim = getenv("DTCLIB_SIM_FILE");
	if (file.size() == 0 && sim != nullptr)
	{
		file = std::string(sim);
	}

	ResetDDRReadAddress();
	DTC_TLOG(TLVL_VerifySimFileInDTC) << "VerifySimFileInDTC Opening file";
	std::ifstream is(file, std::ifstream::binary);
	if (!is || !is.good())
	{
		DTC_TLOG(TLVL_ERROR) << "VerifySimFileInDTC Failed to open file " << file << "!";
	}

	DTC_TLOG(TLVL_VerifySimFileInDTC) << "VerifySimFileInDTC Reading file";
	while (is && is.good() && sizeCheck)
	{
		DTC_TLOG(TLVL_VerifySimFileInDTC2) << "VerifySimFileInDTC Reading a DMA from file..." << file;
		uint64_t file_buffer_size;
		auto buffer_from_file = reinterpret_cast<mu2e_databuff_t*>(new char[0x10000]);
		is.read(reinterpret_cast<char*>(&file_buffer_size), sizeof(uint64_t));
		if (is.eof())
		{
			DTC_TLOG(TLVL_VerifySimFileInDTC2) << "VerifySimFileInDTC End of file reached.";
			delete[] buffer_from_file;
			break;
		}
		is.read(reinterpret_cast<char*>(buffer_from_file), file_buffer_size - sizeof(uint64_t));
		if (file_buffer_size < 80 && file_buffer_size > 0)
		{
			// auto oldSize = file_buffer_size;
			file_buffer_size = 80;
			uint64_t sixtyFour = 64;
			memcpy(reinterpret_cast<uint64_t*>(buffer_from_file), &sixtyFour, sizeof(uint64_t));
			// bzero(reinterpret_cast<uint64_t*>(buffer_from_file) + 2, sz - oldSize);
		}

		if (file_buffer_size > 0 && (file_buffer_size + totalSize < 0xFFFFFFFF || simMode_ == DTC_SimMode_LargeFile))
		{
			DTC_TLOG(TLVL_VerifySimFileInDTC2) << "VerifySimFileInDTC Expected Size is " << file_buffer_size - sizeof(uint64_t) << ", reading from device";
			auto inclusiveByteCount = *(reinterpret_cast<uint64_t*>(buffer_from_file));
			DTC_TLOG(TLVL_VerifySimFileInDTC3) << "VerifySimFileInDTC: DMA Write size: " << file_buffer_size
											   << ", Inclusive byte count: " << inclusiveByteCount;
			if (file_buffer_size - 8 != inclusiveByteCount)
			{
				DTC_TLOG(TLVL_ERROR) << "VerifySimFileInDTC: ERROR: DMA Write size " << file_buffer_size
									 << " is inconsistent with DMA byte count " << inclusiveByteCount << " for DMA at 0x"
									 << std::hex << totalSize << " (0x" << file_buffer_size - 8 << " != 0x" << inclusiveByteCount << ")";
				sizeCheck = false;
			}

			totalSize += file_buffer_size;
			n++;
			DTC_TLOG(TLVL_VerifySimFileInDTC3) << "VerifySimFileInDTC: totalSize is now " << totalSize << ", n is now " << n;
			// WriteDetectorEmulatorData(buffer_from_file, static_cast<size_t>(sz));
			DisableDetectorEmulator();
			SetDetectorEmulationDMACount(1);
			EnableDetectorEmulator();

			mu2e_databuff_t* buffer_from_device;
			auto tmo_ms = 1500;
			DTC_TLOG(TLVL_VerifySimFileInDTC) << "VerifySimFileInDTC - before read for DAQ ";
			auto sts = device_.read_data(DTC_DMA_Engine_DAQ, reinterpret_cast<void**>(&buffer_from_device), tmo_ms);
			if (writeOutput && sts > 8)
			{
				DTC_TLOG(TLVL_VerifySimFileInDTC3) << "VerifySimFileInDTC: Writing to binary file";
				outputStream.write(reinterpret_cast<char*>(*buffer_from_device), sts);
			}

			if (sts == 0)
			{
				DTC_TLOG(TLVL_ERROR) << "VerifySimFileInDTC Error reading buffer " << n << ", aborting!";
				delete[] buffer_from_file;
				is.close();
				if (writeOutput) outputStream.close();
				return false;
			}
			size_t readSz = *(reinterpret_cast<uint64_t*>(buffer_from_device));
			DTC_TLOG(TLVL_VerifySimFileInDTC) << "VerifySimFileInDTC - after read, bc=" << inclusiveByteCount << " sts=" << sts
											  << " rdSz=" << readSz;

			// DMA engine strips off leading 64-bit word
			DTC_TLOG(TLVL_VerifySimFileInDTC3) << "VerifySimFileInDTC - Checking buffer size";
			if (static_cast<size_t>(sts) != inclusiveByteCount)
			{
				DTC_TLOG(TLVL_ERROR) << "VerifySimFileInDTC Buffer " << n << " has size 0x" << std::hex << sts
									 << " but the input file has size 0x" << std::hex << inclusiveByteCount
									 << " for that buffer!";

				device_.read_release(DTC_DMA_Engine_DAQ, 1);
				delete[] buffer_from_file;
				is.close();
				if (writeOutput) outputStream.close();
				return false;
			}

			DTC_TLOG(TLVL_VerifySimFileInDTC2) << "VerifySimFileInDTC - Checking buffer contents";
			size_t cnt = sts % sizeof(uint64_t) == 0 ? sts / sizeof(uint64_t) : 1 + (sts / sizeof(uint64_t));

			for (size_t ii = 0; ii < cnt; ++ii)
			{
				auto l = *(reinterpret_cast<uint64_t*>(*buffer_from_device) + ii);
				auto r = *(reinterpret_cast<uint64_t*>(*buffer_from_file) + ii);
				if (l != r)
				{
					size_t address = totalSize - file_buffer_size + ((ii + 1) * sizeof(uint64_t));
					DTC_TLOG(TLVL_ERROR) << "VerifySimFileInDTC Buffer " << n << " word " << ii << " (Address in file 0x" << std::hex
										 << address << "):"
										 << " Expected 0x" << std::hex << r << ", but got 0x" << std::hex << l
										 << ". Returning False!";
					DTC_TLOG(TLVL_VerifySimFileInDTC3) << "VerifySimFileInDTC Next words: "
														  "Expected 0x"
													   << std::hex << *(reinterpret_cast<uint64_t*>(*buffer_from_file) + ii + 1) << ", "
																																	"DTC: 0x"
													   << std::hex << *(reinterpret_cast<uint64_t*>(*buffer_from_device) + ii + 1);
					delete[] buffer_from_file;
					is.close();
					if (writeOutput) outputStream.close();
					device_.read_release(DTC_DMA_Engine_DAQ, 1);
					return false;
				}
			}
			device_.read_release(DTC_DMA_Engine_DAQ, 1);
		}
		else if (file_buffer_size > 0)
		{
			DTC_TLOG(TLVL_VerifySimFileInDTC2) << "VerifySimFileInDTC DTC memory is now full. Closing file.";
			sizeCheck = false;
		}
		delete[] buffer_from_file;
	}

	DTC_TLOG(TLVL_VerifySimFileInDTC) << "VerifySimFileInDTC Closing file. sizecheck=" << sizeCheck << ", eof=" << is.eof()
									  << ", fail=" << is.fail() << ", bad=" << is.bad();
	__COUT_INFO__ << "VerifySimFileInDTC: The Detector Emulation file was written correctly";
	is.close();
	if (writeOutput) outputStream.close();
	return true;
}

// ROC Register Functions
uint16_t DTCLib::DTC::ReadROCRegister(const DTC_Link_ID& link, const uint16_t address, int tmo_ms)
{
	uint16_t retries = 0;  // change to 1 to attempt reinitializing
	do
	{
		dcsDMAInfo_.currentReadPtr = nullptr;

		device_.begin_dcs_transaction();
		ReleaseAllBuffers(DTC_DMA_Engine_DCS);
		SendDCSRequestPacket(link, DTC_DCSOperationType_Read, address,
							 0x0 /*data*/, 0x0 /*address2*/, 0x0 /*data2*/,
							 false /*quiet*/);

		uint16_t data = 0xFFFF;

		try
		{
			auto reply = ReadNextDCSPacket(tmo_ms);
			device_.end_dcs_transaction();

			if (reply != nullptr)  // have data!
			{
				auto replytmp = reply->GetReply(false);
				auto linktmp = reply->GetLinkID();
				data = replytmp.second;

				DTC_TLOG(TLVL_TRACE) << "Got packet, "
									 << "link=" << static_cast<int>(linktmp) << " (expected " << static_cast<int>(link) << "), "
									 << "address=" << static_cast<int>(replytmp.first) << " (expected " << static_cast<int>(address)
									 << "), "
									 << "data=" << data;
				if (linktmp == link && replytmp.first == address)
					return data;
				else
				{
					__SS__ << "Mismatch identified in link=" << linktmp << " != " << link << " or "
						   << "address=" << replytmp.first << " != " << address << ". Corrupt ROC response?" << __E__;
					ss << "\n\nIf interpreting as a DTC_DCSReplyPacket, here is the data: \n"
					   << reply->toJSON();
					__SS_THROW__;
				}
			}
		}
		catch (const std::exception& e)
		{
			__SS__ << "Failure attempting to read a ROC register at link " << static_cast<int>(link) << " address 0x" << std::hex << static_cast<int>(address) << ". Exception caught: " << e.what() << __E__;
			__SS_THROW__;
		}

		// if here then software received no response from DTC, try a software re-init to realign DMA pointers
		if (retries)  // do not reinit on last try
		{
			__COUT__ << "Software received no response to the DCS request from the DTC, trying a DMA re-init. retries = " << retries << __E__;
			device_.initDMAEngine();
		}

	} while (retries--);

	// throw exception for no data after retries
	__SS__ << "A timeout occurred attempting to read a ROC register at link " << static_cast<int>(link) << " address 0x" << std::hex << static_cast<int>(address) << ". No DCS reply packet received after " << std::dec << tmo_ms << " ms! "
		   << "Check the clocks and that the ROC link is enabled and locked. Restarting the DTC software instance may fix the problem and realign DMA pointers." << std::endl;

	if (TTEST(20))
	{
		__COUT_ERR__ << "\n"
					 << ss.str();
		device_.spy(DTC_DMA_Engine_DCS, 3 /* for once */ | 8 /* for wide view */);
		__COUT_ERR__ << otsStyleStackTrace();
	}

	__SS_THROW__;
}  // end ReadROCRegister()

bool DTCLib::DTC::WriteROCRegister(const DTC_Link_ID& link, const uint16_t address, const uint16_t data, bool requestAck, int ack_tmo_ms)
{
	device_.begin_dcs_transaction();
	if (requestAck)
	{
		dcsDMAInfo_.currentReadPtr = nullptr;
		ReleaseAllBuffers(DTC_DMA_Engine_DCS);
	}
	SendDCSRequestPacket(link, DTC_DCSOperationType_Write, address, data,
						 0x0 /*address2*/, 0x0 /*data2*/,
						 false /*quiet*/, requestAck);

	bool ackReceived = false;
	if (requestAck)
	{
		DTC_TLOG(TLVL_TRACE) << "WriteROCRegister: Checking for ack";
		auto reply = ReadNextDCSPacket(ack_tmo_ms);
		while (reply != nullptr)
		{
			auto reply1tmp = reply->GetReply(false);
			auto linktmp = reply->GetLinkID();
			DTC_TLOG(TLVL_TRACE) << "Got packet, "
								 << "link=" << static_cast<int>(linktmp) << " (expected " << static_cast<int>(link) << "), "
								 << "address1=" << static_cast<int>(reply1tmp.first) << " (expected "
								 << static_cast<int>(address) << "), "
								 << "data1=" << static_cast<int>(reply1tmp.second);

			reply.reset(nullptr);
			if (reply1tmp.first != address || linktmp != link || !reply->IsAckRequested())
			{
				DTC_TLOG(TLVL_TRACE) << "Address or link did not match, or ack bit was not set, reading next packet!";
				reply = ReadNextDCSPacket(ack_tmo_ms);  // Read the next packet
			}
			else
			{
				ackReceived = true;
			}
		}
	}
	device_.end_dcs_transaction();
	return !requestAck || ackReceived;
}

std::pair<uint16_t, uint16_t> DTCLib::DTC::ReadROCRegisters(const DTC_Link_ID& link, const uint16_t address1,
															const uint16_t address2, int tmo_ms)
{
	dcsDMAInfo_.currentReadPtr = nullptr;

	device_.begin_dcs_transaction();
	ReleaseAllBuffers(DTC_DMA_Engine_DCS);
	SendDCSRequestPacket(link, DTC_DCSOperationType_Read, address1, 0, address2);
	usleep(2500);
	uint16_t data1 = 0xFFFF;
	uint16_t data2 = 0xFFFF;

	auto reply = ReadNextDCSPacket(tmo_ms);

	while (reply != nullptr)
	{
		auto reply1tmp = reply->GetReply(false);
		auto reply2tmp = reply->GetReply(true);
		auto linktmp = reply->GetLinkID();
		DTC_TLOG(TLVL_TRACE) << "Got packet, "
							 << "link=" << static_cast<int>(linktmp) << " (expected " << static_cast<int>(link) << "), "
							 << "address1=" << static_cast<int>(reply1tmp.first) << " (expected "
							 << static_cast<int>(address1) << "), "
							 << "data1=" << static_cast<int>(reply1tmp.second)
							 << "address2=" << static_cast<int>(reply2tmp.first) << " (expected "
							 << static_cast<int>(address2) << "), "
							 << "data2=" << static_cast<int>(reply2tmp.second);

		reply.reset(nullptr);
		if (reply1tmp.first != address1 || reply2tmp.first != address2 || linktmp != link)
		{
			DTC_TLOG(TLVL_TRACE) << "Address or link did not match, reading next packet!";
			reply = ReadNextDCSPacket(tmo_ms);  // Read the next packet
			continue;
		}
		else
		{
			data1 = reply1tmp.second;
			data2 = reply2tmp.second;
		}
	}
	device_.end_dcs_transaction();
	DTC_TLOG(TLVL_TRACE) << "ReadROCRegisters returning " << static_cast<int>(data1) << " for link " << static_cast<int>(link)
						 << ", address " << static_cast<int>(address1) << ", " << static_cast<int>(data2) << ", address "
						 << static_cast<int>(address2);
	return std::make_pair(data1, data2);
}

bool DTCLib::DTC::WriteROCRegisters(const DTC_Link_ID& link, const uint16_t address1, const uint16_t data1,
									const uint16_t address2, const uint16_t data2, bool requestAck, int ack_tmo_ms)
{
	device_.begin_dcs_transaction();
	if (requestAck)
	{
		dcsDMAInfo_.currentReadPtr = nullptr;
		ReleaseAllBuffers(DTC_DMA_Engine_DCS);
	}
	SendDCSRequestPacket(link, DTC_DCSOperationType_Write, address1, data1, address2, data2, false /*quiet*/, requestAck);

	bool ackReceived = false;
	if (requestAck)
	{
		DTC_TLOG(TLVL_TRACE) << "WriteROCRegisters: Checking for ack";
		auto reply = ReadNextDCSPacket(ack_tmo_ms);
		while (reply != nullptr)
		{
			auto reply1tmp = reply->GetReply(false);
			auto reply2tmp = reply->GetReply(true);
			auto linktmp = reply->GetLinkID();
			DTC_TLOG(TLVL_TRACE) << "Got packet, "
								 << "link=" << static_cast<int>(linktmp) << " (expected " << static_cast<int>(link) << "), "
								 << "address1=" << static_cast<int>(reply1tmp.first) << " (expected "
								 << static_cast<int>(address1) << "), "
								 << "data1=" << static_cast<int>(reply1tmp.second)
								 << "address2=" << static_cast<int>(reply2tmp.first) << " (expected "
								 << static_cast<int>(address2) << "), "
								 << "data2=" << static_cast<int>(reply2tmp.second);

			reply.reset(nullptr);
			if (reply1tmp.first != address1 || reply2tmp.first != address2 || linktmp != link || !reply->IsAckRequested())
			{
				DTC_TLOG(TLVL_TRACE) << "Address or link did not match, or ack bit was not set, reading next packet!";
				reply = ReadNextDCSPacket(ack_tmo_ms);  // Read the next packet
			}
			else
			{
				ackReceived = true;
			}
		}
	}
	device_.end_dcs_transaction();
	return !requestAck || ackReceived;
}

void DTCLib::DTC::ReadROCBlock(
	std::vector<uint16_t>& data,
	const DTC_Link_ID& link, const uint16_t address,
	const uint16_t wordCount, bool incrementAddress, int tmo_ms)
{
	DTC_DCSRequestPacket req(link, DTC_DCSOperationType_BlockRead, false, incrementAddress, address, wordCount);

	DTC_TLOG(TLVL_SendDCSRequestPacket) << "ReadROCBlock before WriteDMADCSPacket - DTC_DCSRequestPacket";

	dcsDMAInfo_.currentReadPtr = nullptr;

	if (!ReadDCSReception()) EnableDCSReception();

	device_.begin_dcs_transaction();
	ReleaseAllBuffers(DTC_DMA_Engine_DCS);
	WriteDMAPacket(req);
	DTC_TLOG(TLVL_SendDCSRequestPacket) << "ReadROCBlock after  WriteDMADCSPacket - DTC_DCSRequestPacket";

	usleep(2500);

	auto reply = ReadNextDCSPacket(tmo_ms);
	while (reply != nullptr)
	{
		auto replytmp = reply->GetReply(false);
		auto linktmp = reply->GetLinkID();
		DTC_TLOG(TLVL_TRACE) << "Got packet, "
							 << "link=" << static_cast<int>(linktmp) << " (expected " << static_cast<int>(link) << "), "
							 << "address=" << static_cast<int>(replytmp.first) << " (expected " << static_cast<int>(address)
							 << "), "
							 << "wordCount=" << static_cast<int>(replytmp.second);

		data = reply->GetBlockReadData();
		auto packetCount = reply->GetBlockPacketCount();
		reply.reset(nullptr);
		if (replytmp.first != address || linktmp != link)
		{
			DTC_TLOG(TLVL_TRACE) << "Address or link did not match, reading next packet!";
			reply = ReadNextDCSPacket(tmo_ms);  // Read the next packet
			continue;
		}

		auto wordCount = replytmp.second;
		auto processedWords = 3;

		while (packetCount > 0)
		{
			dcsDMAInfo_.lastReadPtr = reinterpret_cast<uint8_t*>(dcsDMAInfo_.lastReadPtr) + 16;
			auto dataPacket = new DTC_DataPacket(dcsDMAInfo_.lastReadPtr);
			if (dataPacket == nullptr) break;

			DTC_TLOG(TLVL_TRACE) << "ReadROCBlock: next data packet: " << dataPacket->toJSON();
			auto byteInPacket = 0;

			while (wordCount - processedWords > 0 && byteInPacket < 16)
			{
				uint16_t thisWord = dataPacket->GetByte(byteInPacket) + (dataPacket->GetByte(byteInPacket + 1) << 8);
				byteInPacket += 2;
				data.push_back(thisWord);
				processedWords++;
			}

			packetCount--;
		}
	}
	device_.end_dcs_transaction();

	DTC_TLOG(TLVL_TRACE) << "ReadROCBlock returning " << static_cast<int>(data.size()) << " words for link " << static_cast<int>(link)
						 << ", address " << static_cast<int>(address);
}

bool DTCLib::DTC::WriteROCBlock(const DTC_Link_ID& link, const uint16_t address,
								const std::vector<uint16_t>& blockData, bool requestAck, bool incrementAddress, int ack_tmo_ms)
{
	DTC_DCSRequestPacket req(link, DTC_DCSOperationType_BlockWrite, requestAck, incrementAddress, address);
	req.SetBlockWriteData(blockData);

	DTC_TLOG(TLVL_SendDCSRequestPacket) << "WriteROCBlock before WriteDMADCSPacket - DTC_DCSRequestPacket";

	if (!ReadDCSReception()) EnableDCSReception();

	device_.begin_dcs_transaction();
	if (requestAck)
	{
		dcsDMAInfo_.currentReadPtr = nullptr;
		ReleaseAllBuffers(DTC_DMA_Engine_DCS);
	}
	WriteDMAPacket(req);
	DTC_TLOG(TLVL_SendDCSRequestPacket) << "WriteROCBlock after  WriteDMADCSPacket - DTC_DCSRequestPacket";

	bool ackReceived = false;
	if (requestAck)
	{
		DTC_TLOG(TLVL_TRACE) << "WriteROCBlock: Checking for ack";
		auto reply = ReadNextDCSPacket(ack_tmo_ms);
		while (reply != nullptr)
		{
			auto reply1tmp = reply->GetReply(false);
			auto linktmp = reply->GetLinkID();
			DTC_TLOG(TLVL_TRACE) << "Got packet, "
								 << "link=" << static_cast<int>(linktmp) << " (expected " << static_cast<int>(link) << "), "
								 << "address1=" << static_cast<int>(reply1tmp.first) << " (expected "
								 << static_cast<int>(address) << "), "
								 << "data1=" << static_cast<int>(reply1tmp.second);

			reply.reset(nullptr);
			if (reply1tmp.first != address || linktmp != link || !reply->IsAckRequested())
			{
				DTC_TLOG(TLVL_TRACE) << "Address or link did not match, or ack bit was not set, reading next packet!";
				reply = ReadNextDCSPacket(ack_tmo_ms);  // Read the next packet
			}
			else
			{
				ackReceived = true;
			}
		}
	}
	device_.end_dcs_transaction();
	return !requestAck || ackReceived;
}

uint16_t DTCLib::DTC::ReadExtROCRegister(const DTC_Link_ID& link, const uint16_t block,
										 const uint16_t address, int tmo_ms)
{
	uint16_t addressT = address & 0x7FFF;
	WriteROCRegister(link, 12, block, false, tmo_ms);
	WriteROCRegister(link, 13, addressT, false, tmo_ms);
	WriteROCRegister(link, 13, addressT | 0x8000, false, tmo_ms);
	return ReadROCRegister(link, 22, tmo_ms);
}

bool DTCLib::DTC::WriteExtROCRegister(const DTC_Link_ID& link, const uint16_t block,
									  const uint16_t address,
									  const uint16_t data, bool requestAck, int ack_tmo_ms)
{
	uint16_t dataT = data & 0x7FFF;
	bool success = true;
	success &= WriteROCRegister(link, 12, block + (address << 8), requestAck, ack_tmo_ms);
	success &= WriteROCRegister(link, 13, dataT, requestAck, ack_tmo_ms);
	success &= WriteROCRegister(link, 13, dataT | 0x8000, requestAck, ack_tmo_ms);
	return success;
}

std::string DTCLib::DTC::ROCRegDump(const DTC_Link_ID& link)
{
	std::ostringstream o;
	o.setf(std::ios_base::boolalpha);
	o << "{";
	o << "\"Forward Detector 0 Status\": " << ReadExtROCRegister(link, 8, 0) << ",\n";
	o << "\"Forward Detector 1 Status\": " << ReadExtROCRegister(link, 9, 0) << ",\n";
	o << "\"Command Handler Status\": " << ReadExtROCRegister(link, 10, 0) << ",\n";
	o << "\"Packet Sender 0 Status\": " << ReadExtROCRegister(link, 11, 0) << ",\n";
	o << "\"Packet Sender 1 Status\": " << ReadExtROCRegister(link, 12, 0) << ",\n";
	o << "\"Forward Detector 0 Errors\": " << ReadExtROCRegister(link, 8, 1) << ",\n";
	o << "\"Forward Detector 1 Errors\": " << ReadExtROCRegister(link, 9, 1) << ",\n";
	o << "\"Command Handler Errors\": " << ReadExtROCRegister(link, 10, 1) << ",\n";
	o << "\"Packet Sender 0 Errors\": " << ReadExtROCRegister(link, 11, 1) << ",\n";
	o << "\"Packet Sender 1 Errors\": " << ReadExtROCRegister(link, 12, 1) << "\n";
	o << "}";

	return o.str();
}

void DTCLib::DTC::SendHeartbeatPacket(const DTC_Link_ID& link, const DTC_EventWindowTag& when, bool quiet)
{
	DTC_HeartbeatPacket req(link, when);
	DTC_TLOG(TLVL_SendHeartbeatPacket) << "SendHeartbeatPacket before WriteDMADAQPacket - DTC_HeartbeatPacket";
	if (!quiet) DTC_TLOG(TLVL_SendHeartbeatPacket) << req.toJSON();
	WriteDMAPacket(req);
	DTC_TLOG(TLVL_SendHeartbeatPacket) << "SendHeartbeatPacket after  WriteDMADAQPacket - DTC_HeartbeatPacket";
}

// Note! Before calling this function SendDCSRequestPacket(), the device must be locked with device_.begin_dcs_transaction();
void DTCLib::DTC::SendDCSRequestPacket(const DTC_Link_ID& link, const DTC_DCSOperationType type, const uint16_t address,
									   const uint16_t data, const uint16_t address2, const uint16_t data2, bool quiet, bool requestAck)
{
	DTC_DCSRequestPacket req(link, type, requestAck, false /*incrementAddress*/, address, data);

	if (!quiet) DTC_TLOG(TLVL_SendDCSRequestPacket) << "Init DCS Packet: \n"
													<< req.toJSON();

	if (type == DTC_DCSOperationType_DoubleRead ||
		type == DTC_DCSOperationType_DoubleWrite)
	{
		DTC_TLOG(TLVL_SendDCSRequestPacket) << "Double operation enabled!";
		req.AddRequest(address2, data2);
	}

	DTC_TLOG(TLVL_SendDCSRequestPacket) << "SendDCSRequestPacket before WriteDMADCSPacket - DTC_DCSRequestPacket";

	if (!quiet) DTC_TLOG(TLVL_SendDCSRequestPacket) << "Sending DCS Packet: \n"
													<< req.toJSON();

	if (!ReadDCSReception()) EnableDCSReception();

	WriteDMAPacket(req);
	DTC_TLOG(TLVL_SendDCSRequestPacket) << "SendDCSRequestPacket after  WriteDMADCSPacket - DTC_DCSRequestPacket";
}

std::unique_ptr<DTCLib::DTC_Event> DTCLib::DTC::ReadNextDAQDMA(int tmo_ms)
{
	DTC_TLOG(TLVL_ReadNextDAQPacket) << "ReadNextDAQDMA BEGIN";

	if (daqDMAInfo_.currentReadPtr != nullptr)
	{
		DTC_TLOG(TLVL_ReadNextDAQPacket) << "ReadNextDAQDMA BEFORE BUFFER CHECK daqDMAInfo_.currentReadPtr="
										 << (void*)daqDMAInfo_.currentReadPtr << " *nextReadPtr_=0x" << std::hex
										 << *(uint16_t*)daqDMAInfo_.currentReadPtr;
	}
	else
	{
		DTC_TLOG(TLVL_ReadNextDAQPacket) << "ReadNextDAQDMA BEFORE BUFFER CHECK daqDMAInfo_.currentReadPtr=nullptr";
	}

	auto index = CFOandDTC_DMAs::GetCurrentBuffer(&daqDMAInfo_);

	// Need new buffer if GetCurrentBuffer returns -1 (no buffers) or -2 (done with all held buffers)
	if (index < 0)
	{
		DTC_TLOG(TLVL_ReadNextDAQPacket) << "ReadNextDAQDMA Obtaining new DAQ Buffer";

		void* oldBufferPtr = nullptr;
		if (daqDMAInfo_.buffer.size() > 0) oldBufferPtr = &daqDMAInfo_.buffer.back()[0];
		auto sts = ReadBuffer(DTC_DMA_Engine_DAQ, tmo_ms);  // does return code
		if (sts <= 0)
		{
			DTC_TLOG(TLVL_ReadNextDAQPacket) << "ReadNextDAQDMA: ReadBuffer returned " << sts << ", returning nullptr";
			return nullptr;
		}
		// MUST BE ABLE TO HANDLE daqbuffer_==nullptr OR retry forever?
		daqDMAInfo_.currentReadPtr = &daqDMAInfo_.buffer.back()[0];
		DTC_TLOG(TLVL_ReadNextDAQPacket) << "ReadNextDAQDMA daqDMAInfo_.currentReadPtr=" << (void*)daqDMAInfo_.currentReadPtr
										 << " *daqDMAInfo_.currentReadPtr=0x" << std::hex << *(unsigned*)daqDMAInfo_.currentReadPtr
										 << " lastReadPtr_=" << (void*)daqDMAInfo_.lastReadPtr;
		void* bufferIndexPointer = static_cast<uint8_t*>(daqDMAInfo_.currentReadPtr) + 4;
		if (daqDMAInfo_.currentReadPtr == oldBufferPtr && daqDMAInfo_.bufferIndex == *static_cast<uint32_t*>(bufferIndexPointer))
		{
			DTC_TLOG(TLVL_WARN) << "ReadNextDAQDMA: New buffer is the same as old. Releasing buffer and returning nullptr";
			daqDMAInfo_.currentReadPtr = nullptr;
			// We didn't actually get a new buffer...this probably means there's no more data
			// Try and see if we're merely stuck...hopefully, all the data is out of the buffers...
			device_.read_release(DTC_DMA_Engine_DAQ, 1);
			return nullptr;
		}
		daqDMAInfo_.bufferIndex++;

		daqDMAInfo_.currentReadPtr = static_cast<uint8_t*>(daqDMAInfo_.currentReadPtr) + 4;
		*static_cast<uint32_t*>(daqDMAInfo_.currentReadPtr) = daqDMAInfo_.bufferIndex;
		daqDMAInfo_.currentReadPtr = static_cast<uint8_t*>(daqDMAInfo_.currentReadPtr) + 4;

		index = daqDMAInfo_.buffer.size() - 1;
	}

	DTC_TLOG(TLVL_ReadNextDAQPacket) << "Creating DTC_Event from current DMA Buffer";
	// Utilities::PrintBuffer(daqDMAInfo_.currentReadPtr, 128, TLVL_ReadNextDAQPacket);
	auto res = std::make_unique<DTC_Event>(daqDMAInfo_.currentReadPtr);  // only does setup of Event Header

	auto eventByteCount = res->GetEventByteCount();
	if (eventByteCount == 0)
	{
		__SS__ << "Event inclusive byte count cannot be zero!" << __E__;
		__SS_THROW__;
	}
	size_t remainingBufferSize = CFOandDTC_DMAs::GetBufferByteCount(&daqDMAInfo_, index) - sizeof(uint64_t);
	DTC_TLOG(TLVL_ReadNextDAQPacket) << "eventByteCount: " << eventByteCount << ", remainingBufferSize: " << remainingBufferSize;
	// Check for continued DMA
	if (eventByteCount > remainingBufferSize)
	{
		// We're going to set lastReadPtr here, so that if this buffer isn't used by GetData, we start at the beginning of this event next time
		daqDMAInfo_.lastReadPtr = static_cast<uint8_t*>(daqDMAInfo_.currentReadPtr);

		auto inmem = std::make_unique<DTC_Event>(eventByteCount);
		memcpy(const_cast<void*>(inmem->GetRawBufferPointer()), res->GetRawBufferPointer(), remainingBufferSize);

		auto bytes_read = remainingBufferSize;
		while (bytes_read < eventByteCount)
		{
			DTC_TLOG(TLVL_ReadNextDAQPacket) << "ReadNextDAQDMA Obtaining new DAQ Buffer, bytes_read=" << bytes_read << ", eventByteCount=" << eventByteCount;

			void* oldBufferPtr = nullptr;
			if (daqDMAInfo_.buffer.size() > 0) oldBufferPtr = &daqDMAInfo_.buffer.back()[0];
			auto sts = ReadBuffer(DTC_DMA_Engine_DAQ, tmo_ms);  // does return code
			if (sts <= 0)
			{
				DTC_TLOG(TLVL_WARN) << "ReadNextDAQDMA: ReadBuffer returned " << sts << ", returning nullptr";
				return nullptr;
			}
			// MUST BE ABLE TO HANDLE daqbuffer_==nullptr OR retry forever?
			daqDMAInfo_.currentReadPtr = &daqDMAInfo_.buffer.back()[0];
			DTC_TLOG(TLVL_ReadNextDAQPacket) << "ReadNextDAQDMA daqDMAInfo_.currentReadPtr=" << (void*)daqDMAInfo_.currentReadPtr
											 << " *daqDMAInfo_.currentReadPtr=0x" << std::hex << *(unsigned*)daqDMAInfo_.currentReadPtr
											 << " lastReadPtr_=" << (void*)daqDMAInfo_.lastReadPtr;
			void* bufferIndexPointer = static_cast<uint8_t*>(daqDMAInfo_.currentReadPtr) + 4;
			if (daqDMAInfo_.currentReadPtr == oldBufferPtr && daqDMAInfo_.bufferIndex == *static_cast<uint32_t*>(bufferIndexPointer))
			{
				DTC_TLOG(TLVL_WARN) << "ReadNextDAQDMA: New buffer is the same as old. Releasing buffer and returning nullptr";
				daqDMAInfo_.currentReadPtr = nullptr;
				// We didn't actually get a new buffer...this probably means there's no more data
				// Try and see if we're merely stuck...hopefully, all the data is out of the buffers...
				device_.read_release(DTC_DMA_Engine_DAQ, 1);
				return nullptr;
			}
			daqDMAInfo_.bufferIndex++;

			size_t buffer_size = *static_cast<uint16_t*>(daqDMAInfo_.currentReadPtr);
			daqDMAInfo_.currentReadPtr = static_cast<uint8_t*>(daqDMAInfo_.currentReadPtr) + 4;
			*static_cast<uint32_t*>(daqDMAInfo_.currentReadPtr) = daqDMAInfo_.bufferIndex;
			daqDMAInfo_.currentReadPtr = static_cast<uint8_t*>(daqDMAInfo_.currentReadPtr) + 4;

			size_t remainingEventSize = eventByteCount - bytes_read;
			size_t copySize = remainingEventSize < buffer_size - 8 ? remainingEventSize : buffer_size - 8;
			memcpy(const_cast<uint8_t*>(static_cast<const uint8_t*>(inmem->GetRawBufferPointer()) + bytes_read), daqDMAInfo_.currentReadPtr, copySize);
			bytes_read += buffer_size - 8;

			// Increment by the size of the data block
			daqDMAInfo_.currentReadPtr = reinterpret_cast<char*>(daqDMAInfo_.currentReadPtr) + copySize;
		}

		res.swap(inmem);
	}
	else  // Event not split over multiple DMAs
	{
		// Update the packet pointers

		// lastReadPtr_ is easy...
		daqDMAInfo_.lastReadPtr = daqDMAInfo_.currentReadPtr;

		// Increment by the size of the data block
		daqDMAInfo_.currentReadPtr = reinterpret_cast<char*>(daqDMAInfo_.currentReadPtr) + res->GetEventByteCount();
	}
	res->SetupEvent();  // does setup of Event header + all payload

	DTC_TLOG(TLVL_ReadNextDAQPacket) << "ReadNextDAQDMA: RETURN";
	return res;
}  // end ReadNextDAQDMA()

bool DTCLib::DTC::ReadNextDAQSubEventDMA(std::vector<std::unique_ptr<DTC_SubEvent>>& output, int tmo_ms)
{
	TRACE_EXIT
	{
		DTC_TLOG(TLVL_ReadNextDAQPacket) << "ReadNextDAQSubEventDMA EXIT"
										 << " currentReadPtr=" << (void*)daqDMAInfo_.currentReadPtr
										 << " lastReadPtr=" << (void*)daqDMAInfo_.lastReadPtr
										 << " buffer.size()=" << daqDMAInfo_.buffer.size();
	};

	DTC_TLOG(TLVL_ReadNextDAQPacket) << "ReadNextDAQSubEventDMA BEGIN";

	if (daqDMAInfo_.currentReadPtr != nullptr)
	{
		DTC_TLOG(TLVL_ReadNextDAQPacket) << "ReadNextDAQSubEventDMA BEFORE BUFFER CHECK daqDMAInfo_.currentReadPtr="
										 << (void*)daqDMAInfo_.currentReadPtr << " currentBufferTransferSize=0x" << std::hex
										 << *(uint16_t*)daqDMAInfo_.currentReadPtr;
	}
	else
	{
		DTC_TLOG(TLVL_ReadNextDAQPacket) << "ReadNextDAQSubEventDMA BEFORE BUFFER CHECK daqDMAInfo_.currentReadPtr=nullptr";
	}

	auto index = CFOandDTC_DMAs::GetCurrentBuffer(&daqDMAInfo_);  // if buffers onhand, returns daqDMAInfo_.buffer.size().. which is count used by ReleaseBuffers()

	size_t metaBufferSize = 0;

	// Need new starting subevent buffer if GetCurrentBuffer returns -1 (no buffers) or -2 (done with all held buffers)
	if (index < 0)
	{
		DTC_TLOG(TLVL_ReadNextDAQPacket) << "ReadNextDAQSubEventDMA Obtaining new DAQ Buffer";

		void* oldBufferPtr = nullptr;
		if (daqDMAInfo_.buffer.size() > 0) oldBufferPtr = &daqDMAInfo_.buffer.back()[0];
		auto sts = ReadBuffer(DTC_DMA_Engine_DAQ, tmo_ms);  // does return code
		if (sts <= 0)
		{
			DTC_TLOG(TLVL_ReadNextDAQPacket) << "ReadNextDAQSubEventDMA: ReadBuffer returned " << sts << ", returning false";
			return false;
		}
		metaBufferSize = sts;
		// MUST BE ABLE TO HANDLE daqbuffer_==nullptr OR retry forever?
		daqDMAInfo_.currentReadPtr = &daqDMAInfo_.buffer.back()[0];
		DTC_TLOG(TLVL_ReadNextDAQPacket) << "ReadNextDAQSubEventDMA daqDMAInfo_.currentReadPtr=" << (void*)daqDMAInfo_.currentReadPtr
										 << " currentBufferTransferSize=0x" << std::hex << *(unsigned*)daqDMAInfo_.currentReadPtr
										 << " lastReadPtr=" << (void*)daqDMAInfo_.lastReadPtr;
		void* bufferIndexPointer = static_cast<uint8_t*>(daqDMAInfo_.currentReadPtr) + 4;
		if (daqDMAInfo_.currentReadPtr == oldBufferPtr && daqDMAInfo_.bufferIndex == *static_cast<uint32_t*>(bufferIndexPointer))
		{
			daqDMAInfo_.currentReadPtr = nullptr;
			// We didn't actually get a new buffer...this probably means there's no more data
			// Try and see if we're merely stuck...hopefully, all the data is out of the buffers...
			device_.read_release(DTC_DMA_Engine_DAQ, 1);
			DTC_TLOG(TLVL_WARN)
				<< "ReadNextDAQSubEventDMA: New buffer was the same as old. Released buffer and returning false";
			return false;
		}
		daqDMAInfo_.bufferIndex++;

		daqDMAInfo_.currentReadPtr = static_cast<uint8_t*>(daqDMAInfo_.currentReadPtr) + 4;
		*static_cast<uint32_t*>(daqDMAInfo_.currentReadPtr) = daqDMAInfo_.bufferIndex;
		daqDMAInfo_.currentReadPtr = static_cast<uint8_t*>(daqDMAInfo_.currentReadPtr) + 4;

		index = daqDMAInfo_.buffer.size() - 1;
		DTC_TLOG(TLVL_ReadNextDAQPacket) << "Creating DTC_SubEvent from new DMA Buffer, index=" << index << " in " << daqDMAInfo_.buffer.size() << " buffers.";
	}
	else  // buffer already onhand
	{
		__SS__ << "Impossible buffer already onhand!" << __E__;
		__SS_THROW__;

		// DTC_TLOG(TLVL_ReadNextDAQPacket) << "Creating DTC_SubEvent from current DMA Buffer, index=" << index <<
		// 	" in " << daqDMAInfo_.buffer.size() << " buffers.";
	}

	if (  // current read ptr sanity check
		(void*)(static_cast<uint8_t*>(daqDMAInfo_.currentReadPtr) - 8) !=
		(void*)(&daqDMAInfo_.buffer[index][0]))
	{
		__SS__ << "Impossible current buffer pointer for index " << index << ".. " << (void*)(static_cast<uint8_t*>(daqDMAInfo_.currentReadPtr) - 8) << " != " << (void*)&daqDMAInfo_.buffer[index - 1][0] << __E__;
		__SS_THROW__;
	}

	// Utilities::PrintBuffer(daqDMAInfo_.currentReadPtr, 128, TLVL_ReadNextDAQPacket);

	size_t inBufferByteCount = CFOandDTC_DMAs::GetBufferByteCount(&daqDMAInfo_, index);
	// Use DMA descriptor byte count from ReadBuffer (dont use the in-buffer DMA transfer size count because the DTC stacks sub-transfers!)
	// Subtract 1 for tlast byte, but NOT when buffer is completely full (no room for tlast)
	size_t remainingBufferSize = metaBufferSize < sizeof(mu2e_databuff_t) ? metaBufferSize - 1 : metaBufferSize;
	DTC_TLOG(TLVL_ReadNextDAQPacket) << "sizeof(DTC_SubEventHeader) = " << sizeof(DTC_SubEventHeader) << " GetBufferByteCount=" << inBufferByteCount
									 << " metaBufferSize=" << metaBufferSize;

	// Extract multiple subevents from buffer
	//  Note: the while loop condition allows entry when there is at least the
	//  per-subevent DMA transfer size (8 bytes) remaining.  This covers three cases:
	//    1) Only the 8-byte per-subevent prefix at the buffer tail (0 header bytes) -- entire subevent is in the next buffer
	//    2) Partial subevent header straddles the buffer boundary (1..47 header bytes)
	//    3) Full subevent header (and possibly payload) fits in the current buffer
	//  Cases 1 and 2 are handled by reading continuation buffer(s) and assembling contiguously.
	while (remainingBufferSize >= sizeof(uint64_t))
	{
		remainingBufferSize -= sizeof(uint64_t);  // remove per-subevent DMA transfer size from remaining byte count

		// Check if the subevent header is fully contained in the current buffer
		bool headerSplitAcrossBuffers = (remainingBufferSize < sizeof(DTC_SubEventHeader));

		if (headerSplitAcrossBuffers)
		{
			// The subevent header straddles the DMA buffer boundary (or is entirely in the next buffer).
			// We must read the continuation buffer to assemble the full header before we can
			// determine the subevent byte count and proceed with normal processing.
			DTC_TLOG(TLVL_ReadNextDAQPacket) << "ReadNextDAQSubEventDMA: subevent header split across DMA buffer boundary!"
											 << " partialBytes(remainingBufferSize)=" << remainingBufferSize << " < sizeof(DTC_SubEventHeader)=" << sizeof(DTC_SubEventHeader)
											 << " at currentReadPtr=" << (void*)daqDMAInfo_.currentReadPtr;

			daqDMAInfo_.lastReadPtr = static_cast<uint8_t*>(daqDMAInfo_.currentReadPtr);

			// Save partial data from end of current buffer (may be 0 if only the per-subevent prefix was here)
			size_t partialBytes = remainingBufferSize;

			// Read continuation buffer to get the rest
			void* oldBufferPtr = nullptr;
			if (daqDMAInfo_.buffer.size() > 0) oldBufferPtr = &daqDMAInfo_.buffer.back()[0];

			int sts;
			int retry = 10;
			while ((sts = ReadBuffer(DTC_DMA_Engine_DAQ, 10 /* retries */)) <= 0 && --retry > 0)
				usleep(1000);

			if (sts <= 0)
			{
				__SS__ << "Timeout after receiving only partial subevent header (" << partialBytes << "/" << sizeof(DTC_SubEventHeader) << " bytes)!"
					   << " currentReadPtr=" << (void*)daqDMAInfo_.currentReadPtr;
				__SS_THROW__;
			}

			auto* contBufferStart = &daqDMAInfo_.buffer.back()[0];
			if (contBufferStart == oldBufferPtr)
			{
				__SS__ << "Received same buffer twice, only received partial subevent header!! partialBytes=" << partialBytes;
				__SS_THROW__;
			}
			daqDMAInfo_.bufferIndex++;

			// Assemble a temporary buffer with at least the full header so we can read the subevent byte count.
			// NOTE: continuation buffers do NOT have a per-sub-transfer DMA byte count header at offset 0.
			size_t contBufferPayload = sts;
			size_t headerBytesNeeded = sizeof(DTC_SubEventHeader) - partialBytes;

			if (contBufferPayload < headerBytesNeeded)
			{
				__SS__ << "Continuation buffer too small to complete subevent header! contBufferPayload=" << contBufferPayload
					   << " headerBytesNeeded=" << headerBytesNeeded;
				__SS_THROW__;
			}

			// Assemble the complete header from the partial data in the old buffer
			// (if any) and the beginning of the continuation buffer.
			uint8_t headerBuf[sizeof(DTC_SubEventHeader)];
			if (partialBytes > 0)
				memcpy(headerBuf, daqDMAInfo_.currentReadPtr, partialBytes);
			memcpy(headerBuf + partialBytes, contBufferStart, headerBytesNeeded);

			// Extract subEventByteCount from the assembled header (first 25 bits of first 4 bytes)
			auto subEventByteCount = static_cast<size_t>(
				*reinterpret_cast<uint32_t*>(headerBuf) & 0x1FFFFFF);

			if (subEventByteCount < sizeof(DTC_SubEventHeader))
			{
				__SS__ << "SubEvent inclusive byte count (" << subEventByteCount << ") cannot be less than the size of the subevent header ("
					   << sizeof(DTC_SubEventHeader) << "-bytes)! (detected during split-header handling)" << __E__;
				__SS_THROW__;
			}

			DTC_TLOG(TLVL_ReadNextDAQPacket) << "ReadNextDAQSubEventDMA split-header: assembled subEventByteCount=" << subEventByteCount
											 << " (0x" << std::hex << subEventByteCount << ")"
											 << " partialBytes=" << std::dec << partialBytes << " contBufferPayload=" << contBufferPayload;

			// Allocate contiguous memory for the full subevent and copy data in
			auto inmem = std::make_unique<DTC_SubEvent>(subEventByteCount);

			// Copy partial data from end of previous buffer (if any)
			if (partialBytes > 0)
				memcpy(const_cast<void*>(inmem->GetRawBufferPointer()), daqDMAInfo_.currentReadPtr, partialBytes);

			// Copy data from continuation buffer
			size_t contBytesForSubEvent = subEventByteCount - partialBytes;
			size_t contCopySize = contBytesForSubEvent < contBufferPayload ? contBytesForSubEvent : contBufferPayload;
			memcpy(const_cast<uint8_t*>(static_cast<const uint8_t*>(inmem->GetRawBufferPointer()) + partialBytes),
				   contBufferStart, contCopySize);

			auto bytes_read = partialBytes + contCopySize;
			daqDMAInfo_.currentReadPtr = reinterpret_cast<char*>(contBufferStart) + contCopySize;

			size_t lastBufferPayloadSize = contBufferPayload;
			size_t lastCopySize = contCopySize;

			// If the subevent still needs more data from additional buffers, continue reading
			while (bytes_read < subEventByteCount)
			{
				DTC_TLOG(TLVL_ReadNextDAQPacket) << "ReadNextDAQSubEventDMA split-header continuation: bytes_read=" << bytes_read
												 << " subEventByteCount=" << subEventByteCount;

				oldBufferPtr = nullptr;
				if (daqDMAInfo_.buffer.size() > 0) oldBufferPtr = &daqDMAInfo_.buffer.back()[0];

				retry = 10;
				while ((sts = ReadBuffer(DTC_DMA_Engine_DAQ, 10 /* retries */)) <= 0 && --retry > 0)
					usleep(1000);

				if (sts <= 0)
				{
					__SS__ << "Timeout after receiving only partial subevent (split-header)! bytes_read=" << bytes_read
						   << " subEventByteCount=" << subEventByteCount;
					__SS_THROW__;
				}

				daqDMAInfo_.currentReadPtr = &daqDMAInfo_.buffer.back()[0];
				if (daqDMAInfo_.currentReadPtr == oldBufferPtr)
				{
					__SS__ << "Received same buffer twice during split-header subevent continuation! bytes_read=" << bytes_read;
					__SS_THROW__;
				}
				daqDMAInfo_.bufferIndex++;

				size_t buffer_size = sts;
				size_t remainingEventSize = subEventByteCount - bytes_read;
				size_t copySize = remainingEventSize < buffer_size ? remainingEventSize : buffer_size;

				DTC_TLOG(TLVL_ReadNextDAQPacket) << "ReadNextDAQSubEventDMA split-header continuation buffer: buffer_size=" << buffer_size
												 << " copySize=" << copySize << " remainingEventSize=" << remainingEventSize;

				memcpy(const_cast<uint8_t*>(static_cast<const uint8_t*>(inmem->GetRawBufferPointer()) + bytes_read),
					   daqDMAInfo_.currentReadPtr, copySize);
				bytes_read += buffer_size;

				daqDMAInfo_.currentReadPtr = reinterpret_cast<char*>(daqDMAInfo_.currentReadPtr) + copySize;
				lastBufferPayloadSize = buffer_size;
				lastCopySize = copySize;
			}

			// Now construct the SubEvent with the fully assembled data
			auto res = std::make_unique<DTC_SubEvent>(inmem->GetRawBufferPointer());

			DTC_TLOG(TLVL_ReadNextDAQPacket) << "ReadNextDAQSubEventDMA split-header: tag=" << res->GetEventWindowTag().GetEventWindowTag(true)
											 << std::hex << "(0x" << res->GetEventWindowTag().GetEventWindowTag(true) << ")"
											 << " subEventByteCount=" << std::dec << subEventByteCount;

			res.swap(inmem);

			try
			{
				std::string accumulatedErrors = "";
				auto ok = res->SetupSubEvent(accumulatedErrors);
				if (!ok)
				{
					__SS__ << "SubEvent is corrupt (split-header)! EWT=" << res->GetEventWindowTag() << ".\n\nAccumulated Errors: " << accumulatedErrors << __E__;
					__SS_THROW__;
				}
			}
			catch (...)
			{
				device_.spy(DTC_DMA_Engine_DAQ, 3 /* for once */ | 8 /* for wide view */);
				DTC_TLOG(TLVL_ERROR) << otsStyleStackTrace();
				throw;
			}

			output.push_back(std::move(res));

			// Update remaining buffer size for the last-read buffer to check for more subevents
			remainingBufferSize = lastBufferPayloadSize - lastCopySize;

			// Skip past the next sub-transfer DMA byte count header (same as single-buffer path)
			daqDMAInfo_.currentReadPtr = reinterpret_cast<char*>(daqDMAInfo_.currentReadPtr) + sizeof(uint64_t);
		}
		else  // subevent header fully in current buffer
		{
			auto res = std::make_unique<DTC_SubEvent>(daqDMAInfo_.currentReadPtr);  // only does setup of SubEvent header!

			auto subEventByteCount = res->GetSubEventByteCount();
			if (subEventByteCount < sizeof(DTC_SubEventHeader))
			{
				__SS__ << "SubEvent inclusive byte count cannot be less than the size of the subevent header (" << sizeof(DTC_SubEventHeader) << "-bytes)!" << __E__;
				__SS_THROW__;
			}

			DTC_TLOG(TLVL_ReadNextDAQPacket) << "subevent tag=" << res->GetEventWindowTag().GetEventWindowTag(true) << std::hex << "(0x" << res->GetEventWindowTag().GetEventWindowTag(true) << ")"
											 << " inclusive byte count: 0x" << std::hex << subEventByteCount << " (" << std::dec << subEventByteCount << ") inclusive packets " << subEventByteCount / 16 << ", remaining buffer size: 0x" << std::hex << remainingBufferSize << " (" << std::dec << remainingBufferSize << ") this buffer in packets = " << (remainingBufferSize - sizeof(DTC_SubEventHeader)) / 16 << ". "
											 << "Total subevent packet count: " << (subEventByteCount - sizeof(DTC_SubEventHeader)) / 16;

			// Check for continued DMA
			if (subEventByteCount > remainingBufferSize)
			{
				DTC_TLOG(TLVL_ReadNextDAQPacket) << "subevent needs more data by bytes " << std::hex << subEventByteCount - remainingBufferSize << " (" << std::dec << subEventByteCount - remainingBufferSize << ") packets " << (subEventByteCount - remainingBufferSize) / 16 << ". subEventByteCount=" << subEventByteCount << " remainingBufferSize=" << remainingBufferSize;

				// We're going to set lastReadPtr here, so that if this buffer isn't used by GetData, we start at the beginning of this event next time
				daqDMAInfo_.lastReadPtr = static_cast<uint8_t*>(daqDMAInfo_.currentReadPtr);

				auto inmem = std::make_unique<DTC_SubEvent>(subEventByteCount);

				if (0)  // for debugging
				{
					std::cout << "1st DMA buffer res size=" << remainingBufferSize << "\n";
					auto ptr = reinterpret_cast<const uint8_t*>(res->GetRawBufferPointer());
					for (size_t i = 0; i < remainingBufferSize /* + 16 */; i += 4)
						std::cout << std::dec << "res#" << i << "/" << remainingBufferSize << "(" << i / 16 << "/" << remainingBufferSize / 16 << ")" << std::hex << std::setw(8) << std::setfill('0') << *((uint32_t*)(&(ptr[i]))) << std::endl;
				}

				memcpy(const_cast<void*>(inmem->GetRawBufferPointer()), res->GetRawBufferPointer(), remainingBufferSize);

				if (0)  // for debugging
				{
					std::cout << "1st DMA buffer inmem size=" << remainingBufferSize << "\n";
					auto ptr = reinterpret_cast<const uint8_t*>(inmem->GetRawBufferPointer());
					for (size_t i = 0; i < remainingBufferSize + 16; i += 4)
						std::cout << std::dec << "inmem#" << i << "(" << i / 16 << ")" << std::hex << std::setw(8) << std::setfill('0') << *((uint32_t*)(&(ptr[i]))) << std::endl;
				}

				auto bytes_read = remainingBufferSize;
				size_t lastBufferPayloadSize = 0;
				size_t lastCopySize = 0;

				while (bytes_read < subEventByteCount)
				{
					DTC_TLOG(TLVL_ReadNextDAQPacket) << "ReadNextDAQSubEventDMA tag=" << inmem->GetEventWindowTag().GetEventWindowTag(true) << std::hex << "(0x" << inmem->GetEventWindowTag().GetEventWindowTag(true) << ")"
													 << " Obtaining new DAQ Buffer, bytes_read=" << bytes_read << ", subEventByteCount=" << subEventByteCount;

					void* oldBufferPtr = nullptr;
					if (daqDMAInfo_.buffer.size() > 0) oldBufferPtr = &daqDMAInfo_.buffer.back()[0];

					if (0)  // for debugging
					{
						std::cout << "1st DMA buffer\n";
						auto ptr = reinterpret_cast<const uint8_t*>(&daqDMAInfo_.buffer.back()[0]);
						for (size_t i = 0; i < bytes_read + sizeof(DTCLib::DTC_SubEventHeader); i += 4)
							std::cout << std::dec << "#" << i << "(" << i / 16 << ")" << std::hex << std::setw(8) << std::setfill('0') << *((uint32_t*)(&(ptr[i]))) << std::endl;
					}

					int sts;
					int retry = 10;
					// timeout is an exception at this point because no way to resolve partial subevent record!
					while ((sts = ReadBuffer(DTC_DMA_Engine_DAQ, 10 /* retries */))  // does return code
							   <= 0 &&
						   --retry > 0) usleep(1000);

					if (sts <= 0)
					{
						__SS__ << "Timeout of " << tmo_ms << " ms after receiving only partial subevent! Subevent tag=" << inmem->GetEventWindowTag().GetEventWindowTag(true) << std::hex << "(0x" << inmem->GetEventWindowTag().GetEventWindowTag(true) << ")";
						__SS_THROW__;
					}

					daqDMAInfo_.currentReadPtr = &daqDMAInfo_.buffer.back()[0];
					DTC_TLOG(TLVL_ReadNextDAQPacket) << "ReadNextDAQSubEventDMA daqDMAInfo_.currentReadPtr=" << (void*)daqDMAInfo_.currentReadPtr
													 << " *daqDMAInfo_.currentReadPtr=0x" << std::hex << *(unsigned*)daqDMAInfo_.currentReadPtr
													 << " lastReadPtr=" << (void*)daqDMAInfo_.lastReadPtr;

					if (0)  // for debugging
					{
						std::cout << "1st buffer\n";
						auto ptr = reinterpret_cast<const uint8_t*>(inmem->GetRawBufferPointer());
						for (size_t i = 0; i < bytes_read + 16; i += 4)
							std::cout << std::dec << "#" << i << "(" << i / 16 << ")" << std::hex << std::setw(8) << std::setfill('0') << *((uint32_t*)(&(ptr[i]))) << std::endl;
					}

					if (daqDMAInfo_.currentReadPtr == oldBufferPtr)
					{
						// We didn't actually get a new buffer...this probably means there's no more data
						// timeout is an exception at this point because no way to resolve partial subevent record!
						__SS__ << "Received same buffer twice, only received partial subevent!! Subevent tag=" << inmem->GetEventWindowTag().GetEventWindowTag(true) << std::hex << "(0x" << inmem->GetEventWindowTag().GetEventWindowTag(true) << ")";
						__SS_THROW__;
					}
					daqDMAInfo_.bufferIndex++;

					// NOTE: continuation buffers do NOT have a per-sub-transfer DMA byte count header at offset 0
					//  (unlike the initial buffer). The raw continuation data starts at offset 0.
					//  Use the full sts (DMA descriptor byte count) as payload size.
					size_t buffer_size = sts;

					size_t remainingEventSize = subEventByteCount - bytes_read;
					size_t copySize = remainingEventSize < buffer_size ? remainingEventSize : buffer_size;

					if (0)  // for debugging
					{
						std::cout << "2nd buffer\n";
						auto ptr = reinterpret_cast<const uint8_t*>(daqDMAInfo_.currentReadPtr);
						for (size_t i = 0; i < copySize; i += 4)
							std::cout << std::dec << "#" << i << "(" << i / 16 << "/" << copySize / 16 << ")" << std::hex << std::setw(8) << std::setfill('0') << *((uint32_t*)(&(ptr[i]))) << std::endl;
					}

					DTC_TLOG(TLVL_ReadNextDAQPacket) << "ReadNextDAQSubEventDMA got new buffer for subevent continuation, buffer_size=" << buffer_size << " copySize=" << copySize << " remainingEventSize=" << remainingEventSize;
					DTC_TLOG(TLVL_ReadNextDAQPacket) << "ReadNextDAQSubEventDMA so far bytes_read = " << bytes_read << " packets = " << bytes_read / 16 - sizeof(DTC_SubEventHeader) / 16;
					DTC_TLOG(TLVL_ReadNextDAQPacket) << "ReadNextDAQSubEventDMA copySize = " << copySize << " packets = " << copySize / 16;
					memcpy(const_cast<uint8_t*>(static_cast<const uint8_t*>(inmem->GetRawBufferPointer()) + bytes_read), daqDMAInfo_.currentReadPtr, copySize);
					bytes_read += buffer_size;

					// Increment by the size of the data block
					daqDMAInfo_.currentReadPtr = reinterpret_cast<char*>(daqDMAInfo_.currentReadPtr) + copySize;

					lastBufferPayloadSize = buffer_size;
					lastCopySize = copySize;
				}  // end primary continuation of multi-DMA subevent transfers

				res.swap(inmem);

				try
				{
					std::string accumulatedErrors = "";
					auto ok = res->SetupSubEvent(accumulatedErrors);  // does setup of SubEvent header + all payload
					if (!ok)
					{
						__SS__ << "SubEvent is corrupt! EWT=" << res->GetEventWindowTag() << ".\n\nAccumulated Errors: " << accumulatedErrors << __E__;
						__SS_THROW__;
					}
				}
				catch (...)
				{
					device_.spy(DTC_DMA_Engine_DAQ, 3 /* for once */ | 8 /* for wide view */);
					DTC_TLOG(TLVL_ERROR) << otsStyleStackTrace();
					throw;
				}

				output.push_back(std::move(res));

				// Update remaining buffer size for the last-read buffer to check for more subevents
				remainingBufferSize = lastBufferPayloadSize - lastCopySize;

				// Skip past the next sub-transfer DMA byte count header (same as single-buffer path)
				daqDMAInfo_.currentReadPtr = reinterpret_cast<char*>(daqDMAInfo_.currentReadPtr) + sizeof(uint64_t);
			}
			else  // SubEvent not split over multiple DMAs
			{
				// Update the packet pointers

				// lastReadPtr_ is easy...
				daqDMAInfo_.lastReadPtr = daqDMAInfo_.currentReadPtr;

				// Increment by the size of the data block + skip past next sub-transfer DMA byte count
				daqDMAInfo_.currentReadPtr = reinterpret_cast<char*>(daqDMAInfo_.currentReadPtr) + subEventByteCount + sizeof(uint64_t);
				remainingBufferSize -= subEventByteCount;

				try
				{
					std::string accumulatedErrors = "";
					auto ok = res->SetupSubEvent(accumulatedErrors);  // does setup of SubEvent header + all payload
					if (!ok)
					{
						__SS__ << "SubEvent is corrupt! EWT=" << res->GetEventWindowTag() << ".\n\nAccumulated Errors: " << accumulatedErrors << __E__;
						__SS_THROW__;
					}
				}
				catch (...)
				{
					device_.spy(DTC_DMA_Engine_DAQ, 3 /* for once */ | 8 /* for wide view */);
					DTC_TLOG(TLVL_ERROR) << otsStyleStackTrace();
					throw;
				}

				output.push_back(std::move(res));
			}
		}  // end subevent header fully in current buffer
	}      // end primary subevent extraction loop

	DTC_TLOG(TLVL_ReadNextDAQPacket) << "ReadNextDAQSubEventDMA: RETURN " << output.size() << " SubEvents";
	return output.size() > 0;
}  // end ReadNextDAQSubEventDMA()

std::unique_ptr<DTCLib::DTC_DCSReplyPacket> DTCLib::DTC::ReadNextDCSPacket(int tmo_ms)
{
	try
	{
		auto test = ReadNextPacket(DTC_DMA_Engine_DCS, tmo_ms);
		if (test == nullptr) return nullptr;  // Couldn't read new block

		__COUTT__ << "If interpreting as a DTC_DataPacket, here is the data: " << test->toJSON();

		auto output = std::make_unique<DTC_DCSReplyPacket>(*test.get());
		if (output->ROCIsCorrupt())
		{
			__SS__ << "ROC has set its DCS corrupt flag (check the ROC error bit details)!" << __E__;
			__SS_THROW__;
		}
		if (output->GetType() == DTC_DCSOperationType_InvalidS2C)
		{
			__SS__ << "DTC identifed an invalid DCS request from software!" << __E__;
			__SS_THROW__;
		}
		if (output->GetType() == DTC_DCSOperationType_Timeout)
		{
			__SS__ << "No response from the ROC at link " << output->GetLinkID() << " to the DCS request! The DTC identifed a ROC response timeout!" << __E__;
			__SS_THROW__;
		}
		if (lastDTCErrorBitsValue_ != output->GetDTCErrorBits())  // Note: DTC Error bits are only included in DCS reply packets
		{
			__COUTV__((int)output->GetDTCErrorBits());
			__COUTV__((int)lastDTCErrorBitsValue_);
			lastDTCErrorBitsValue_ = output->GetDTCErrorBits();

			__SS__ << "There was one or more errors identified in DCS handling of its ROC (a DTC Soft Reset will clear these errors):" << __E__;
			if ((lastDTCErrorBitsValue_ >> 0) & 0x1)
				ss << "\t* bit-0 is set: SERDES PLL associated with the ROC has lost lock." << __E__;
			if ((lastDTCErrorBitsValue_ >> 1) & 0x1)
				ss << "\t* bit-1 is set: SERDES clock-data-recovery associated with the ROC has lost lock." << __E__;
			if ((lastDTCErrorBitsValue_ >> 2) & 0x1)
				ss << "\t* bit-2 is set: Invalid packet (i.e., CRC mismatch) has been received from ROC." << __E__;
			if ((lastDTCErrorBitsValue_ >> 3) & 0x1)
				ss << "\t* bit-3 is set: Error in DTC handling of this ROC’s DCS requests has occurred (check the DTC error bit details)." << __E__;

			if (lastDTCErrorBitsValue_)  // throw exception if error
			{
				ss << "\n\nIf interpreting as a DTC_DataPacket, here is the data: \n"
				   << test->toJSON();
				__SS_THROW__;
			}
		}
		if ((lastDTCErrorBitsValue_ >> 2) & 0x1)  // Make sure CRC bit errors are always reported
		{
			__SS__ << "bit-2 is set: Invalid packet (i.e., CRC mismatch) has been received in response to the DCS request!" << __E__;
			ss << "\n\nIf interpreting as a DTC_DataPacket, here is the data: \n"
			   << test->toJSON();
			__SS_THROW__;
		}

		DTC_TLOG(TLVL_ReadNextDAQPacket) << output->toJSON();
		return output;
	}
	catch (...)  // make sure the dcs transaction is ended on exception
	{
		device_.end_dcs_transaction();

		__COUT_ERR__ << "Error reading the next DCS packet!" << __E__;
		throw;
	}
}

std::unique_ptr<DTCLib::DTC_DataPacket> DTCLib::DTC::ReadNextPacket(const DTC_DMA_Engine& engine, int tmo_ms)
{
	DTC_TLOG(TLVL_ReadNextDAQPacket) << "ReadNextPacket BEGIN";
	CFOandDTC_DMAs::DMAInfo* info;
	if (engine == DTC_DMA_Engine_DAQ)
		info = &daqDMAInfo_;
	else if (engine == DTC_DMA_Engine_DCS)
		info = &dcsDMAInfo_;
	else
	{
		DTC_TLOG(TLVL_ERROR) << "ReadNextPacket: Invalid DMA Engine specified!";
		throw new DTC_DataCorruptionException();
	}

	if (info->currentReadPtr != nullptr)
	{
		DTC_TLOG(TLVL_ReadNextDAQPacket) << "ReadNextPacket BEFORE BUFFER CHECK info->currentReadPtr="
										 << (void*)info->currentReadPtr << " *nextReadPtr_=0x" << std::hex
										 << *(uint16_t*)info->currentReadPtr;
	}
	else
	{
		DTC_TLOG(TLVL_ReadNextDAQPacket) << "ReadNextPacket BEFORE BUFFER CHECK info->currentReadPtr=nullptr";
	}

	auto index = CFOandDTC_DMAs::GetCurrentBuffer(info);

	// Need new buffer if GetCurrentBuffer returns -1 (no buffers) or -2 (done with all held buffers)
	if (index < 0)
	{
		DTC_TLOG(TLVL_ReadNextDAQPacket) << "ReadNextPacket Obtaining new " << (engine == DTC_DMA_Engine_DAQ ? "DAQ" : "DCS")
										 << " Buffer";

		void* oldBufferPtr = nullptr;
		if (info->buffer.size() > 0) oldBufferPtr = &info->buffer.back()[0];
		auto sts = ReadBuffer(engine, tmo_ms);  // does return code
		if (sts <= 0)
		{
			DTC_TLOG(TLVL_ReadNextDAQPacket) << "ReadNextPacket: ReadBuffer returned " << sts << ", returning nullptr";
			return nullptr;
		}
		// MUST BE ABLE TO HANDLE daqbuffer_==nullptr OR retry forever?
		info->currentReadPtr = &info->buffer.back()[0];
		DTC_TLOG(TLVL_ReadNextDAQPacket) << "ReadNextPacket info->currentReadPtr=" << (void*)info->currentReadPtr
										 << " *info->currentReadPtr=0x" << std::hex << *(unsigned*)info->currentReadPtr
										 << " lastReadPtr_=" << (void*)info->lastReadPtr;
		void* bufferIndexPointer = static_cast<uint8_t*>(info->currentReadPtr) + 4;
		if (info->currentReadPtr == oldBufferPtr && info->bufferIndex == *static_cast<uint32_t*>(bufferIndexPointer))
		{
			DTC_TLOG(TLVL_ReadNextDAQPacket)
				<< "ReadNextPacket: New buffer is the same as old. Releasing buffer and returning nullptr";
			info->currentReadPtr = nullptr;
			// We didn't actually get a new buffer...this probably means there's no more data
			// Try and see if we're merely stuck...hopefully, all the data is out of the buffers...
			device_.read_release(engine, 1);
			return nullptr;
		}
		info->bufferIndex++;

		info->currentReadPtr = reinterpret_cast<uint8_t*>(info->currentReadPtr) + 4;
		*static_cast<uint32_t*>(info->currentReadPtr) = info->bufferIndex;
		info->currentReadPtr = reinterpret_cast<uint8_t*>(info->currentReadPtr) + 4;

		index = info->buffer.size() - 1;
	}

	// Read the next packet
	DTC_TLOG(TLVL_ReadNextDAQPacket) << "ReadNextPacket reading next packet from buffer: info->currentReadPtr="
									 << (void*)info->currentReadPtr;

	auto blockByteCount = *reinterpret_cast<uint16_t*>(info->currentReadPtr);
	DTC_TLOG(TLVL_ReadNextDAQPacket) << "ReadNextPacket: blockByteCount=" << blockByteCount
									 << ", info->currentReadPtr=" << (void*)info->currentReadPtr
									 << ", *nextReadPtr=" << (int)*((uint16_t*)info->currentReadPtr);
	if (blockByteCount == 0 || blockByteCount == 0xcafe)
	{
		auto test = std::make_unique<DTC_DataPacket>(info->currentReadPtr);
		DTC_TLOG(TLVL_ReadNextDAQPacket) << "Check bad data (interpreting as DTC_DataPacket): " << test->toJSON();

		if (static_cast<size_t>(index) < info->buffer.size() - 1)
		{
			DTC_TLOG(TLVL_ReadNextDAQPacket) << "ReadNextPacket: blockByteCount=" << blockByteCount << " (the first 16-bits) is invalid, moving to next buffer";
			auto nextBufferPtr = *info->buffer[index + 1];
			info->currentReadPtr = nextBufferPtr + 8;  // Offset past DMA header
			return ReadNextPacket(engine, tmo_ms);     // Recursion
		}
		else
		{
			DTC_TLOG(TLVL_ReadNextDAQPacket)
				<< "ReadNextPacket: blockByteCount is invalid=" << blockByteCount << " (the first 16-bits), and this is the last buffer! Returning nullptr!";
			info->currentReadPtr = nullptr;
			// This buffer is invalid, release it!
			// Try and see if we're merely stuck...hopefully, all the data is out of the buffers...
			device_.read_release(engine, 1);

			__SS__ << "The DTC returned a packet with an invalid BlockByteCount=" << blockByteCount << " (the first 16-bits of the packet).\n\n";
			ss << "\n\nIf interpreting as a DTC_DataPacket, here is the data: \n"
			   << test->toJSON();
			__SS_THROW__;
			// return nullptr;
		}
	}

	auto test = std::make_unique<DTC_DataPacket>(info->currentReadPtr);
	DTC_TLOG(TLVL_ReadNextDAQPacket) << "ReadNextPacket: current+blockByteCount="
									 << (void*)(reinterpret_cast<uint8_t*>(info->currentReadPtr) + blockByteCount)
									 << ", end of dma buffer="
									 << (void*)(info->buffer[index][0] + CFOandDTC_DMAs::GetBufferByteCount(info, index) +
												8);  // +8 because first 8 bytes are not included in byte count
	if (reinterpret_cast<uint8_t*>(info->currentReadPtr) + blockByteCount >
		info->buffer[index][0] + CFOandDTC_DMAs::GetBufferByteCount(info, index) + 8)
	{
		blockByteCount = static_cast<uint16_t>(
			info->buffer[index][0] + CFOandDTC_DMAs::GetBufferByteCount(info, index) + 8 -
			reinterpret_cast<uint8_t*>(info->currentReadPtr));  // +8 because first 8 bytes are not included in byte count
		DTC_TLOG(TLVL_ReadNextDAQPacket) << "ReadNextPacket: Adjusting blockByteCount to " << blockByteCount
										 << " due to end-of-DMA condition";
		test->SetByte(0, blockByteCount & 0xFF);
		test->SetByte(1, (blockByteCount >> 8));
	}

	DTC_TLOG(TLVL_ReadNextDAQPacket) << test->toJSON();

	// Update the packet pointers

	// lastReadPtr_ is easy...
	info->lastReadPtr = info->currentReadPtr;

	// Increment by the size of the data block
	info->currentReadPtr = reinterpret_cast<char*>(info->currentReadPtr) + blockByteCount;

	DTC_TLOG(TLVL_ReadNextDAQPacket) << "ReadNextPacket: RETURN";
	return test;
}

void DTCLib::DTC::WriteDetectorEmulatorData(mu2e_databuff_t* buf, size_t sz)
{
	if (sz < dmaSize_)
	{
		sz = dmaSize_;
	}
	auto retry = 3;
	int errorCode;
	do
	{
		DTC_TLOG(TLVL_WriteDetectorEmulatorData) << "WriteDetectorEmulatorData: Writing buffer of size " << sz;
		errorCode = device_.write_data(DTC_DMA_Engine_DAQ, buf, sz);
		retry--;
	} while (retry > 0 && errorCode != 0);
	if (errorCode != 0)
	{
		DTC_TLOG(TLVL_ERROR) << "WriteDetectorEmulatorData: write_data returned " << errorCode
							 << ", throwing DTC_IOErrorException!";
		throw DTC_IOErrorException(errorCode);
	}
}

//
// Private Functions.
//   On success, returns number of bytes read.
int DTCLib::DTC::ReadBuffer(const DTC_DMA_Engine& channel, int retries /* = 10 */)
{
	mu2e_databuff_t* buffer;

	int retry = 1;
	if (retries > 0) retry = retries;

	int errorCode;
	TRACE_EXIT
	{
		DTC_TLOG(TLVL_ReadBuffer) << "ReadBuffer found " << ((errorCode > 0) ? "DATA" : "NO Data") << ". There are now " << (channel == DTC_DMA_Engine_DAQ ? daqDMAInfo_.buffer.size() : dcsDMAInfo_.buffer.size()) << " DAQ buffers held in the DTC Library";
	};

	do
	{
		DTC_TLOG(TLVL_ReadBuffer) << "ReadBuffer before device_.read_data retries=" << retries << " retry=" << retry;
		// WARNING NOTE: if there is existing data still sitting in unreleased buffer, the timeout will not add any delay
		//  read_data() on success, returns number of bytes read.
		errorCode = device_.read_data(channel, reinterpret_cast<void**>(&buffer), 1 /* tmo_ms */);
		// NOTE: Adding delay here significantly impacts data rates!
		//  if (errorCode == 0) usleep(tmo_ms*1000); //create timeout delay here to match tmo (tmo not used by read_data())

	} while (retry-- > 0 && errorCode == 0);  // error code of 0 is timeout

	if (errorCode == 0)
	{
		DTC_TLOG(TLVL_ReadBuffer) << "ReadBuffer: Device timeout occurred! ec=" << errorCode << ", rt=" << retry;
	}
	else if (errorCode < 0)
	{
		DTC_TLOG(TLVL_ERROR) << "ReadBuffer: read_data returned error code " << errorCode << ", throwing DTC_IOErrorException!";
		throw DTC_IOErrorException(errorCode);
	}
	else
	{
		DTC_TLOG(TLVL_ReadBuffer) << "ReadBuffer buffer_=" << (void*)buffer << " errorCode=" << errorCode << " *buffer_=0x"
								  << std::hex << *(unsigned*)buffer;
		if (channel == DTC_DMA_Engine_DAQ)
		{
			daqDMAInfo_.buffer.push_back(buffer);
			DTC_TLOG(TLVL_ReadBuffer) << "ReadBuffer: There are now " << daqDMAInfo_.buffer.size()
									  << " DAQ buffers held in the DTC Library";
		}
		else if (channel == DTC_DMA_Engine_DCS)
		{
			dcsDMAInfo_.buffer.push_back(buffer);
			DTC_TLOG(TLVL_ReadBuffer) << "ReadBuffer: There are now " << dcsDMAInfo_.buffer.size()
									  << " DCS buffers held in the DTC Library";
		}
	}
	return errorCode;
}  // ReadBuffer

void DTCLib::DTC::ReleaseAllBuffers(const DTC_DMA_Engine& channel)
{
	TLOG_ENTEX(1) << "ReleaseAllBuffers - channel=" << channel << "\n"
				  << otsStyleStackTrace();

	if (channel == DTC_DMA_Engine_DAQ)
	{
		daqDMAInfo_.buffer.clear();
		device_.release_all(channel);

		// clear (sub)event extraction members
		lastDMABufferWasFull_ = false;
		lastDMABufferWasMax_ = false;
		lastBufferTailCount_ = 0;
		hasLastGoodSubEventHeader_ = false;
		totalEventsParsed_ = 0;
		subEventHeaderQwsFilled_ = 0;
		needToFinishEvent_ = false;
		currentEventSize_ = 0;
		subEventByteCount_ = 0;
		extractedSubeventBytes_ = 0;
		extractedEvents_.clear();
	}
	else if (channel == DTC_DMA_Engine_DCS)
	{
		bool lock_taken_locally = false;
		if (!device_.thread_owns_dcs_lock())
		{
			device_.begin_dcs_transaction();
			lock_taken_locally = true;
		}

		dcsDMAInfo_.buffer.clear();
		device_.release_all(channel);

		if (lock_taken_locally) { device_.end_dcs_transaction(); }
	}
}

// ReleaseBuffers releases the buffers that are held by ReadBuffer,
// to release all DMA buffers and force hw and sw to align (for DCS) use ReleaseAllBuffers
void DTCLib::DTC::ReleaseBuffers(const DTC_DMA_Engine& channel)  //, int count)//count==0 means all
{
	DTC_TLOG(TLVL_ReleaseBuffers) << "ReleaseBuffers BEGIN";
	CFOandDTC_DMAs::DMAInfo* info;
	if (channel == DTC_DMA_Engine_DAQ)
		info = &daqDMAInfo_;
	else if (channel == DTC_DMA_Engine_DCS)
		info = &dcsDMAInfo_;
	else
	{
		DTC_TLOG(TLVL_ERROR) << "ReleaseBuffers: Invalid DMA Engine specified!";
		throw new DTC_DataCorruptionException();
	}

	auto releaseBufferCount = CFOandDTC_DMAs::GetCurrentBuffer(info);  // not GetCurrentBuffer(info), but rather Count!!!!
	if (releaseBufferCount > 0)
	{
		DTC_TLOG(TLVL_ReleaseBuffers) << "ReleaseBuffers releasing " << releaseBufferCount << " "
									  << (channel == DTC_DMA_Engine_DAQ ? "DAQ" : "DCS") << " buffers.";

		if (channel == DTC_DMA_Engine_DCS)
			device_.begin_dcs_transaction();
		device_.read_release(channel, releaseBufferCount);
		if (channel == DTC_DMA_Engine_DCS)
			device_.end_dcs_transaction();

		for (int ii = 0; ii < releaseBufferCount; ++ii)
		{
			info->buffer.pop_front();
		}
	}
	// else
	// {
	// 	DTC_TLOG(TLVL_ReleaseBuffers) << "ReleaseBuffers releasing ALL " << (channel == DTC_DMA_Engine_DAQ ? "DAQ" : "DCS")
	// 							  << " buffers.";
	// 	ReleaseAllBuffers(channel);
	// }
	DTC_TLOG(TLVL_ReleaseBuffers) << "ReleaseBuffers END";
}

// int DTCLib::DTC::GetCurrentBuffer(DMAInfo* info)
// {
// 	DTC_TLOG(TLVL_GetCurrentBuffer) << "GetCurrentBuffer BEGIN currentReadPtr=" << (void*)info->currentReadPtr
// 									<< " buffer.size()=" << info->buffer.size();

// 	if (info->buffer.size())  // might crash in ReleaseBuffers if currentReadPtr = nullptr ????????????????????????????????
// 	{
// 		DTC_TLOG(TLVL_GetCurrentBuffer) << "GetCurrentBuffer returning info->buffer.size()=" << info->buffer.size();
// 		return info->buffer.size();
// 	}

// 	if (info->currentReadPtr == nullptr || info->buffer.size() == 0)
// 	{
// 		DTC_TLOG(TLVL_GetCurrentBuffer) << "GetCurrentBuffer returning -1 because not currently reading a buffer";
// 		return -1;
// 	}

// #if 0
// 	for (size_t ii = 0; ii < info->buffer.size(); ++ii)
// 	{
// 		auto bufferptr = *info->buffer[ii];
// 		uint16_t bufferSize = *reinterpret_cast<uint16_t*>(bufferptr);
// 		DTC_TLOG(TLVL_GetCurrentBuffer) << "GetCurrentBuffer bufferptr="<<(void*)bufferptr<<" bufferSize="<<bufferSize;
// 		if (info->currentReadPtr > bufferptr &&
// 			info->currentReadPtr < bufferptr + bufferSize)
// 		{
// 			DTC_TLOG(TLVL_GetCurrentBuffer) << "Found matching buffer at index " << ii << ".";
// 			return ii;
// 		}
// 	}
// #endif
// 	DTC_TLOG(TLVL_GetCurrentBuffer) << "GetCurrentBuffer returning -2: Have buffers but none match read ptr position, need new";
// 	return -2;
// }

// uint16_t DTCLib::DTC::GetBufferByteCount(DMAInfo* info, size_t index)
// {
// 	if (index >= info->buffer.size()) return 0;
// 	auto bufferptr = *info->buffer[index];
// 	uint16_t bufferSize = *reinterpret_cast<uint16_t*>(bufferptr);
// 	return bufferSize;
// }

// This is on DMA Channel 1 (i.e., DCS)
void DTCLib::DTC::WriteDataPacket(const DTC_DataPacket& packet)
{
	DTC_TLOG(TLVL_WriteDataPacket) << "WriteDataPacket: Writing DCS DMA packet: " << packet.toJSON();
	mu2e_databuff_t buf;
	uint64_t size = packet.GetSize() + sizeof(uint64_t);
	//	uint64_t packetSize = packet.GetSize();
	if (size < static_cast<uint64_t>(dmaSize_)) size = dmaSize_;

	bzero(&buf[0], size);
	memcpy(&buf[0], &size, sizeof(uint64_t));
	memcpy(&buf[8], packet.GetData(), packet.GetSize() * sizeof(uint8_t));

	Utilities::PrintBuffer(buf, size, 0, TLVL_TRACE + 30);

	bool lock_taken_locally = false;
	if (!device_.thread_owns_dcs_lock())
	{
		device_.begin_dcs_transaction();
		lock_taken_locally = true;
	}

	auto retry = 3;
	int errorCode;
	do
	{
		DTC_TLOG(TLVL_WriteDataPacket) << "Attempting to write DCS DMA data...";
		errorCode = device_.write_data(DTC_DMA_Engine_DCS, &buf, size);
		DTC_TLOG(TLVL_WriteDataPacket) << "Attempted to write DCS DMA data, errorCode=" << errorCode << ", retries=" << retry;
		retry--;
	} while (retry > 0 && errorCode != 0);

	if (lock_taken_locally)
		device_.end_dcs_transaction();

	if (errorCode != 0)
	{
		DTC_TLOG(TLVL_ERROR) << "WriteDataPacket: write_data DCS DMA returned " << errorCode << ", throwing DTC_IOErrorException! lock_taken_locally=" << lock_taken_locally;
		throw DTC_IOErrorException(errorCode);
	}
}

void DTCLib::DTC::WriteDMAPacket(const DTC_DMAPacket& packet)
{
	WriteDataPacket(packet.ConvertToDataPacket());
}
