
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

// GetSubEventData ~~
//	Similart to GetData() but retrieves a SubEvent, as opposed to an Event -- This is appropriate for SW Event building or more basic tests.
std::vector<std::unique_ptr<DTCLib::DTC_SubEvent>> DTCLib::DTC::GetSubEventData(DTC_EventWindowTag when, bool matchEventWindowTag)
{
	DTC_TLOG(TLVL_GetData) << "GetSubEventData begin EventWindowTag=" << when.GetEventWindowTag(true) << ", doMatching=" << (matchEventWindowTag ? "true" : "false");
	std::vector<std::unique_ptr<DTC_SubEvent>> output;
	std::unique_ptr<DTC_SubEvent> packet = nullptr;

	// Release read buffers here "I am done with everything I read before" (because the return is pointers to the raw data, not copies)
	ReleaseBuffers(DTC_DMA_Engine_DAQ);  // Currently race condition because GetCurrentBuffer(info) is used inside to decide how many buffers to release.

	try
	{
		// Read the next DTC_SubEvent
		auto tries = 0;
		auto done = true;  // false; This code has been changed around!!!!
		do
		{
			while (packet == nullptr && tries < 3)
			{
				DTC_TLOG(TLVL_GetData) << "GetSubEventData before ReadNextDAQSubEventDMA(...), tries = " << tries;
				packet = ReadNextDAQSubEventDMA(100 /* ms */);
				if (packet != nullptr)
					DTC_TLOG(TLVL_GetData) << "GetSubEventData after ReadNextDAQSubEvent, found tag = " << packet->GetEventWindowTag().GetEventWindowTag(true) << " (0x" << std::hex << packet->GetEventWindowTag().GetEventWindowTag(true) << "), expected tag = " << std::dec << when.GetEventWindowTag(true) << " (0x" << std::hex << when.GetEventWindowTag(true) << ")";
				else
					DTC_TLOG(TLVL_GetData) << "GetSubEventData after ReadNextDAQSubEvent, packet == nullptr";
				tries++;
				// if (packet == nullptr) usleep(5000);
			}

			// return if no data found
			if (packet == nullptr)
			{
				DTC_TLOG(TLVL_GetData) << "GetData: Timeout Occurred! (DTC_SubEvent is nullptr after retries); no data found; RETURNing output.size()=" << output.size();
				return output;
			}

			// return if failed to match
			if (packet->GetEventWindowTag() != when && matchEventWindowTag)
			{
				DTC_TLOG(TLVL_ERROR) << "GetData: Error: DTC_SubEvent has wrong Event Window Tag! 0x" << std::hex << when.GetEventWindowTag(true)
									 << "(expected) != 0x" << std::hex << packet->GetEventWindowTag().GetEventWindowTag(true);
				packet.reset(nullptr);
				daqDMAInfo_.currentReadPtr = daqDMAInfo_.lastReadPtr;
				return output;
			}

			// increment for next packet search
			when = DTC_EventWindowTag(packet->GetEventWindowTag().GetEventWindowTag(true) + 1);

			DTC_TLOG(TLVL_GetData) << "GetData: Adding DTC_SubEvent tag = " << packet->GetEventWindowTag().GetEventWindowTag(true) << " to the list, ptr=" << (void*)daqDMAInfo_.lastReadPtr;
			output.push_back(std::move(packet));

		} while (!done);  // primary loop to read one subevent
	}
	catch (DTC_WrongPacketTypeException& ex)
	{
		daqDMAInfo_.currentReadPtr = nullptr;
		DTC_TLOG(TLVL_ERROR) << "GetData: Bad omen: Wrong packet type at the current read position";
		device_.spy(DTC_DMA_Engine_DAQ, 3 /* for once */ | 8 /* for wide view */ | 16 /* for stack trace */);
		throw;
	}
	catch (DTC_IOErrorException& ex)
	{
		daqDMAInfo_.currentReadPtr = nullptr;
		DTC_TLOG(TLVL_ERROR) << "GetData: IO Exception Occurred!";
		device_.spy(DTC_DMA_Engine_DAQ, 3 /* for once */ | 8 /* for wide view */ | 16 /* for stack trace */);
		throw;
	}
	catch (DTC_DataCorruptionException& ex)
	{
		daqDMAInfo_.currentReadPtr = nullptr;
		DTC_TLOG(TLVL_ERROR) << "GetData: Data Corruption Exception Occurred!";
		device_.spy(DTC_DMA_Engine_DAQ, 3 /* for once */ | 8 /* for wide view */ | 16 /* for stack trace */);
		throw;
	}

	DTC_TLOG(TLVL_GetData) << "GetSubEventData RETURN output.size()=" << output.size();
	return output;
}  // GetSubEventData

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
		   << "Restarting the DTC software instance may fix the problem and realign DMA pointers." << std::endl;

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

std::unique_ptr<DTCLib::DTC_SubEvent> DTCLib::DTC::ReadNextDAQSubEventDMA(int tmo_ms)
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

	// Need new starting subevent buffer if GetCurrentBuffer returns -1 (no buffers) or -2 (done with all held buffers)
	if (index < 0)
	{
		DTC_TLOG(TLVL_ReadNextDAQPacket) << "ReadNextDAQSubEventDMA Obtaining new DAQ Buffer";

		void* oldBufferPtr = nullptr;
		if (daqDMAInfo_.buffer.size() > 0) oldBufferPtr = &daqDMAInfo_.buffer.back()[0];
		auto sts = ReadBuffer(DTC_DMA_Engine_DAQ, tmo_ms);  // does return code
		if (sts <= 0)
		{
			DTC_TLOG(TLVL_ReadNextDAQPacket) << "ReadNextDAQSubEventDMA: ReadBuffer returned " << sts << ", returning nullptr";
			return nullptr;
		}
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
				<< "ReadNextDAQSubEventDMA: New buffer was the same as old. Released buffer and returning nullptr";
			return nullptr;
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
	auto res = std::make_unique<DTC_SubEvent>(daqDMAInfo_.currentReadPtr);  // only does setup of SubEvent header!

	auto subEventByteCount = res->GetSubEventByteCount();
	if (subEventByteCount < sizeof(DTC_SubEventHeader))
	{
		__SS__ << "SubEvent inclusive byte count cannot be less than the size of the subevent header (" << sizeof(DTC_SubEventHeader) << "-bytes)!" << __E__;
		__SS_THROW__;
	}
	size_t remainingBufferSize = CFOandDTC_DMAs::GetBufferByteCount(&daqDMAInfo_, index);
	DTC_TLOG(TLVL_ReadNextDAQPacket) << "sizeof(DTC_SubEventHeader) = " << sizeof(DTC_SubEventHeader) << " GetBufferByteCount=" << remainingBufferSize;

	remainingBufferSize -= sizeof(uint64_t);

	DTC_TLOG(TLVL_ReadNextDAQPacket) << "subevent tag=" << res->GetEventWindowTag().GetEventWindowTag(true) << std::hex << "(0x" << res->GetEventWindowTag().GetEventWindowTag(true) << ")"
									 << " inclusive byte count: 0x" << std::hex << subEventByteCount << " (" << std::dec << subEventByteCount << ") inclusive packets " << subEventByteCount / 16 << ", remaining buffer size: 0x" << std::hex << remainingBufferSize << " (" << std::dec << remainingBufferSize << ") this buffer packets = " << (remainingBufferSize - sizeof(DTC_SubEventHeader)) / 16 << ". "
									 << "Total subevent packet count: " << (subEventByteCount - sizeof(DTC_SubEventHeader)) / 16;

	// Check for continued DMA
	if (subEventByteCount > remainingBufferSize)
	{
		DTC_TLOG(TLVL_ReadNextDAQPacket) << "subevent needs more data by bytes " << std::hex << subEventByteCount - remainingBufferSize << " (" << std::dec << subEventByteCount - remainingBufferSize << ") packets " << (subEventByteCount - remainingBufferSize) / 16 << ". subEventByteCount=" << subEventByteCount << " remainingBufferSize=" << remainingBufferSize;

		// We're going to set lastReadPtr here, so that if this buffer isn't used by GetData, we start at the beginning of this event next time
		daqDMAInfo_.lastReadPtr = static_cast<uint8_t*>(daqDMAInfo_.currentReadPtr);

		auto inmem = std::make_unique<DTC_SubEvent>(subEventByteCount);

		if (0)  // for deubbging
		{
			std::cout << "1st DMA buffer res size=" << remainingBufferSize << "\n";
			auto ptr = reinterpret_cast<const uint8_t*>(res->GetRawBufferPointer());
			for (size_t i = 0; i < remainingBufferSize + 16; i += 4)
				std::cout << std::dec << "res#" << i << "/" << remainingBufferSize << "(" << i / 16 << "/" << remainingBufferSize / 16 << ")" << std::hex << std::setw(8) << std::setfill('0') << *((uint32_t*)(&(ptr[i]))) << std::endl;
		}

		memcpy(const_cast<void*>(inmem->GetRawBufferPointer()), res->GetRawBufferPointer(), remainingBufferSize);

		if (0)  // for deubbging
		{
			std::cout << "1st DMA buffer inmem size=" << remainingBufferSize << "\n";
			auto ptr = reinterpret_cast<const uint8_t*>(inmem->GetRawBufferPointer());
			for (size_t i = 0; i < remainingBufferSize + 16; i += 4)
				std::cout << std::dec << "inmem#" << i << "(" << i / 16 << ")" << std::hex << std::setw(8) << std::setfill('0') << *((uint32_t*)(&(ptr[i]))) << std::endl;
		}

		auto bytes_read = remainingBufferSize;
		while (bytes_read < subEventByteCount)
		{
			DTC_TLOG(TLVL_ReadNextDAQPacket) << "ReadNextDAQSubEventDMA tag=" << inmem->GetEventWindowTag().GetEventWindowTag(true) << std::hex << "(0x" << inmem->GetEventWindowTag().GetEventWindowTag(true) << ")"
											 << " Obtaining new DAQ Buffer, bytes_read=" << bytes_read << ", subEventByteCount=" << subEventByteCount;

			void* oldBufferPtr = nullptr;
			if (daqDMAInfo_.buffer.size() > 0) oldBufferPtr = &daqDMAInfo_.buffer.back()[0];

			if (0)  // for deubbging
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

			if (0)  // for deubbging
			{
				std::cout << "1st buffer\n";
				auto ptr = reinterpret_cast<const uint8_t*>(inmem->GetRawBufferPointer());
				for (size_t i = 0; i < bytes_read + 16; i += 4)
					std::cout << std::dec << "#" << i << "(" << i / 16 << ")" << std::hex << std::setw(8) << std::setfill('0') << *((uint32_t*)(&(ptr[i]))) << std::endl;
			}

			void* bufferIndexPointer = static_cast<uint8_t*>(daqDMAInfo_.currentReadPtr) + 4;
			if (daqDMAInfo_.currentReadPtr == oldBufferPtr && daqDMAInfo_.bufferIndex == *static_cast<uint32_t*>(bufferIndexPointer))
			{
				// We didn't actually get a new buffer...this probably means there's no more data
				// timeout is an exception at this point because no way to resolve partial subevent record!
				__SS__ << "Received same buffer twice, only received partial subevent!! Subevent tag=" << inmem->GetEventWindowTag().GetEventWindowTag(true) << std::hex << "(0x" << inmem->GetEventWindowTag().GetEventWindowTag(true) << ")";
				__SS_THROW__;
			}
			daqDMAInfo_.bufferIndex++;

			size_t buffer_size = *static_cast<uint16_t*>(daqDMAInfo_.currentReadPtr);
			daqDMAInfo_.currentReadPtr = static_cast<uint8_t*>(daqDMAInfo_.currentReadPtr) + 4;
			*static_cast<uint32_t*>(daqDMAInfo_.currentReadPtr) = daqDMAInfo_.bufferIndex;
			daqDMAInfo_.currentReadPtr = static_cast<uint8_t*>(daqDMAInfo_.currentReadPtr) + 4;

			size_t remainingEventSize = subEventByteCount - bytes_read;
			size_t copySize = remainingEventSize < buffer_size - 8 ? remainingEventSize : buffer_size - 8;

			if (0)  // for deubbging
			{
				std::cout << "2nd buffer\n";
				auto ptr = reinterpret_cast<const uint8_t*>(daqDMAInfo_.currentReadPtr);
				for (size_t i = 0; i < copySize; i += 4)
					std::cout << std::dec << "#" << i << "(" << i / 16 << ")" << std::hex << std::setw(8) << std::setfill('0') << *((uint32_t*)(&(ptr[i]))) << std::endl;
			}

			DTC_TLOG(TLVL_ReadNextDAQPacket) << "ReadNextDAQSubEventDMA bytes_read = " << bytes_read << " packets = " << bytes_read / 16 - sizeof(DTC_SubEventHeader) / 16;
			DTC_TLOG(TLVL_ReadNextDAQPacket) << "ReadNextDAQSubEventDMA copySize = " << copySize << " packets = " << copySize / 16;
			memcpy(const_cast<uint8_t*>(static_cast<const uint8_t*>(inmem->GetRawBufferPointer()) + bytes_read), daqDMAInfo_.currentReadPtr, copySize);
			bytes_read += buffer_size - 8;

			// Increment by the size of the data block
			daqDMAInfo_.currentReadPtr = reinterpret_cast<char*>(daqDMAInfo_.currentReadPtr) + copySize;
		}  // end primary continuation of multi-DMA subevent transfers

		res.swap(inmem);
	}
	else  // SubEvent not split over multiple DMAs
	{
		// Update the packet pointers

		// lastReadPtr_ is easy...
		daqDMAInfo_.lastReadPtr = daqDMAInfo_.currentReadPtr;

		// Increment by the size of the data block
		daqDMAInfo_.currentReadPtr = reinterpret_cast<char*>(daqDMAInfo_.currentReadPtr) + res->GetSubEventByteCount();
	}

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

	DTC_TLOG(TLVL_ReadNextDAQPacket) << "ReadNextDAQSubEventDMA: RETURN ptr to next SubEvent";
	return res;
}  // end ReadNextDAQSubEventDMA()

std::unique_ptr<DTCLib::DTC_DCSReplyPacket> DTCLib::DTC::ReadNextDCSPacket(int tmo_ms)
{
	try
	{
		auto test = ReadNextPacket(DTC_DMA_Engine_DCS, tmo_ms);
		if (test == nullptr) return nullptr;  // Couldn't read new block

		__COUT__ << "If interpreting as a DTC_DataPacket, here is the data: " << test->toJSON();

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
	TLOG_ENTEX(1) << "ReleaseAllBuffers - channel=" << channel;

	if (channel == DTC_DMA_Engine_DAQ)
	{
		daqDMAInfo_.buffer.clear();
		device_.release_all(channel);
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
