

#include "dtcInterfaceLib/otsStyleCoutMacros.h"

#include "Mu2eUtil.h"

void DTCLib::Mu2eUtil::parse_arguments(int argc, char** argv)
{
	for (auto optind = 1; optind < argc; ++optind)
	{
		if (argv[optind][0] == '-')
		{
			switch (argv[optind][1])
			{
				case 'i':
					incrementTimestamp = false;
					break;
				case 'd':
					delay = DTCLib::Utilities::getOptionValue(&optind, &argv);
					break;
				case 'D':
					cfodelay = DTCLib::Utilities::getOptionValue(&optind, &argv);
					break;
				case 'S':
					syncRequests = true;
					break;
				case 'n':
					number = DTCLib::Utilities::getOptionValue(&optind, &argv);
					break;
				case 'o':
					timestampOffset = DTCLib::Utilities::getOptionValueLong(&optind, &argv) & 0x0000FFFFFFFFFFFF;  // Timestamps are 48 bits
					break;
				case 'c':
					packetCount = DTCLib::Utilities::getOptionValue(&optind, &argv);
					break;
				case 'N':
					forceNoDebug = true;
					break;
				case 'b':
					blockCount = DTCLib::Utilities::getOptionValue(&optind, &argv);
					break;
				case 'E':
					eventCount = DTCLib::Utilities::getOptionValue(&optind, &argv);
					break;
				case 'a':
					requestsAhead = DTCLib::Utilities::getOptionValue(&optind, &argv);
					break;
				case 'q':
					quiet = true;
					quietCount = DTCLib::Utilities::getOptionValue(&optind, &argv);
					break;
				case 'p':
					useSimFile = true;
					break;
				case 'P':
					useSimFile = true;
					simFile = DTCLib::Utilities::getOptionValue(&optind, &argv);
					break;
				case 'G':
					readGenerated = true;
					break;
				case 'Q':
					quiet = true;
					reallyQuiet = true;
					break;
				case 's':
					checkSERDES = true;
					break;
				case 'e':
					useCFOEmulator = false;
					break;
				case 'f':
					rawOutput = true;
					rawOutputFile = DTCLib::Utilities::getOptionString(&optind, &argv);
					break;
				case 'H':
					writeDMAHeadersToOutput = true;
					break;
				case 't':
					debugType = DTC_DebugType_ExternalSerialWithReset;
					stickyDebugType = false;
					break;
				case 'T':
					val = DTCLib::Utilities::getOptionValue(&optind, &argv);
					if (val < static_cast<int>(DTC_DebugType_Invalid))
					{
						stickyDebugType = true;
						debugType = static_cast<DTC_DebugType>(val);
						break;
					}
					TLOG(TLVL_ERROR) << "Invalid Debug Type passed to -T!" << std::endl;
					printHelpMsg();
					break;
				case 'r':
					rocMask = DTCLib::Utilities::getOptionValue(&optind, &argv);
					break;
				case 'C':
					clockToProgram = DTCLib::Utilities::getOptionValue(&optind, &argv) % 3;
					break;
				case 'F':
					targetFrequency = DTCLib::Utilities::getOptionValue(&optind, &argv);
					break;
				case 'v':
					expectedDesignVersion = DTCLib::Utilities::getOptionString(&optind, &argv);
					break;
				case 'V':
					skipVerify = true;
					break;
				case '-':  // Long option
				{
					auto option = DTCLib::Utilities::getLongOptionOption(&optind, &argv);
					if (option == "--timestamp-list")
					{
						timestampFile = DTCLib::Utilities::getLongOptionString(&optind, &argv);
					}
					else if (option == "--dtc")
					{
						dtc = DTCLib::Utilities::getLongOptionValue(&optind, &argv);
					}
					else if (option == "--cfoDRP")
					{
						useCFODRP = true;
					}
					else if (option == "--heartbeats")
					{
						heartbeatsAfter = DTCLib::Utilities::getLongOptionValue(&optind, &argv);
					}
					else if (option == "--binary-file-mode")
					{
						binaryFileOutput = true;
						rawOutput = true;
						rawOutputFile = DTCLib::Utilities::getLongOptionString(&optind, &argv);
					}
					else if (option == "--stop-verify")
					{
						stopOnVerifyFailure = true;
					}
					else if (option == "--stop-on-timeout")
					{
						stopOnTimeout = true;
					}
					else if (option == "--extra-reads")
					{
						extraReads = DTCLib::Utilities::getLongOptionValue(&optind, &argv);
					}
					else if (option == "--help")
					{
						printHelpMsg();
					}
					break;
				}
				default:
					TLOG(TLVL_ERROR) << "Unknown option: " << argv[optind] << std::endl;
					printHelpMsg();
					break;
				case 'h':
					printHelpMsg();
					break;
			}
		}
		else
		{
			op = std::string(argv[optind]);
		}
	}

	TLOG(TLVL_DEBUG) << "Options are: " << std::boolalpha
					 << "Operation: " << std::string(op) << ", DTC: " << dtc << ", Num: " << number << ", Delay: " << delay
					 << ", CFO Delay: " << cfodelay << ", TS Offset: " << timestampOffset << ", PacketCount: " << packetCount
					 << ", Force NO Debug Flag: " << forceNoDebug << ", DataBlock Count: " << blockCount;
	TLOG(TLVL_DEBUG) << std::boolalpha << ", Event Count: " << eventCount << ", Requests Ahead of Reads: " << requestsAhead
					 << ", Synchronous Request Mode: " << syncRequests << ", Use DTC CFO Emulator: " << useCFOEmulator << (useCFODRP ? " (CFO Emulator DRPs)" : " (DTC DRPs)")
					 << ", Increment TS: " << incrementTimestamp << ", Quiet Mode: " << quiet << " (" << quietCount << ")"
					 << ", Really Quiet Mode: " << reallyQuiet << ", Check SERDES Error Status: " << checkSERDES;
	TLOG(TLVL_DEBUG) << std::boolalpha << ", Read Data from DDR: " << readGenerated
					 << ", Use Sim File: " << useSimFile << ", Skip Verify: " << skipVerify << ", ROC Mask: " << std::hex
					 << rocMask << ", Debug Type: " << DTC_DebugTypeConverter(debugType).toString()
					 << ", Target Frequency: " << std::dec << targetFrequency;
	TLOG(TLVL_DEBUG) << std::boolalpha << ", Clock To Program: " << (clockToProgram == 0 ? "SERDES" : (clockToProgram == 1 ? "DDR" : "Timing"))
					 << ", Expected Design Version: " << expectedDesignVersion << ", Heartbeats: " << heartbeatsAfter;
	if (rawOutput)
	{
		TLOG(TLVL_DEBUG) << ", Raw output file: " << rawOutputFile;
	}
	if (simFile.size() > 0)
	{
		TLOG(TLVL_DEBUG) << ", Sim file: " << simFile;
	}
}

void DTCLib::Mu2eUtil::read_data()
{
	TLOG(TLVL_DEBUG) << "Operation \"read_data\"" << std::endl;
	auto thisDTC = new DTC(DTC_SimMode_NoCFO, dtc, rocMask, expectedDesignVersion);

	auto device = thisDTC->GetDevice();
	if (readGenerated)
	{
		thisDTC->EnableDetectorEmulatorMode();
		thisDTC->SetDetectorEmulationDMACount(number);
		thisDTC->EnableDetectorEmulator();
	}
	for (unsigned ii = 0; ii < number; ++ii)
	{
		TLOG((reallyQuiet ? TLVL_DEBUG + 4 : TLVL_INFO)) << "Buffer Read " << ii << std::endl;
		mu2e_databuff_t* buffer;
		auto tmo_ms = 1500;
		auto sts = device->read_data(DTC_DMA_Engine_DAQ, reinterpret_cast<void**>(&buffer), tmo_ms);

		TLOG(TLVL_TRACE) << "util - read for DAQ - ii=" << ii << ", sts=" << sts << ", buffer=" << (void*)buffer;
		if (sts > 0)
		{
			auto bufSize = static_cast<uint16_t>(*reinterpret_cast<uint64_t*>(&buffer[0]));
			TLOG(TLVL_TRACE) << "util - bufSize is " << bufSize;

			if (!reallyQuiet)
			{
				DTCLib::Utilities::PrintBuffer(buffer, bufSize, quietCount);
			}
		}
		device->read_release(DTC_DMA_Engine_DAQ, 1);
		if (delay > 0) usleep(delay);
	}
	delete thisDTC;
}

void DTCLib::Mu2eUtil::toggle_serdes()
{
	TLOG(TLVL_DEBUG) << "Swapping SERDES Oscillator Clock" << std::endl;
	auto thisDTC = new DTC(DTC_SimMode_NoCFO, dtc, rocMask, expectedDesignVersion);
	auto clock = thisDTC->ReadSERDESOscillatorClock();
	if (clock == DTC_SerdesClockSpeed_3125Gbps)
	{
		TLOG(TLVL_INFO) << "Setting SERDES Oscillator Clock to 2.5 Gbps" << std::endl;
		thisDTC->SetSERDESOscillatorClock(DTC_SerdesClockSpeed_25Gbps);
	}
	else if (clock == DTC_SerdesClockSpeed_25Gbps)
	{
		TLOG(TLVL_INFO) << "Setting SERDES Oscillator Clock to 3.125 Gbps" << std::endl;
		thisDTC->SetSERDESOscillatorClock(DTC_SerdesClockSpeed_3125Gbps);
	}
	else
	{
		TLOG(TLVL_ERROR) << "Error: SERDES clock not recognized value!";
	}
	delete thisDTC;
}

void DTCLib::Mu2eUtil::reset_ddr()
{
	TLOG(TLVL_DEBUG) << "Resetting DDR Address" << std::endl;
	auto thisDTC = new DTC(DTC_SimMode_NoCFO, dtc, rocMask, expectedDesignVersion);

	thisDTC->ResetDDRInterface();
	thisDTC->ResetDDRReadAddress();
	thisDTC->ResetDDRWriteAddress();
	thisDTC->ResetDDR();
	delete thisDTC;
}

void DTCLib::Mu2eUtil::reset_detector_emulator()
{
	TLOG(TLVL_DEBUG) << "Resetting Detector Emulator" << std::endl;
	auto thisDTC = new DTC(DTC_SimMode_NoCFO, dtc, rocMask, expectedDesignVersion);
	thisDTC->ClearDetectorEmulatorInUse();
	thisDTC->ResetDDR();
	thisDTC->SoftReset();
	delete thisDTC;
}

void DTCLib::Mu2eUtil::verify_sim_file()
{
	TLOG(TLVL_DEBUG) << "Operation \"verify_simfile\"" << std::endl;
	auto thisDTC = new DTC(DTC_SimMode_NoCFO, dtc, rocMask, expectedDesignVersion);
	auto device = thisDTC->GetDevice();

	device->ResetDeviceTime();

	if (useSimFile)
	{
		thisDTC->DisableDetectorEmulator();
		thisDTC->EnableDetectorEmulatorMode();
		thisDTC->VerifySimFileInDTC(simFile, rawOutputFile);
	}
}

void DTCLib::Mu2eUtil::verify_stream()
{
	TLOG(TLVL_DEBUG) << "Operation \"verify_stream\"" << std::endl;
	auto startTime = std::chrono::steady_clock::now();
	auto thisDTC = new DTC(DTC_SimMode_NoCFO, dtc, rocMask, expectedDesignVersion);
	auto device = thisDTC->GetDevice();

	auto initTime = device->GetDeviceTime();
	device->ResetDeviceTime();
	auto afterInit = std::chrono::steady_clock::now();

	DTCSoftwareCFO cfo(thisDTC, useCFOEmulator, packetCount, debugType, stickyDebugType, quiet, false, forceNoDebug, useCFODRP);

	if (useSimFile)
	{
		auto overwrite = false;
		if (simFile.size() > 0) overwrite = true;
		thisDTC->WriteSimFileToDTC(simFile, false, overwrite, rawOutputFile, skipVerify);
		if (readGenerated)
		{
			exit(0);
		}
	}
	else if (readGenerated)
	{
		thisDTC->DisableDetectorEmulator();
		thisDTC->EnableDetectorEmulatorMode();
		thisDTC->SetDetectorEmulationDMACount(number);
		thisDTC->EnableDetectorEmulator();
	}

	if (thisDTC->GetSimMode() != DTC_SimMode_Loopback && timestampFile != "")
	{
		syncRequests = false;
		std::set<DTC_EventWindowTag> timestamps;
		std::ifstream is(timestampFile);
		uint64_t a;
		while (is >> a)
		{
			timestamps.insert(DTC_EventWindowTag(a));
		}
		number = timestamps.size();
		cfo.SendRequestsForList(timestamps, cfodelay, heartbeatsAfter);
	}
	else if (thisDTC->GetSimMode() != DTC_SimMode_Loopback && !syncRequests)
	{
		cfo.SendRequestsForRange(number, DTC_EventWindowTag(timestampOffset), incrementTimestamp, cfodelay, requestsAhead, heartbeatsAfter);
	}
	else if (thisDTC->GetSimMode() == DTC_SimMode_Loopback)
	{
		uint64_t ts = timestampOffset;
		DTC_DataHeaderPacket header(DTC_Link_0, static_cast<uint16_t>(0), DTC_DataStatus_Valid, 0, DTC_Subsystem_Other, 0, DTC_EventWindowTag(ts));
		TLOG(TLVL_INFO) << "Request: " << header.toJSON() << std::endl;
		thisDTC->WriteDMAPacket(header);
	}

	auto readoutRequestTime = device->GetDeviceTime();
	device->ResetDeviceTime();
	auto afterRequests = std::chrono::steady_clock::now();

	DTC_Data_Verifier verifier;

	for (unsigned ii = 0; ii < number + extraReads; ++ii)
	{
		if (syncRequests && ii < number)
		{
			auto startRequest = std::chrono::steady_clock::now();
			cfo.SendRequestForTimestamp(DTC_EventWindowTag(timestampOffset + (incrementTimestamp ? ii : 0), heartbeatsAfter));
			auto endRequest = std::chrono::steady_clock::now();
			readoutRequestTime +=
				std::chrono::duration_cast<std::chrono::duration<double, std::ratio<1>>>(endRequest - startRequest).count();
		}
		TLOG((reallyQuiet ? TLVL_DEBUG + 4 : TLVL_INFO)) << "Buffer Read " << std::dec << ii << std::endl;

		bool readSuccess = false;
		bool timeout = false;
		bool verified = false;
		size_t sts = 0;
		unsigned buffers_read = 1;
		mu2e_databuff_t* buffer = readDTCBuffer(device, readSuccess, timeout, sts, false);

		if (!readSuccess && checkSERDES)
			break;
		else if (!readSuccess)
			continue;

		void* readPtr = &buffer[0];
		uint16_t bufSize = static_cast<uint16_t>(*static_cast<uint64_t*>(readPtr));
		readPtr = static_cast<uint8_t*>(readPtr) + 8;

		if (timeout)
		{
			if (stopOnTimeout)
			{
				TLOG(TLVL_ERROR) << "Timeout detected and stop-on-timeout mode enabled. Stopping after " << ii << " events!";
				break;
			}

			TLOG(TLVL_WARNING) << "Timeout detected, moving to next read";
			continue;
		}

		DTC_Event evt(readPtr);
		size_t eventByteCount = evt.GetEventByteCount();
		size_t subEventCount = 0;
		if (eventByteCount > bufSize - 8U)
		{
			DTC_Event newEvt(eventByteCount);
			memcpy(const_cast<void*>(newEvt.GetRawBufferPointer()), evt.GetRawBufferPointer(), bufSize - 8);
			size_t newEvtSize = bufSize - 8;
			while (newEvtSize < eventByteCount)
			{
				TLOG(TLVL_TRACE) << "Reading continued DMA, current size " << newEvtSize << " / " << eventByteCount;
				buffer = readDTCBuffer(device, readSuccess, timeout, sts, true);
				if (!readSuccess)
				{
					TLOG(TLVL_ERROR) << "Unable to receive continued DMA! Aborting!";
					break;
				}
				readPtr = &buffer[0];
				bufSize = static_cast<uint16_t>(*static_cast<uint64_t*>(readPtr));
				readPtr = static_cast<uint8_t*>(readPtr) + 8;
				buffers_read++;

				size_t bytes_to_read = bufSize - 8;
				if (newEvtSize + bufSize - 8 > eventByteCount) { bytes_to_read = eventByteCount - newEvtSize; }

				memcpy(const_cast<uint8_t*>(static_cast<const uint8_t*>(newEvt.GetRawBufferPointer()) + newEvtSize), readPtr, bytes_to_read);
				newEvtSize += bufSize - 8;
			}

			if (!readSuccess && checkSERDES)
				break;
			else if (!readSuccess)
				continue;

			newEvt.SetupEvent();
			subEventCount = newEvt.GetSubEventCount();

			if (thisDTC->GetSimMode() == DTC_SimMode_ROCEmulator ||
				thisDTC->GetSimMode() == DTC_SimMode_Performance)
			{
				auto roc_mask_tmp = rocMask;
				size_t num_rocs = 0;

				while (roc_mask_tmp != 0)
				{
					if ((roc_mask_tmp & 0x1) == 1)
					{
						num_rocs++;
					}
					roc_mask_tmp = roc_mask_tmp >> 4;
				}

				size_t expectedEventSize = sizeof(DTC_EventHeader) + sizeof(DTC_SubEventHeader) + num_rocs * ((packetCount + 1) * 16);
				if (newEvt.GetEventByteCount() != expectedEventSize)
				{
					TLOG(TLVL_WARNING) << "DTC_Event size mismatch! Expected size was " << expectedEventSize << ", actual " << newEvt.GetEventByteCount();
				}
			}

			verified = verifier.VerifyEvent(newEvt);
		}
		else
		{
			evt.SetupEvent();
			subEventCount = evt.GetSubEventCount();
			if (thisDTC->GetSimMode() == DTC_SimMode_ROCEmulator ||
				thisDTC->GetSimMode() == DTC_SimMode_Performance)
			{
				auto roc_mask_tmp = rocMask;
				size_t num_rocs = 0;

				while (roc_mask_tmp != 0)
				{
					if ((roc_mask_tmp & 0x1) == 1)
					{
						num_rocs++;
					}
					roc_mask_tmp = roc_mask_tmp >> 4;
				}

				size_t expectedEventSize = sizeof(DTC_EventHeader) + sizeof(DTC_SubEventHeader) + num_rocs * ((packetCount + 1) * 16);
				if (evt.GetEventByteCount() != expectedEventSize)
				{
					TLOG(TLVL_WARNING) << "DTC_Event size mismatch! Expected size was " << expectedEventSize << ", actual " << evt.GetEventByteCount();
				}
			}
			verified = verifier.VerifyEvent(evt);
		}
		if (!reallyQuiet)
		{
			if (verified)
			{
				TLOG(TLVL_INFO) << "Event verified successfully, " << subEventCount << " sub-events";
			}
			else
			{
				TLOG(TLVL_WARNING) << "Event verification failed!";
			}
		}

		if (stopOnVerifyFailure && !verified)
		{
			TLOG(TLVL_ERROR) << "Event verification error encountered and stop-verify mode enabled. Stopping after " << ii << " events!";
			break;
		}

		device->read_release(DTC_DMA_Engine_DAQ, buffers_read);
		if (delay > 0) usleep(delay);
	}
	device->release_all(DTC_DMA_Engine_DAQ);

	auto readDevTime = device->GetDeviceTime();
	auto doneTime = std::chrono::steady_clock::now();
	auto totalBytesRead = device->GetReadSize();
	auto totalBytesWritten = device->GetWriteSize();
	auto totalTime =
		std::chrono::duration_cast<std::chrono::duration<double, std::ratio<1>>>(doneTime - startTime).count();
	auto totalInitTime =
		std::chrono::duration_cast<std::chrono::duration<double, std::ratio<1>>>(afterInit - startTime).count();
	auto totalRequestTime =
		std::chrono::duration_cast<std::chrono::duration<double, std::ratio<1>>>(afterRequests - afterInit).count();
	auto totalReadTime =
		std::chrono::duration_cast<std::chrono::duration<double, std::ratio<1>>>(doneTime - afterRequests).count();

	TLOG(TLVL_INFO) << "Total Elapsed Time: " << Utilities::FormatTimeString(totalTime) << "." << std::endl
					<< "Total Init Time: " << Utilities::FormatTimeString(totalInitTime) << "." << std::endl
					<< "Total Readout Request Time: " << Utilities::FormatTimeString(totalRequestTime) << "." << std::endl
					<< "Total Read Time: " << Utilities::FormatTimeString(totalReadTime) << "." << std::endl;
	TLOG(TLVL_INFO) << "Device Init Time: " << Utilities::FormatTimeString(initTime) << "." << std::endl
					<< "Device Request Time: " << Utilities::FormatTimeString(readoutRequestTime) << "." << std::endl
					<< "Device Read Time: " << Utilities::FormatTimeString(readDevTime) << "." << std::endl;
	TLOG(TLVL_INFO) << "Total Bytes Written: " << Utilities::FormatByteString(static_cast<double>(totalBytesWritten), "")
					<< "." << std::endl
					<< "Total Bytes Read: " << Utilities::FormatByteString(static_cast<double>(totalBytesRead), "") << "."
					<< std::endl;
	TLOG(TLVL_INFO) << "Total PCIe Rate: "
					<< Utilities::FormatByteString((totalBytesWritten + totalBytesRead) / totalTime, "/s") << std::endl
					<< "Read Rate: " << Utilities::FormatByteString(totalBytesRead / totalReadTime, "/s") << std::endl
					<< "Device Read Rate: " << Utilities::FormatByteString(totalBytesRead / readDevTime, "/s") << std::endl;
}

void DTCLib::Mu2eUtil::buffer_test()
{
	TLOG(TLVL_DEBUG) << "Operation \"buffer_test\"" << std::endl;
	auto startTime = std::chrono::steady_clock::now();  // get the beggining of the test
	auto thisDTC = new DTC(DTC_SimMode_NoCFO, dtc, rocMask, expectedDesignVersion);
	auto device = thisDTC->GetDevice();

	auto initTime = device->GetDeviceTime();
	device->ResetDeviceTime();
	auto afterInit = std::chrono::steady_clock::now();

	// Emulated CFO
	DTCSoftwareCFO cfo(thisDTC,
					   useCFOEmulator,
					   packetCount,
					   debugType,
					   stickyDebugType,
					   quiet,
					   false,
					   forceNoDebug,
					   useCFODRP);

	if (useSimFile)
	{
		auto overwrite = false;
		if (simFile.size() > 0) overwrite = true;
		thisDTC->WriteSimFileToDTC(simFile, false, overwrite, rawOutputFile, skipVerify);
		if (readGenerated)
		{
			exit(0);
		}
	}
	else if (readGenerated)
	{
		thisDTC->DisableDetectorEmulator();
		thisDTC->EnableDetectorEmulatorMode();
		thisDTC->SetDetectorEmulationDMACount(number);
		thisDTC->EnableDetectorEmulator();
	}

	if (thisDTC->GetSimMode() != DTC_SimMode_Loopback && timestampFile != "")
	{
		syncRequests = false;
		std::set<DTC_EventWindowTag> timestamps;
		std::ifstream is(timestampFile);
		uint64_t a;
		while (is >> a)
		{
			timestamps.insert(DTC_EventWindowTag(a));
		}
		number = timestamps.size();
		cfo.SendRequestsForList(timestamps, cfodelay, heartbeatsAfter);
	}
	else if (thisDTC->GetSimMode() != DTC_SimMode_Loopback && !syncRequests)
	{
		cfo.SendRequestsForRange(number,
								 DTC_EventWindowTag(timestampOffset),
								 incrementTimestamp,
								 cfodelay,
								 requestsAhead,
								 heartbeatsAfter);
	}
	else if (thisDTC->GetSimMode() == DTC_SimMode_Loopback)
	{
		uint64_t ts = timestampOffset;
		DTC_DataHeaderPacket header(DTC_Link_0, static_cast<uint16_t>(0), DTC_DataStatus_Valid, 0, DTC_Subsystem_Other, 0, DTC_EventWindowTag(ts));
		TLOG(TLVL_INFO) << "Request: " << header.toJSON() << std::endl;
		thisDTC->WriteDMAPacket(header);
	}

	// record the response time
	auto readoutRequestTime = device->GetDeviceTime();
	device->ResetDeviceTime();
	auto afterRequests = std::chrono::steady_clock::now();

	for (unsigned ii = 0; ii < number + extraReads; ++ii)
	{
		if (syncRequests && ii < number)
		{
			auto startRequest = std::chrono::steady_clock::now();
			cfo.SendRequestForTimestamp(DTC_EventWindowTag(timestampOffset + (incrementTimestamp ? ii : 0), heartbeatsAfter));
			auto endRequest = std::chrono::steady_clock::now();
			readoutRequestTime +=
				std::chrono::duration_cast<std::chrono::duration<double, std::ratio<1>>>(endRequest - startRequest).count();
		}
		TLOG((reallyQuiet ? TLVL_DEBUG + 4 : TLVL_INFO)) << "Buffer Read " << std::dec << ii << std::endl;

		bool readSuccess = false;
		bool timeout = false;
		size_t sts = 0;
		mu2e_databuff_t* buffer = readDTCBuffer(device, readSuccess, timeout, sts, false);

		// check if the read of the buffer is succesful
		if (!readSuccess && checkSERDES)
			break;
		else if (!readSuccess)
			continue;

		if (stopOnTimeout && timeout)
		{
			TLOG(TLVL_ERROR) << "Timeout detected and stop-on-timeout mode enabled. Stopping after " << ii << " events!";
			break;
		}

		if (!reallyQuiet)
		{
			DTCLib::Utilities::PrintBuffer(buffer, sts, quietCount);
		}

		device->read_release(DTC_DMA_Engine_DAQ, 1);
		if (delay > 0) usleep(delay);
	}
	device->release_all(DTC_DMA_Engine_DAQ);

	auto readDevTime = device->GetDeviceTime();
	auto doneTime = std::chrono::steady_clock::now();
	auto totalBytesRead = device->GetReadSize();
	auto totalBytesWritten = device->GetWriteSize();
	auto totalTime =
		std::chrono::duration_cast<std::chrono::duration<double, std::ratio<1>>>(doneTime - startTime).count();
	auto totalInitTime =
		std::chrono::duration_cast<std::chrono::duration<double, std::ratio<1>>>(afterInit - startTime).count();
	auto totalRequestTime =
		std::chrono::duration_cast<std::chrono::duration<double, std::ratio<1>>>(afterRequests - afterInit).count();
	auto totalReadTime =
		std::chrono::duration_cast<std::chrono::duration<double, std::ratio<1>>>(doneTime - afterRequests).count();

	TLOG(TLVL_INFO) << "Total Elapsed Time: " << Utilities::FormatTimeString(totalTime) << "." << std::endl
					<< "Total Init Time: " << Utilities::FormatTimeString(totalInitTime) << "." << std::endl
					<< "Total Readout Request Time: " << Utilities::FormatTimeString(totalRequestTime) << "." << std::endl
					<< "Total Read Time: " << Utilities::FormatTimeString(totalReadTime) << "." << std::endl;
	TLOG(TLVL_INFO) << "Device Init Time: " << Utilities::FormatTimeString(initTime) << "." << std::endl
					<< "Device Request Time: " << Utilities::FormatTimeString(readoutRequestTime) << "." << std::endl
					<< "Device Read Time: " << Utilities::FormatTimeString(readDevTime) << "." << std::endl;
	TLOG(TLVL_INFO) << "Total Bytes Written: " << Utilities::FormatByteString(static_cast<double>(totalBytesWritten), "")
					<< "." << std::endl
					<< "Total Bytes Read: " << Utilities::FormatByteString(static_cast<double>(totalBytesRead), "") << "."
					<< std::endl;
	TLOG(TLVL_INFO) << "Total PCIe Rate: "
					<< Utilities::FormatByteString((totalBytesWritten + totalBytesRead) / totalTime, "/s") << std::endl
					<< "Read Rate: " << Utilities::FormatByteString(totalBytesRead / totalReadTime, "/s") << std::endl
					<< "Device Read Rate: " << Utilities::FormatByteString(totalBytesRead / readDevTime, "/s") << std::endl;
}

void DTCLib::Mu2eUtil::read_release()
{
	TLOG(TLVL_DEBUG) << "Operation \"read_release\"";
	mu2edev device;
	device.init(DTCLib::DTC_SimMode_Disabled, dtc);
	for (unsigned ii = 0; ii < number; ++ii)
	{
		void* buffer;
		auto tmo_ms = 0;
		auto stsRD = device.read_data(DTC_DMA_Engine_DAQ, &buffer, tmo_ms);
		auto stsRL = device.read_release(DTC_DMA_Engine_DAQ, 1);
		TLOG(TLVL_TRACE + 10) << "util - release/read for DAQ and DCS ii=" << ii << ", stsRD=" << stsRD << ", stsRL=" << stsRL << ", buffer=" << buffer;
		if (delay > 0) usleep(delay);
	}
}

void DTCLib::Mu2eUtil::program_clock()
{
	TLOG(TLVL_DEBUG) << "Operation \"program_clock\"";
	auto thisDTC = new DTC(DTC_SimMode_NoCFO, dtc, rocMask, expectedDesignVersion, true);
	auto oscillator = clockToProgram == 0 ? DTC_OscillatorType_SERDES
										  : (clockToProgram == 1 ? DTC_OscillatorType_DDR : DTC_OscillatorType_Timing);
	thisDTC->SetNewOscillatorFrequency(oscillator, targetFrequency);
	delete thisDTC;
}

void DTCLib::Mu2eUtil::dma_info()
{
	TLOG(TLVL_DEBUG) << "Opearation \"dma_info\"";
	if (dtc == -1)
	{
		auto dtcE = getenv("DTCLIB_DTC");
		if (dtcE != nullptr)
		{
			dtc = atoi(dtcE);
		}
		else
			dtc = 0;
	}

	mu2edev device;
	device.init(DTCLib::DTC_SimMode_Disabled, dtc);
	device.meta_dump();
}

void DTCLib::Mu2eUtil::run()
{
	if (rawOutput) outputStream.open(rawOutputFile, std::ios::out | std::ios::app | std::ios::binary);

	if (op == "read_data")
	{
		read_data();
	}
	else if (op == "toggle_serdes")
	{
		toggle_serdes();
	}
	else if (op == "reset_ddr")
	{
		reset_ddr();
	}
	else if (op == "reset_detemu")
	{
		reset_detector_emulator();
	}
	else if (op == "verify_simfile")
	{
		verify_sim_file();
	}
	else if (op == "verify_stream")
	{
		verify_stream();
	}
	else if (op == "buffer_test")
	{
		buffer_test();
	}
	else if (op == "read_release")
	{
		read_release();
	}
	else if (op == "program_clock")
	{
		program_clock();
	}
	else if (op == "dma_info")
	{
		dma_info();
	}
	else
	{
		TLOG(TLVL_ERROR) << "Unrecognized operation: " << op << std::endl;
		printHelpMsg();
	}

	if (rawOutput)
	{
		outputStream.flush();
		outputStream.close();
	}
}

void DTCLib::Mu2eUtil::printHelpMsg()
{
	std::cout << "Usage: mu2eUtil [options] "
				 "[read_data,reset_ddrread,reset_detemu,toggle_serdes,loopback,verify_stream,buffer_test,read_release,program_clock,verify_simfile,dma_info]"
			  << std::endl;
	std::cout
		<< "Options are:" << std::endl
		<< "    -h, --help: This message." << std::endl
		<< "    -n: Number of times to repeat test. (Default: 1)" << std::endl
		<< "    -o: Starting Timestamp offest. (Default: 1)." << std::endl
		<< "    -i: Do not increment Timestamps." << std::endl
		<< "    -S: Synchronous Timestamp mode (1 RR & DR per Read operation)" << std::endl
		<< "    -d: Delay between tests, in us (Default: 0)." << std::endl
		<< "    -D: CFO Request delay interval (Default: 1000 (minimum)." << std::endl
		<< "    -c: Number of Debug Packets to request (Default: 0)." << std::endl
		<< "    -N: Do NOT set the Debug flag in generated Data Request packets" << std::endl
		<< "    -b: Number of Data Blocks to generate per Event (Default: 1)." << std::endl
		<< "    -E: Number of Events to generate per DMA block (Default: 1)." << std::endl
		<< "    -a: Number of Readout Request/Data Requests to send before starting to read data (Default: 0)."
		<< std::endl
		<< "    -q: Quiet mode (Don't print requests) Additionally, for buffer_test mode, limits to N (Default 1) "
		   "packets at the beginning and end of the buffer."
		<< std::endl
		<< "    -Q: Really Quiet mode (Try not to print anything)" << std::endl
		<< "    -s: Stop on SERDES Error." << std::endl
		<< "    -e: Use DTCLib's SoftwareCFO instead of the DTC CFO Emulator" << std::endl
		<< "    -t: Use DebugType flag (1st request gets ExternalDataWithFIFOReset, the rest get ExternalData)"
		<< std::endl
		<< "    -T: Set DebugType flag for ALL requests (0, 1, or 2)" << std::endl
		<< "    -f: RAW Output file path" << std::endl
		<< "    -H: Write DMA headers to raw output file (when -f is used with -g)" << std::endl
		<< "    -p: Send DTCLIB_SIM_FILE to DTC and enable Detector Emulator mode" << std::endl
		<< "    -P: Send <file> to DTC and enable Detector Emulator mode (Default: \"\")" << std::endl
		<< "    -G: Enable Detector Emulator Mode" << std::endl
		<< "    -r: # of rocs to enable. Hexadecimal, each digit corresponds to a link. ROC_0: 1, ROC_1: 3, ROC_2: 5, "
		   "ROC_3: 7, ROC_4: 9, ROC_5: B (Default 0x1, All possible: 0xBBBBBB)"
		<< std::endl
		<< "    -F: Frequency to program (in Hz, sorry...Default 166666667 Hz)" << std::endl
		<< "    -C: Clock to program (0: SERDES, 1: DDR, 2: Timing, Default 0)" << std::endl
		<< "    -v: Expected DTC Design version string (Default: \"\")" << std::endl
		<< "    -V: Do NOT attempt to verify that the sim file landed in DTC memory correctly" << std::endl
		<< "    --timestamp-list: Read <file> for timestamps to request (CFO will generate heartbeats for all timestamps in range spanned by file)" << std::endl
		<< "    --dtc: Use dtc <num> (Defaults to DTCLIB_DTC if set, 0 otherwise, see ls /dev/mu2e* for available DTCs)" << std::endl
		<< "    --cfoDRP: Send DRPs from the DTC CFO Emulator instead of from the DTC itself" << std::endl
		<< "    --heartbeats: Send <int> heartbeats after each DataRequestPacket" << std::endl
		<< "    --binary-file-mode: Write DMA sizes to <file> along with read data, to generate a new binary file for detector emulator mode (not compatible with -f)" << std::endl
		<< "    --stop-verify: If a verify_stream mode error occurs, stop processing" << std::endl
		<< "    --stop-on-timeout: Stop verify_stream or buffer_test mode if a timeout is detected (0xCAFE in first packet of buffer)" << std::endl
		<< "    --extra-reads: Number of extra DMA reads to attempt in verify_stream and buffer_test modes (Default: 1)" << std::endl;

	exit(0);
}

mu2e_databuff_t* DTCLib::Mu2eUtil::readDTCBuffer(mu2edev* device, bool& readSuccess, bool& timeout, size_t& sts, bool continuedMode)
{
	mu2e_databuff_t* buffer;
	auto tmo_ms = 1500;
	readSuccess = false;
	TLOG(TLVL_TRACE) << "util - before read for DAQ";
	sts = device->read_data(DTC_DMA_Engine_DAQ, reinterpret_cast<void**>(&buffer), tmo_ms);
	TLOG(TLVL_TRACE) << "util - after read for DAQ sts=" << sts << ", buffer=" << (void*)buffer;

	if (sts > 0)
	{
		readSuccess = true;
		void* readPtr = &buffer[0];
		uint16_t bufSize = static_cast<uint16_t>(*static_cast<uint64_t*>(readPtr));
		readPtr = static_cast<uint8_t*>(readPtr) + 8;
		TLOG((reallyQuiet ? TLVL_DEBUG + 4 : TLVL_INFO)) << "Buffer reports DMA size of " << std::dec << bufSize << " bytes. Device driver reports read of "
														 << sts << " bytes," << std::endl;

		TLOG(TLVL_TRACE) << "util - bufSize is " << bufSize;
		if (binaryFileOutput)
		{
			uint64_t dmaWriteSize = sts + 8;
			outputStream.write(reinterpret_cast<char*>(&dmaWriteSize), sizeof(dmaWriteSize));
			outputStream.write(reinterpret_cast<char*>(&buffer[0]), sts);
		}
		else if (rawOutput)
		{
			outputStream.write(static_cast<char*>(readPtr), sts - 8);
		}

		timeout = false;
		if (!continuedMode && sts > sizeof(DTC_EventHeader) + sizeof(DTC_SubEventHeader) + 8)
		{
			// Check for dead or cafe in first packet
			readPtr = static_cast<uint8_t*>(readPtr) + sizeof(DTC_EventHeader) + sizeof(DTC_SubEventHeader);
			std::vector<size_t> wordsToCheck{1, 2, 3, 7, 8};
			for (auto& word : wordsToCheck)
			{
				auto wordPtr = static_cast<uint16_t*>(readPtr) + (word - 1);
				TLOG(TLVL_TRACE + 1) << word << (word == 1 ? "st" : word == 2 ? "nd"
																: word == 3   ? "rd"
																			  : "th")
									 << " word of buffer: " << *wordPtr;
				if (*wordPtr == 0xcafe || *wordPtr == 0xdead)
				{
					TLOG(TLVL_WARNING) << "Buffer Timeout detected! " << word << (word == 1 ? "st" : word == 2 ? "nd"
																								 : word == 3   ? "rd"
																											   : "th")
									   << " word of buffer is 0x" << std::hex << *wordPtr;
					DTCLib::Utilities::PrintBuffer(readPtr, 16, 0, TLVL_TRACE + 3);
					timeout = true;
					break;
				}
			}
		}
	}
	return buffer;
}
