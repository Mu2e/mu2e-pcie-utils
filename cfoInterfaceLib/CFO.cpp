
#include "TRACE/tracemf.h"
#define TRACE_NAME "CFO.cpp"

#include "CFO.h"
#define TLVL_GetData TLVL_DEBUG + 5
#define TLVL_ReadBuffer TLVL_DEBUG + 7
#define TLVL_ReadNextDAQPacket TLVL_DEBUG + 8
#define TLVL_ReleaseBuffers TLVL_DEBUG + 20
#define TLVL_GetCurrentBuffer TLVL_DEBUG + 21

#include "dtcInterfaceLib/otsStyleCoutMacros.h"

#define CFO_TLOG(lvl) TLOG(lvl) << "CFO " << device_.getDeviceUID() << ": "
#undef __COUT_HDR__
#define __COUT_HDR__ "CFO " << device_.getDeviceUID() << ": "

#include <unistd.h>
#include <fstream>
#include <iostream>
#include <sstream>  // Convert uint to hex string

CFOLib::CFO::CFO(DTC_SimMode mode, int cfo, std::string expectedDesignVersion, bool skipInit, const std::string& uid)
	: CFO_Registers(mode, cfo, expectedDesignVersion, skipInit, uid), daqDMAInfo_()  //, dcsDMAInfo_()
{
	__COUT_INFO__ << "CONSTRUCTOR";
}

CFOLib::CFO::~CFO()
{
	__COUT_INFO__ << "DESTRUCTOR";
	// TLOG_ENTEX(-6);
	TRACE_EXIT
	{
		__COUT__ << "DESTRUCTOR exit";
	};

	// do not want to release all from destructor, in case this instance of the device is not the 'owner' of a DMA-channel
	//	so do not call ReleaseAllBuffers(CFO_DMA_Engine_DAQ) or ReleaseAllBuffers(CFO_DMA_Engine_RunPlan);
}

//
// DMA Functions
//
bool CFOLib::CFO::GetData(std::vector<std::unique_ptr<CFOLib::CFO_Event>>& output,
						  DTC_EventWindowTag when, bool matchEventWindowTag)
{
	CFO_TLOG(TLVL_GetData) << "GetCFOEventData begin EventWindowTag=" << when.GetEventWindowTag(true) << ", matching=" << (matchEventWindowTag ? "true" : "false");
	// std::vector<std::unique_ptr<CFO_Event>> output;
	output.clear();  // start out with vector empty
	bool result = false;

	// Release read buffers here "I am done with everything I read before" (because the return may be pointers to the raw data, not copies)
	ReleaseBuffers(DTC_DMA_Engine_DAQ);  // Currently race condition because GetCurrentBuffer(info) is used inside to decide how many buffers to release.

	try
	{
		// Read the next CFO_Event
		auto tries = 0;
		while (!result && tries < 3)
		{
			CFO_TLOG(TLVL_GetData) << "GetCFOEventData before ReadNextCFORecordDMA(...), tries = " << tries;
			result = ReadNextCFORecordDMA(output, 100 /* ms */);
			if (result)
			{
				if (output.size() == 0)  // check that output vector is not empty

				{
					__SS__ << "Impossible empty ouput vector after returned success!" << __E__;
					__SS_THROW__;
				}

				CFO_TLOG(TLVL_GetData) << "GetCFOEventData after ReadNextCFORecordDMA, found tag = "
									   << output.back()->GetEventWindowTag().GetEventWindowTag(true) << " (0x"
									   << std::hex << output.back()->GetEventWindowTag().GetEventWindowTag(true) << "), expected tag = "
									   << std::dec << when.GetEventWindowTag(true) << " (0x" << std::hex << when.GetEventWindowTag(true) << ")";
			}
			else
				CFO_TLOG(TLVL_GetData) << "GetCFOEventData after ReadNextCFORecordDMA, no data";
			tries++;
			// if (!result) usleep(5000); //causes loss of bandwidth
		}

		// return if no data found
		if (!result)
		{
			CFO_TLOG(TLVL_GetData) << "GetData: Timeout Occurred! (CFO_Event is nullptr after retries); no data found; RETURNing output.size()=" << output.size();
			return output.size();
		}

		// return if failed to match
		if (matchEventWindowTag && output[0]->GetEventWindowTag() != when)
		{
			CFO_TLOG(TLVL_ERROR) << "GetData: Error: CFO_Event has wrong Event Window Tag! 0x" << std::hex << when.GetEventWindowTag(true)
								 << "(expected) != 0x" << std::hex << output[0]->GetEventWindowTag().GetEventWindowTag(true);
			// packet.reset(nullptr);
			daqDMAInfo_.currentReadPtr = daqDMAInfo_.lastReadPtr;
			return output.size();
		}

		// // increment for next packet search
		// when = DTC_EventWindowTag(packet->GetEventWindowTag().GetEventWindowTag(true) + 1);

		// CFO_TLOG(TLVL_GetData) << "GetData: Adding CFO_Event tag = " << packet->GetEventWindowTag().GetEventWindowTag(true) << " to the list, ptr=" << (void*)daqDMAInfo_.lastReadPtr;
		// output.push_back(std::move(packet));
	}
	catch (DTC_WrongPacketTypeException& ex)
	{
		daqDMAInfo_.currentReadPtr = nullptr;
		CFO_TLOG(TLVL_ERROR) << "GetData: Bad omen: Wrong packet type at the current read position";
		device_.spy(DTC_DMA_Engine_DAQ, 3 /* for once */ | 8 /* for wide view */ | 16 /* for stack trace */);
		throw;
	}
	catch (DTC_IOErrorException& ex)
	{
		daqDMAInfo_.currentReadPtr = nullptr;
		CFO_TLOG(TLVL_ERROR) << "GetData: IO Exception Occurred!";
		device_.spy(DTC_DMA_Engine_DAQ, 3 /* for once */ | 8 /* for wide view */ | 16 /* for stack trace */);
		throw;
	}
	catch (DTC_DataCorruptionException& ex)
	{
		daqDMAInfo_.currentReadPtr = nullptr;
		CFO_TLOG(TLVL_ERROR) << "GetData: Data Corruption Exception Occurred!";
		device_.spy(DTC_DMA_Engine_DAQ, 3 /* for once */ | 8 /* for wide view */ | 16 /* for stack trace */);
		throw;
	}

	CFO_TLOG(TLVL_GetData) << "GetCFOEventData RETURN output.size()=" << output.size()
						   << " first tag=" << output[0]->GetEventWindowTag().GetEventWindowTag(true)
						   << " last tag=" << output.back()->GetEventWindowTag().GetEventWindowTag(true);
	return output.size();
}  // GetData

bool CFOLib::CFO::ReadNextCFORecordDMA(std::vector<std::unique_ptr<CFO_Event>>& output, int tmo_ms)
{
	TRACE_EXIT
	{
		CFO_TLOG(TLVL_ReadNextDAQPacket) << "ReadNextCFORecordDMA EXIT"
										 << " currentReadPtr=" << (void*)daqDMAInfo_.currentReadPtr
										 << " lastReadPtr=" << (void*)daqDMAInfo_.lastReadPtr
										 << " buffer.size()=" << daqDMAInfo_.buffer.size();
	};

	CFO_TLOG(TLVL_ReadNextDAQPacket) << "ReadNextCFORecordDMA BEGIN";

	if (daqDMAInfo_.currentReadPtr != nullptr)
	{
		CFO_TLOG(TLVL_ReadNextDAQPacket) << "ReadNextCFORecordDMA BEFORE BUFFER CHECK daqDMAInfo_.currentReadPtr="
										 << (void*)daqDMAInfo_.currentReadPtr << " currentBufferTransferSize=0x" << std::hex
										 << *(uint16_t*)daqDMAInfo_.currentReadPtr;
	}
	else
	{
		CFO_TLOG(TLVL_ReadNextDAQPacket) << "ReadNextCFORecordDMA BEFORE BUFFER CHECK daqDMAInfo_.currentReadPtr=nullptr";
	}

	auto index = CFOandDTC_DMAs::GetCurrentBuffer(&daqDMAInfo_);  // if buffers onhand, returns daqDMAInfo_.buffer.size().. which is count used by ReleaseBuffers()

	size_t metaBufferSize = 0;

	// Need new starting subevent buffer if GetCurrentBuffer returns -1 (no buffers) or -2 (done with all held buffers)
	if (index < 0)
	{
		CFO_TLOG(TLVL_ReadNextDAQPacket) << "ReadNextCFORecordDMA Obtaining new DAQ Buffer";

		void* oldBufferPtr = nullptr;
		if (daqDMAInfo_.buffer.size() > 0) oldBufferPtr = &daqDMAInfo_.buffer.back()[0];
		auto sts = ReadBuffer(DTC_DMA_Engine_DAQ, tmo_ms);  // does return code
		if (sts <= 0)
		{
			CFO_TLOG(TLVL_ReadNextDAQPacket) << "ReadNextCFORecordDMA: ReadBuffer returned " << sts << ", returning nullptr";
			return false;
		}
		metaBufferSize = sts;

		// MUST BE ABLE TO HANDLE daqbuffer_==nullptr OR retry forever?
		daqDMAInfo_.currentReadPtr = &daqDMAInfo_.buffer.back()[0];
		CFO_TLOG(TLVL_ReadNextDAQPacket) << "ReadNextCFORecordDMA daqDMAInfo_.currentReadPtr=" << (void*)daqDMAInfo_.currentReadPtr
										 << " currentBufferTransferSize=0x" << std::hex << *(unsigned*)daqDMAInfo_.currentReadPtr
										 << " lastReadPtr=0x" << (void*)daqDMAInfo_.lastReadPtr
										 << " metaBufferSize=0x" << metaBufferSize;
		void* bufferIndexPointer = static_cast<uint8_t*>(daqDMAInfo_.currentReadPtr) + 4;
		if (daqDMAInfo_.currentReadPtr == oldBufferPtr && daqDMAInfo_.bufferIndex == *static_cast<uint32_t*>(bufferIndexPointer))
		{
			daqDMAInfo_.currentReadPtr = nullptr;
			// We didn't actually get a new buffer...this probably means there's no more data
			// Try and see if we're merely stuck...hopefully, all the data is out of the buffers...
			device_.read_release(DTC_DMA_Engine_DAQ, 1);
			CFO_TLOG(TLVL_WARN)
				<< "ReadNextCFORecordDMA: New buffer was the same as old. Released buffer and returning nullptr";
			return false;
		}
		daqDMAInfo_.bufferIndex++;

		daqDMAInfo_.currentReadPtr = static_cast<uint8_t*>(daqDMAInfo_.currentReadPtr) + 4;
		*static_cast<uint32_t*>(daqDMAInfo_.currentReadPtr) = daqDMAInfo_.bufferIndex;
		daqDMAInfo_.currentReadPtr = static_cast<uint8_t*>(daqDMAInfo_.currentReadPtr) + 4;

		index = daqDMAInfo_.buffer.size() - 1;
		CFO_TLOG(TLVL_ReadNextDAQPacket) << "Creating CFO_EventRecord(s) from new DMA Buffer, index=" << index << " in " << daqDMAInfo_.buffer.size() << " buffers.";
	}
	else  // buffer already onhand
	{
		__SS__ << "Impossible buffer already onhand!" << __E__;
		__SS_THROW__;

		// CFO_TLOG(TLVL_ReadNextDAQPacket) << "Creating CFO_EventRecord from current DMA Buffer, index=" << index <<
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
	// auto res = std::make_unique<CFO_Event>(daqDMAInfo_.currentReadPtr);
	// complete data was copied into CFO Event Record

	// auto subEventByteCount = res->GetSubEventByteCount();
	// if (subEventByteCount < sizeof(CFO_EventRecord))
	// {
	// 	__SS__ << "SubEvent inclusive byte count cannot be less than the size of the subevent header (" << sizeof(CFO_EventRecord) << "-bytes)!" << __E__;
	// 	__SS_THROW__;
	// }
	size_t remainingBufferSize = CFOandDTC_DMAs::GetBufferByteCount(&daqDMAInfo_, index);
	CFO_TLOG(TLVL_ReadNextDAQPacket) << "sizeof(CFO_EventRecord) = " << sizeof(CFO_EventRecord)
									 << " GetBufferByteCount=" << remainingBufferSize
									 << " metaBufferSize=" << metaBufferSize;

	if (metaBufferSize == 65536)  // how was this happening? from 16 bits
	{
		__SS__ << "Impossible metaBufferSize of " << metaBufferSize << __E__;
		__SS_THROW__;
	}

	remainingBufferSize = metaBufferSize - 1;  // reset to the meta data value (dont use the DMA transfer size count because the CFO stacks transfers!) minus one (for +1 tlast)

	// 	if (0)  // for deubbging
	// 	{
	// 		std::cout << "1st DMA buffer res size=" << remainingBufferSize << "\n";
	// 		auto ptr = reinterpret_cast<const uint8_t*>(res->GetRawBufferPointer());
	// 		for (size_t i = 0; i < remainingBufferSize + 16; i += 4)
	// 			std::cout << std::dec << "res#" << i << "/" << remainingBufferSize << "(" << i / 16 << "/" << remainingBufferSize / 16 << ")" << std::hex << std::setw(8) << std::setfill('0') << *((uint32_t*)(&(ptr[i]))) << std::endl;
	// 	}

	uint64_t lastTag = -1;
	while (remainingBufferSize >= sizeof(CFO_EventRecord))
	{
		remainingBufferSize -= sizeof(uint64_t);  // remove DMA transfer size from remaining byte count

		auto res = std::make_unique<CFO_Event>(daqDMAInfo_.currentReadPtr);

		CFO_TLOG(TLVL_ReadNextDAQPacket) << "subevent tag=" << res->GetEventWindowTag().GetEventWindowTag(true) << std::hex << "(0x" << res->GetEventWindowTag().GetEventWindowTag(true) << ")"
										 << " inclusive byte count: 0x" << std::hex << sizeof(CFO_EventRecord) << " (" << std::dec << sizeof(CFO_EventRecord) << ")"
										 << ", remaining buffer size: 0x" << std::hex << remainingBufferSize << " (" << std::dec << remainingBufferSize << ")";

		if (res->GetEventWindowTag().GetEventWindowTag(true) == lastTag)
		{
			__SS__ << "Impossible repeating tag " << lastTag << __E__;
			__SS_THROW__;
		}

		lastTag = res->GetEventWindowTag().GetEventWindowTag(true);

		daqDMAInfo_.currentReadPtr = static_cast<uint8_t*>(daqDMAInfo_.currentReadPtr) + sizeof(CFO_EventRecord) + sizeof(uint64_t);
		remainingBufferSize -= sizeof(CFO_EventRecord);

		output.push_back(std::move(res));

		CFO_TLOG(TLVL_ReadNextDAQPacket) << "CFO subevent record JSON=" << output.back()->GetEventRecord().toJson();
	}  // end primary CFO Event Record extraction loop

	// check that size is multiple of CFO Event Record side sanity
	if (remainingBufferSize != 0)
	{
		__SS__ << "Impossible remainingBufferSize of " << remainingBufferSize << __E__;
		__SS_THROW__;
	}

	// // Check if there are more than one CFO_EventRecords
	// if (subEventByteCount > remainingBufferSize)
	// {
	// 	CFO_TLOG(TLVL_ReadNextDAQPacket) << "subevent needs more data by bytes " << std::hex << subEventByteCount - remainingBufferSize << " (" << std::dec << subEventByteCount - remainingBufferSize << ") packets " << (subEventByteCount - remainingBufferSize) / 16 << ". subEventByteCount=" << subEventByteCount << " remainingBufferSize=" << remainingBufferSize;

	// 	// We're going to set lastReadPtr here, so that if this buffer isn't used by GetData, we start at the beginning of this event next time
	// 	daqDMAInfo_.lastReadPtr = static_cast<uint8_t*>(daqDMAInfo_.currentReadPtr);

	// 	auto inmem = std::make_unique<CFO_EventRecord>(subEventByteCount);

	// 	memcpy(const_cast<void*>(inmem->GetRawBufferPointer()), res->GetRawBufferPointer(), remainingBufferSize);

	// 	if (0)  // for deubbging
	// 	{
	// 		std::cout << "1st DMA buffer inmem size=" << remainingBufferSize << "\n";
	// 		auto ptr = reinterpret_cast<const uint8_t*>(inmem->GetRawBufferPointer());
	// 		for (size_t i = 0; i < remainingBufferSize + 16; i += 4)
	// 			std::cout << std::dec << "inmem#" << i << "(" << i / 16 << ")" << std::hex << std::setw(8) << std::setfill('0') << *((uint32_t*)(&(ptr[i]))) << std::endl;
	// 	}

	// 	auto bytes_read = remainingBufferSize;
	// 	while (bytes_read < subEventByteCount)
	// 	{
	// 		CFO_TLOG(TLVL_ReadNextDAQPacket) << "ReadNextCFORecordDMA tag=" << inmem->GetEventWindowTag().GetEventWindowTag(true) << std::hex << "(0x" << inmem->GetEventWindowTag().GetEventWindowTag(true) << ")"
	// 										 << " Obtaining new DAQ Buffer, bytes_read=" << bytes_read << ", subEventByteCount=" << subEventByteCount;

	// 		void* oldBufferPtr = nullptr;
	// 		if (daqDMAInfo_.buffer.size() > 0) oldBufferPtr = &daqDMAInfo_.buffer.back()[0];

	// 		if (0)  // for deubbging
	// 		{
	// 			std::cout << "1st DMA buffer\n";
	// 			auto ptr = reinterpret_cast<const uint8_t*>(&daqDMAInfo_.buffer.back()[0]);
	// 			for (size_t i = 0; i < bytes_read + 32; i += 4)
	// 				std::cout << std::dec << "#" << i << "(" << i / 16 << ")" << std::hex << std::setw(8) << std::setfill('0') << *((uint32_t*)(&(ptr[i]))) << std::endl;
	// 		}

	// 		int sts;
	// 		int retry = 10;
	// 		// timeout is an exception at this point because no way to resolve partial subevent record!
	// 		while ((sts = ReadBuffer(CFO_DMA_Engine_DAQ, 10 /* retries */))  // does return code
	// 				   <= 0 &&
	// 			   --retry > 0) usleep(1000);

	// 		if (sts <= 0)
	// 		{
	// 			__SS__ << "Timeout of " << tmo_ms << " ms after receiving only partial subevent! Subevent tag=" << inmem->GetEventWindowTag().GetEventWindowTag(true) << std::hex << "(0x" << inmem->GetEventWindowTag().GetEventWindowTag(true) << ")";
	// 			__SS_THROW__;
	// 		}

	// 		daqDMAInfo_.currentReadPtr = &daqDMAInfo_.buffer.back()[0];
	// 		CFO_TLOG(TLVL_ReadNextDAQPacket) << "ReadNextCFORecordDMA daqDMAInfo_.currentReadPtr=" << (void*)daqDMAInfo_.currentReadPtr
	// 										 << " *daqDMAInfo_.currentReadPtr=0x" << std::hex << *(unsigned*)daqDMAInfo_.currentReadPtr
	// 										 << " lastReadPtr=" << (void*)daqDMAInfo_.lastReadPtr;

	// 		if (0)  // for deubbging
	// 		{
	// 			std::cout << "1st buffer\n";
	// 			auto ptr = reinterpret_cast<const uint8_t*>(inmem->GetRawBufferPointer());
	// 			for (size_t i = 0; i < bytes_read + 16; i += 4)
	// 				std::cout << std::dec << "#" << i << "(" << i / 16 << ")" << std::hex << std::setw(8) << std::setfill('0') << *((uint32_t*)(&(ptr[i]))) << std::endl;
	// 		}

	// 		void* bufferIndexPointer = static_cast<uint8_t*>(daqDMAInfo_.currentReadPtr) + 4;
	// 		if (daqDMAInfo_.currentReadPtr == oldBufferPtr && daqDMAInfo_.bufferIndex == *static_cast<uint32_t*>(bufferIndexPointer))
	// 		{
	// 			// We didn't actually get a new buffer...this probably means there's no more data
	// 			// timeout is an exception at this point because no way to resolve partial subevent record!
	// 			__SS__ << "Received same buffer twice, only received partial subevent!! Subevent tag=" << inmem->GetEventWindowTag().GetEventWindowTag(true) << std::hex << "(0x" << inmem->GetEventWindowTag().GetEventWindowTag(true) << ")";
	// 			__SS_THROW__;
	// 		}
	// 		daqDMAInfo_.bufferIndex++;

	// 		size_t buffer_size = *static_cast<uint16_t*>(daqDMAInfo_.currentReadPtr);
	// 		daqDMAInfo_.currentReadPtr = static_cast<uint8_t*>(daqDMAInfo_.currentReadPtr) + 4;
	// 		*static_cast<uint32_t*>(daqDMAInfo_.currentReadPtr) = daqDMAInfo_.bufferIndex;
	// 		daqDMAInfo_.currentReadPtr = static_cast<uint8_t*>(daqDMAInfo_.currentReadPtr) + 4;

	// 		size_t remainingEventSize = subEventByteCount - bytes_read;
	// 		size_t copySize = remainingEventSize < buffer_size - 8 ? remainingEventSize : buffer_size - 8;

	// 		if (0)  // for deubbging
	// 		{
	// 			std::cout << "2nd buffer\n";
	// 			auto ptr = reinterpret_cast<const uint8_t*>(daqDMAInfo_.currentReadPtr);
	// 			for (size_t i = 0; i < copySize; i += 4)
	// 				std::cout << std::dec << "#" << i << "(" << i / 16 << ")" << std::hex << std::setw(8) << std::setfill('0') << *((uint32_t*)(&(ptr[i]))) << std::endl;
	// 		}

	// 		CFO_TLOG(TLVL_ReadNextDAQPacket) << "ReadNextCFORecordDMA bytes_read = " << bytes_read << " packets = " << bytes_read / 16 - sizeof(CFO_EventRecord) / 16;
	// 		CFO_TLOG(TLVL_ReadNextDAQPacket) << "ReadNextCFORecordDMA copySize = " << copySize << " packets = " << copySize / 16;
	// 		memcpy(const_cast<uint8_t*>(static_cast<const uint8_t*>(inmem->GetRawBufferPointer()) + bytes_read), daqDMAInfo_.currentReadPtr, copySize);
	// 		bytes_read += buffer_size - 8;

	// 		// Increment by the size of the data block
	// 		daqDMAInfo_.currentReadPtr = reinterpret_cast<char*>(daqDMAInfo_.currentReadPtr) + copySize;
	// 	}  // end primary continuation of multi-DMA subevent transfers

	// 	res.swap(inmem);
	// }
	// else  // SubEvent not split over multiple DMAs
	// {
	// 	// Update the packet pointers

	// 	// lastReadPtr_ is easy...
	// 	daqDMAInfo_.lastReadPtr = daqDMAInfo_.currentReadPtr;

	// 	// Increment by the size of the data block
	// 	daqDMAInfo_.currentReadPtr = reinterpret_cast<char*>(daqDMAInfo_.currentReadPtr) + res->GetSubEventByteCount();
	// }

	// try
	// {
	// 	res->SetupSubEvent();  // does setup of SubEvent header + all payload
	// }
	// catch (...)
	// {
	// 	device_.spy(CFO_DMA_Engine_DAQ, 3 /* for once */ | 8 /* for wide view */);
	// 	CFO_TLOG(TLVL_ERROR) << otsStyleStackTrace();
	// 	throw;
	// }

	CFO_TLOG(TLVL_ReadNextDAQPacket) << "ReadNextCFORecordDMA: RETURN vector of " << output.size() << " CFO Event Records";
	// return res;
	return true;
}  // end ReadNextCFORecordDMA()

std::unique_ptr<CFOLib::CFO_DataPacket> CFOLib::CFO::ReadNextPacket(const DTC_DMA_Engine& engine, int tmo_ms)
{
	CFO_TLOG(TLVL_ReadNextDAQPacket) << "ReadNextPacket BEGIN";
	CFOandDTC_DMAs::DMAInfo* info;
	if (engine == DTC_DMA_Engine_DAQ)
		info = &daqDMAInfo_;
	// else if (engine == DTC_DMA_Engine_DCS)
	// 	info = &dcsDMAInfo_;
	else
	{
		__COUT_ERR__ << "ReadNextPacket: Invalid DMA Engine specified!";
		throw new DTC_DataCorruptionException();
	}

	if (info->currentReadPtr != nullptr)
	{
		CFO_TLOG(TLVL_ReadNextDAQPacket) << "ReadNextPacket BEFORE BUFFER CHECK info->currentReadPtr="
										 << (void*)info->currentReadPtr << " *nextReadPtr_=0x" << std::hex
										 << *(uint16_t*)info->currentReadPtr;
	}
	else
	{
		CFO_TLOG(TLVL_ReadNextDAQPacket) << "ReadNextPacket BEFORE BUFFER CHECK info->currentReadPtr=nullptr";
	}

	auto index = CFOandDTC_DMAs::GetCurrentBuffer(info);

	// Need new buffer if GetCurrentBuffer returns -1 (no buffers) or -2 (done with all held buffers)
	if (index < 0)
	{
		CFO_TLOG(TLVL_ReadNextDAQPacket) << "ReadNextPacket Obtaining new " << (engine == DTC_DMA_Engine_DAQ ? "DAQ" : "DCS")
										 << " Buffer";

		void* oldBufferPtr = nullptr;
		if (info->buffer.size() > 0) oldBufferPtr = &info->buffer.back()[0];
		auto sts = ReadBuffer(engine, tmo_ms);  // does return code
		if (sts <= 0)
		{
			CFO_TLOG(TLVL_ReadNextDAQPacket) << "ReadNextPacket: ReadBuffer returned " << sts << ", returning nullptr";
			return nullptr;
		}
		// MUST BE ABLE TO HANDLE daqbuffer_==nullptr OR retry forever?
		info->currentReadPtr = &info->buffer.back()[0];
		CFO_TLOG(TLVL_ReadNextDAQPacket) << "ReadNextPacket info->currentReadPtr=" << (void*)info->currentReadPtr
										 << " *info->currentReadPtr=0x" << std::hex << *(unsigned*)info->currentReadPtr
										 << " lastReadPtr_=" << (void*)info->lastReadPtr;
		void* bufferIndexPointer = static_cast<uint8_t*>(info->currentReadPtr) + 2;
		if (info->currentReadPtr == oldBufferPtr && info->bufferIndex == *static_cast<uint32_t*>(bufferIndexPointer))
		{
			CFO_TLOG(TLVL_ReadNextDAQPacket)
				<< "ReadNextPacket: New buffer is the same as old. Releasing buffer and returning nullptr";
			info->currentReadPtr = nullptr;
			// We didn't actually get a new buffer...this probably means there's no more data
			// Try and see if we're merely stuck...hopefully, all the data is out of the buffers...
			device_.read_release(engine, 1);
			return nullptr;
		}
		info->bufferIndex++;

		info->currentReadPtr = reinterpret_cast<uint8_t*>(info->currentReadPtr) + 2;
		*static_cast<uint32_t*>(info->currentReadPtr) = info->bufferIndex;
		info->currentReadPtr = reinterpret_cast<uint8_t*>(info->currentReadPtr) + 6;

		index = info->buffer.size() - 1;
	}

	// Read the next packet
	CFO_TLOG(TLVL_ReadNextDAQPacket) << "ReadNextPacket reading next packet from buffer: info->currentReadPtr="
									 << (void*)info->currentReadPtr;

	auto blockByteCount = *reinterpret_cast<uint16_t*>(info->currentReadPtr);
	CFO_TLOG(TLVL_ReadNextDAQPacket) << "ReadNextPacket: blockByteCount=" << blockByteCount
									 << ", info->currentReadPtr=" << (void*)info->currentReadPtr
									 << ", *nextReadPtr=" << (int)*((uint16_t*)info->currentReadPtr);
	if (blockByteCount == 0 || blockByteCount == 0xcafe)
	{
		if (static_cast<size_t>(index) < info->buffer.size() - 1)
		{
			CFO_TLOG(TLVL_ReadNextDAQPacket) << "ReadNextPacket: blockByteCount is invalid, moving to next buffer";
			auto nextBufferPtr = *info->buffer[index + 1];
			info->currentReadPtr = nextBufferPtr + 8;  // Offset past DMA header
			return ReadNextPacket(engine, tmo_ms);     // Recursion
		}
		else
		{
			CFO_TLOG(TLVL_ReadNextDAQPacket)
				<< "ReadNextPacket: blockByteCount is invalid, and this is the last buffer! Returning nullptr!";
			info->currentReadPtr = nullptr;
			// This buffer is invalid, release it!
			// Try and see if we're merely stuck...hopefully, all the data is out of the buffers...
			device_.read_release(engine, 1);
			return nullptr;
		}
	}

	auto test = std::make_unique<CFO_DataPacket>(info->currentReadPtr);
	CFO_TLOG(TLVL_ReadNextDAQPacket) << "ReadNextPacket: current+blockByteCount="
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
		CFO_TLOG(TLVL_ReadNextDAQPacket) << "ReadNextPacket: Adjusting blockByteCount to " << blockByteCount
										 << " due to end-of-DMA condition";
		test->SetByte(0, blockByteCount & 0xFF);
		test->SetByte(1, (blockByteCount >> 8));
	}

	CFO_TLOG(TLVL_ReadNextDAQPacket) << test->toJSON();

	// Update the packet pointers

	// lastReadPtr_ is easy...
	info->lastReadPtr = info->currentReadPtr;

	// Increment by the size of the data block
	info->currentReadPtr = reinterpret_cast<char*>(info->currentReadPtr) + blockByteCount;

	CFO_TLOG(TLVL_ReadNextDAQPacket) << "ReadNextPacket: RETURN";
	return test;
}

//
// Private Functions.
//
int CFOLib::CFO::ReadBuffer(const DTC_DMA_Engine& channel, int tmo_ms)
{
	mu2e_databuff_t* buffer;

	int retry = 1;

	// Break long timeouts into multiple 10 ms retries
	if (tmo_ms > 10)
	{
		retry = tmo_ms / 10;
		tmo_ms = 10;
	}

	int errorCode;
	do
	{
		CFO_TLOG(TLVL_ReadBuffer) << "ReadBuffer before device_.read_data tmo=" << tmo_ms << " retry=" << retry;
		errorCode = device_.read_data(channel, reinterpret_cast<void**>(&buffer), 1 /* tmo_ms */);
		// if (errorCode == 0) usleep(1000);

	} while (retry-- > 0 && errorCode == 0);  // error code of 0 is timeout

	if (errorCode == 0)
	{
		CFO_TLOG(TLVL_ReadBuffer) << "ReadBuffer: Device timeout occurred! ec=" << errorCode << ", rt=" << retry;
	}
	else if (errorCode < 0)
	{
		CFO_TLOG(TLVL_ERROR) << "ReadBuffer: read_data returned " << errorCode << ", throwing CFO_IOErrorException!";
		throw DTC_IOErrorException(errorCode);
	}
	else
	{
		CFO_TLOG(TLVL_ReadBuffer) << "ReadBuffer buffer_=" << (void*)buffer << " errorCode=" << errorCode << " *buffer_=0x"
								  << std::hex << *(unsigned*)buffer;
		if (channel == DTC_DMA_Engine_DAQ)
		{
			daqDMAInfo_.buffer.push_back(buffer);
			CFO_TLOG(TLVL_ReadBuffer) << "ReadBuffer: There are now " << daqDMAInfo_.buffer.size()
									  << " DAQ buffers held in the DTC Library";
		}
		// else if (channel == DTC_DMA_Engine_DCS)
		// {
		// 	dcsDMAInfo_.buffer.push_back(buffer);
		// 	CFO_TLOG(TLVL_ReadBuffer) << "ReadBuffer: There are now " << dcsDMAInfo_.buffer.size()
		// 						  << " DCS buffers held in the DTC Library";
		// }
	}
	return errorCode;
}

void CFOLib::CFO::ReleaseAllBuffers(const DTC_DMA_Engine& channel)
{
	TLOG_ENTEX(1) << "ReleaseAllBuffers - channel=" << channel;

	if (channel == DTC_DMA_Engine_DAQ)
	{
		daqDMAInfo_.buffer.clear();
		device_.release_all(channel);
	}
	// else if (channel == CFO_DMA_Engine_RunPlan)
	// {
	// 	dcsDMAInfo_.buffer.clear();
	// 	device_.release_all(channel);
	// }
	else
	{
		CFO_TLOG(TLVL_ERROR) << "ReadNextPacket: Invalid DMA Engine specified!";
		throw new DTC_DataCorruptionException();
	}
}

void CFOLib::CFO::ReleaseBuffers(const DTC_DMA_Engine& channel)
{
	CFO_TLOG(TLVL_ReleaseBuffers) << "ReleaseBuffers BEGIN";
	CFOandDTC_DMAs::DMAInfo* info;
	if (channel == DTC_DMA_Engine_DAQ)
		info = &daqDMAInfo_;
	// else if (channel == CFO_DMA_Engine_RunPlan)
	// 	info = &dcsDMAInfo_;
	else
	{
		CFO_TLOG(TLVL_ERROR) << "ReadNextPacket: Invalid DMA Engine specified!\n\n"
							 << otsStyleStackTrace();
		throw new DTC_DataCorruptionException();
	}

	auto releaseBufferCount = CFOandDTC_DMAs::GetCurrentBuffer(info);
	if (releaseBufferCount > 0)
	{
		CFO_TLOG(TLVL_ReleaseBuffers) << "ReleaseBuffers releasing " << releaseBufferCount << " "
									  << (channel == DTC_DMA_Engine_DAQ ? "DAQ" : "DCS") << " buffers.";

		// if (channel == CFO_DMA_Engine_DCS)
		// 	device_.begin_dcs_transaction();
		device_.read_release(channel, releaseBufferCount);
		// if (channel == DTC_DMA_Engine_DCS)
		// 	device_.end_dcs_transaction();

		for (int ii = 0; ii < releaseBufferCount; ++ii)
		{
			info->buffer.pop_front();
		}
	}
	// else
	// {
	// 	CFO_TLOG(TLVL_ReleaseBuffers) << "ReleaseBuffers releasing ALL " << (channel == DTC_DMA_Engine_DAQ ? "DAQ" : "DCS")
	// 							  << " buffers.";
	// 	ReleaseAllBuffers(channel);
	// }
	CFO_TLOG(TLVL_ReleaseBuffers) << "ReleaseBuffers END";
}

// int CFOLib::CFO::GetCurrentBuffer(DMAInfo* info)
// {
// 	CFO_TLOG(TLVL_GetCurrentBuffer) << "GetCurrentBuffer BEGIN";
// 	if (info->currentReadPtr == nullptr || info->buffer.size() == 0)
// 	{
// 		CFO_TLOG(TLVL_GetCurrentBuffer) << "GetCurrentBuffer returning -1 because not currently reading a buffer";
// 		return -1;
// 	}

// 	for (size_t ii = 0; ii < info->buffer.size(); ++ii)
// 	{
// 		auto bufferptr = *info->buffer[ii];
// 		uint16_t bufferSize = *reinterpret_cast<uint16_t*>(bufferptr);
// 		if (info->currentReadPtr > bufferptr &&
// 			info->currentReadPtr < bufferptr + bufferSize)
// 		{
// 			CFO_TLOG(TLVL_GetCurrentBuffer) << "Found matching buffer at index " << ii << ".";
// 			return ii;
// 		}
// 	}
// 	CFO_TLOG(TLVL_GetCurrentBuffer) << "GetCurrentBuffer returning -2: Have buffers but none match, need new";
// 	return -2;
// }

// uint16_t CFOLib::CFO::GetBufferByteCount(DMAInfo* info, size_t index)
// {
// 	if (index >= info->buffer.size()) return 0;
// 	auto bufferptr = *info->buffer[index];
// 	uint16_t bufferSize = *reinterpret_cast<uint16_t*>(bufferptr);
// 	return bufferSize;
// }
