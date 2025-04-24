#include "DTCLibTest.h"
#include "DTCSoftwareCFO.h"

#include <iostream>

#include "TRACE/tracemf.h"

DTCLib::DTCLibTest::DTCLibTest()
	: running_(false), classPassed_(0), classFailed_(0), regPassed_(0), regFailed_(0), daqPassed_(0), daqFailed_(0), dcsPassed_(0), dcsFailed_(0), classPassedTemp_(0), classFailedTemp_(0), regPassedTemp_(0), regFailedTemp_(0), daqPassedTemp_(0), daqFailedTemp_(0), dcsPassedTemp_(0), dcsFailedTemp_(0), nTests_(0), runClassTest_(false), runRegTest_(false), runDAQTest_(false), runDCSTest_(false), printMessages_(false)
{
	thisDTC_ = new DTC();
}

DTCLib::DTCLibTest::~DTCLibTest()
{
	nTests_ = 0;
	if (workerThread_.joinable())
	{
		workerThread_.join();
	}
	delete thisDTC_;
}

// Test Control
void DTCLib::DTCLibTest::startTest(bool classEnabled, bool regIOEnabled, bool daqEnabled, bool dcsEnabled, int nTests,
								   bool printMessages)
{
	runClassTest_ = classEnabled;
	runRegTest_ = regIOEnabled;
	runDCSTest_ = dcsEnabled;
	runDAQTest_ = daqEnabled;
	nTests_ = nTests;
	printMessages_ = printMessages;

	if (printMessages_)
	{
		std::cout << "Starting workerThread" << std::endl;
	}

	workerThread_ = std::thread(&DTCLibTest::doTests, this);
	if (nTests_ >= 0)
	{
		workerThread_.join();
	}
}

void DTCLib::DTCLibTest::stopTests()
{
	nTests_ = 0;
	if (workerThread_.joinable())
	{
		workerThread_.join();
	}
}

// Accessors
int DTCLib::DTCLibTest::classPassed()
{
	auto result = classPassed_ - classPassedTemp_;
	classPassedTemp_ = classPassed_;
	return result;
}

int DTCLib::DTCLibTest::classFailed()
{
	auto result = classFailed_ - classFailedTemp_;
	classFailedTemp_ = classFailed_;
	return result;
}

int DTCLib::DTCLibTest::regPassed()
{
	auto result = regPassed_ - regPassedTemp_;
	regPassedTemp_ = regPassed_;
	return result;
}

int DTCLib::DTCLibTest::regFailed()
{
	auto result = regFailed_ - regFailedTemp_;
	regFailedTemp_ = regFailed_;
	return result;
}

int DTCLib::DTCLibTest::daqPassed()
{
	auto result = daqPassed_ - daqPassedTemp_;
	daqPassedTemp_ = daqPassed_;
	return result;
}

int DTCLib::DTCLibTest::daqFailed()
{
	auto result = daqFailed_ - daqFailedTemp_;
	daqFailedTemp_ = daqFailed_;
	return result;
}

int DTCLib::DTCLibTest::dcsPassed()
{
	auto result = dcsPassed_ - dcsPassedTemp_;
	dcsPassedTemp_ = dcsPassed_;
	return result;
}

int DTCLib::DTCLibTest::dcsFailed()
{
	auto result = dcsFailed_ - dcsFailedTemp_;
	dcsFailedTemp_ = dcsFailed_;
	return result;
}

// Private Functions
void DTCLib::DTCLibTest::doTests()
{
	if (printMessages_)
	{
		std::cout << "Worker thread started" << std::endl;
	}
	std::cout << "DEBUG 1" << std::endl;
	running_ = true;
	// Make sure that the link is enabled before the tests.
	thisDTC_->EnableLink(DTC_Link_0, DTC_LinkEnableMode(true, true));

	auto testCount = 0;
	while (testCount < nTests_ || nTests_ < 0)
	{
		++testCount;
		if (runClassTest_)
		{
			doClassTest();
		}
		if (runRegTest_)
		{
			doRegTest();
		}
		if (runDCSTest_)
		{
			doDCSTest();
		}
		if (runDAQTest_)
		{
			doDAQTest();
		}
	}
	running_ = false;

	auto totalPassed = 0;
	auto totalTests = 0;

	if (runClassTest_)
	{
		totalPassed += classPassed_;
		totalTests += classPassed_ + classFailed_;
		std::cout << std::dec << classPassed_ << " of " << classPassed_ + classFailed_
				  << " Class Construction/Destruction Tests passed." << std::endl;
	}
	if (runRegTest_)
	{
		totalPassed += regPassed_;
		totalTests += regPassed_ + regFailed_;
		std::cout << std::dec << regPassed_ << " of " << regPassed_ + regFailed_ << " register I/O tests passed."
				  << std::endl;
	}
	if (runDCSTest_)
	{
		totalPassed += dcsPassed_;
		totalTests += dcsPassed_ + dcsFailed_;
		std::cout << std::dec << dcsPassed_ << " of " << dcsPassed_ + dcsFailed_ << " DCS DMA I/O tests passed."
				  << std::endl;
	}
	if (runDAQTest_)
	{
		totalPassed += daqPassed_;
		totalTests += daqPassed_ + daqFailed_;
		std::cout << std::dec << daqPassed_ << " of " << daqPassed_ + daqFailed_ << " DAQ DMA I/O tests passed."
				  << std::endl;
	}
	std::cout << std::dec << totalPassed << " of " << totalTests << " tests passed." << std::endl;
}

void DTCLib::DTCLibTest::doClassTest()
{
	if (printMessages_)
	{
		std::cout << std::endl
				  << "Test 0: Class Construction/Destruction" << std::endl;
	}
	auto err = false;
	try
	{
		if (printMessages_) std::cout << "Testing DTC_EventWindowTag Class..." << std::endl;

		auto defaultTS = DTC_EventWindowTag();
		if (printMessages_)
			std::cout << "Default Constructor, TS should be 0: " << defaultTS.GetEventWindowTag(true) << std::endl;
		err = err || defaultTS.GetEventWindowTag(true) != 0;

		auto tsSixtyFour = DTC_EventWindowTag(static_cast<uint64_t>(0xFFFFBEEFDEADBEEF));
		if (printMessages_)
			std::cout << "uint64_t Constructor, TS should be 0xBEEFDEADBEEF: " << std::hex << std::showbase
					  << tsSixtyFour.GetEventWindowTag(true) << std::endl;
		err = err || tsSixtyFour.GetEventWindowTag(true) != 0xBEEFDEADBEEF;

		auto tsSixteenThirtyTwo = DTC_EventWindowTag(static_cast<uint32_t>(0xDEADBEEF), static_cast<uint16_t>(0xDEAD));
		if (printMessages_)
			std::cout << "uint32_t/uint16_t Constructor, TS should be 0xDEADDEADBEEF: "
					  << tsSixteenThirtyTwo.GetEventWindowTag(true) << std::endl;
		err = err || tsSixteenThirtyTwo.GetEventWindowTag(true) != 0xDEADDEADBEEF;

		uint8_t arr[6] = {0xAD, 0xDE, 0xAD, 0xDE, 0xEF, 0xBE};
		auto tsArray = DTC_EventWindowTag(arr);
		if (printMessages_)
			std::cout << "Array Constructor, TS should be 0xBEEFDEADDEAD: " << tsArray.GetEventWindowTag(true) << std::endl;
		err = err || tsArray.GetEventWindowTag(true) != 0xBEEFDEADDEAD;

		auto tsCopy = new DTC_EventWindowTag(defaultTS);
		if (printMessages_) std::cout << "Copy Constructor, TS should be 0: " << tsCopy->GetEventWindowTag(true) << std::endl;
		err = err || tsCopy->GetEventWindowTag(true) != 0;

		tsCopy->SetEventWindowTag(0xBEEFDEAD, 0xBEEF);
		if (printMessages_)
			std::cout << "SetEventWindowTag 32/16 Method, TS should be 0xBEEFBEEFDEAD: " << tsCopy->GetEventWindowTag(true)
					  << std::endl;
		err = err || tsCopy->GetEventWindowTag().to_ullong() != 0xBEEFBEEFDEAD;

		tsCopy->SetEventWindowTag(0xDEADDEADBEEFBEEF);
		if (printMessages_)
			std::cout << "SetEventWindowTag 64 Method, TS should be 0xDEADBEEFBEEF: " << tsCopy->GetEventWindowTag(true) << std::endl;
		err = err || tsCopy->GetEventWindowTag().to_ullong() != 0xDEADBEEFBEEF;

		if (printMessages_) std::cout << "Running DTC_EventWindowTag destructor" << std::endl;
		delete tsCopy;

		if (err)
		{
			if (printMessages_) std::cout << "DTC_EventWindowTag Class failed checks!" << std::endl;
			goto end;
		}

		if (printMessages_) std::cout << "Testing DTC_DataPacket Class..." << std::endl;
		auto defaultDP = new DTC_DataPacket();
		if (printMessages_)
			std::cout << "Default Constructor, Size should be 64: " << std::dec << static_cast<int>(defaultDP->GetSize())
					  << ", and IsMemoryPacket should be false: " << (defaultDP->IsMemoryPacket() ? "true" : "false")
					  << std::endl;
		err = defaultDP->GetSize() != 64;
		err = err || defaultDP->IsMemoryPacket();

		defaultDP->Resize(128);
		if (printMessages_)
			std::cout << "Resize(128), Size should be 128: " << static_cast<int>(defaultDP->GetSize()) << std::endl;
		err = err || defaultDP->GetSize() != 128;

		if (printMessages_) std::cout << "Running Data Integrity Check" << std::endl;
		err = err || !DataPacketIntegrityCheck(defaultDP);

		auto memCopyDP = DTC_DataPacket(*defaultDP);
		if (printMessages_)
			std::cout << "Copy Constructor, MemoryPacket, data[64] should be 64: " << static_cast<int>(memCopyDP.GetByte(64))
					  << std::endl;
		err = err || memCopyDP.GetByte(64) != 64;

		if (printMessages_) std::cout << "Running DTC_DataPacket Destructor" << std::endl;
		delete defaultDP;

		auto buf = reinterpret_cast<mu2e_databuff_t*>(new mu2e_databuff_t());
		auto dataBufPtrfDP = new DTC_DataPacket(buf);
		if (printMessages_)
			std::cout << "Databuff Pointer Constructor, Size should be 16: " << static_cast<int>(dataBufPtrfDP->GetSize())
					  << ", and IsMemoryPacket should be true: " << (dataBufPtrfDP->IsMemoryPacket() ? "true" : "false")
					  << std::endl;
		err = dataBufPtrfDP->GetSize() != 16;
		err = err || !dataBufPtrfDP->IsMemoryPacket();

		auto nonmemCopyDP = DTC_DataPacket(*dataBufPtrfDP);
		(*buf)[0] = 0x8F;
		if (printMessages_)
			std::cout
				<< "DataPacket Memory Packet Copy Constructor: Modifications to original buffer should modify both packets: "
				<< std::hex << "COPY[0]: " << static_cast<int>(nonmemCopyDP.GetByte(0))
				<< ", ORIGINAL[0]: " << static_cast<int>(dataBufPtrfDP->GetByte(0)) << std::endl;
		err = err || nonmemCopyDP.GetByte(0) != 0x8F || dataBufPtrfDP->GetByte(0) != 0x8F;

		delete dataBufPtrfDP;
		if (printMessages_)
			std::cout << "Deleting ORIGINAL should not affect COPY: " << static_cast<int>(nonmemCopyDP.GetByte(0))
					  << std::endl;
		err = err || nonmemCopyDP.GetByte(0) != 0x8F;

		uint8_t buff2[16] = {0xf, 0xe, 0xd, 0xc, 0xb, 0xa, 0x9, 0x8, 0x7, 0x6, 0x5, 0x4, 0x3, 0x2, 0x1, 0x0};
		auto uintBufDP = new DTC_DataPacket(buff2);
		if (printMessages_)
			std::cout << "Integer Array Constructor, IsMemoryPacket should be true: "
					  << (uintBufDP->IsMemoryPacket() ? "true" : "false")
					  << ", and data[0] should be 0xf: " << static_cast<int>(uintBufDP->GetByte(0)) << std::endl;
		err = err || !uintBufDP->IsMemoryPacket() || uintBufDP->GetByte(0) != 0xf;

		uintBufDP->Resize(128);
		if (printMessages_)
			std::cout << "Resizing a memory packet should not work, size should be 16: " << std::dec
					  << static_cast<int>(uintBufDP->GetSize()) << std::endl;
		err = err || uintBufDP->GetSize() != 16;

		if (printMessages_) std::cout << "Running DTC_DataPacket Destructor" << std::endl;
		delete uintBufDP;

		if (err)
		{
			if (printMessages_) std::cout << "DTC_DataPacket Class failed checks!" << std::endl;
		}
	}
	catch (std::exception const& ex)
	{
		if (printMessages_)
		{
			std::cout << "Test failed with exception: " << ex.what() << std::endl;
		}
		++classFailed_;
	}

end:
	if (err)
	{
		if (printMessages_) std::cout << "One or more classes failed!" << std::endl;
		++classFailed_;
	}
	else
	{
		if (printMessages_) std::cout << "All Classes Passed" << std::endl;
		++classPassed_;
	}

	if (printMessages_)
	{
		std::cout << std::endl
				  << std::endl;
	}
}

void DTCLib::DTCLibTest::doRegTest()
{
	if (printMessages_)
	{
		std::cout << "Test 1: Register R/W" << std::endl;
	}
	try
	{
		auto designVersion = thisDTC_->ReadDesignVersion();
		if (printMessages_)
		{
			std::cout << "Reading Design Version: " << designVersion << std::endl;
			std::cout << "If simulated, result will be v99.99_2053-49-53-44 (SIMD in ASCII)" << std::endl;
			std::cout << "Attempting to Disable Link 0." << std::endl;
		}
		auto link0Value = thisDTC_->ReadLinkEnabled(DTC_Link_0);
		if (printMessages_)
		{
			std::cout << "Value before: " << link0Value << std::endl;
		}
		thisDTC_->DisableLink(DTC_Link_0);
		auto link0New = thisDTC_->ReadLinkEnabled(DTC_Link_0);

		if (printMessages_)
		{
			std::cout << "Value after: " << link0New << std::endl;
		}
		// Make sure that the link is enabled after the test.
		thisDTC_->EnableLink(DTC_Link_0, DTC_LinkEnableMode(true, true));
		if (link0New != link0Value)
		{
			if (printMessages_)
			{
				std::cout << "Test Succeeded" << std::endl;
			}
			++regPassed_;
		}
		else
		{
			if (printMessages_)
			{
				std::cout << "Test Failed" << std::endl;
			}
			++regFailed_;
		}
	}
	catch (std::exception const& ex)
	{
		if (printMessages_)
		{
			std::cout << "Test failed with exception: " << ex.what() << std::endl;
		}
		++regFailed_;
	}
	if (printMessages_)
	{
		std::cout << std::endl
				  << std::endl;
	}
}

void DTCLib::DTCLibTest::doDCSTest()
{
	if (printMessages_)
	{
		std::cout << "Test 4: DMA R/W on DCS Channel" << std::endl;
	}
	try
	{
		if (printMessages_)
		{
			std::cout << "Running DCS Request/Reply Cycle with 0-11 sequence" << std::endl;
		}
		uint8_t testData[12]{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
		if (printMessages_)
		{
			std::cout << "Data in: ";
			for (auto i = 0; i < 12; i++)
			{
				std::cout << std::dec << static_cast<int>(testData[i]) << " ";
			}
			std::cout << std::endl;
		}
		std::cout << "TEST DISABLED FOR NOW!!!" << std::endl;
		// thisDTC_->DCSRequestReply(DTC_Link_0, DTC_ROC_0, testData);
		if (printMessages_)
		{
			std::cout << "Data out: ";
			for (auto i = 0; i < 12; i++)
			{
				std::cout << std::dec << static_cast<int>(testData[i]) << " ";
			}
			std::cout << std::endl;
			std::cout << "Simulated DTC should match before/after." << std::endl;
		}
		++dcsPassed_;
	}
	catch (std::exception const& ex)
	{
		if (printMessages_)
		{
			std::cout << "Test failed with exception: " << ex.what() << std::endl;
		}
		++dcsFailed_;
	}
	if (printMessages_)
	{
		std::cout << std::endl
				  << std::endl;
	}
}

void DTCLib::DTCLibTest::doDAQTest()
{
	if (printMessages_)
	{
		std::cout << "Test 5: DMA R/W on DAQ Channel" << std::endl;
	}
	try
	{
		thisDTC_->EnableLink(DTC_Link_0, DTC_LinkEnableMode(true, true));

		DTCSoftwareCFO theCFO(thisDTC_, true, 0, DTC_DebugType_SpecialSequence, true, !printMessages_);
		theCFO.SendRequestForTimestamp();
		auto data = thisDTC_->GetData();
		if (data.size() > 0)
		{
			if (printMessages_) std::cout << data.size() << " packets returned\n";
		}
		else
		{
			TRACE_CNTL("modeM", 0L);
			if (printMessages_) std::cout << "no data returned\n";
			++daqFailed_;
			return;
		}

		auto disparity = thisDTC_->ReadSERDESRXDisparityError(DTC_Link_0);
		auto cnit = thisDTC_->ReadSERDESRXCharacterNotInTableError(DTC_Link_0);
		if (cnit.GetData()[0] || cnit.GetData()[1])
		{
			TRACE_CNTL("modeM", 0L);
			std::cout << "Character Not In Table Error detected" << std::endl;
			++daqFailed_;
			return;
		}
		if (disparity.GetData()[0] || disparity.GetData()[1])
		{
			TRACE_CNTL("modeM", 0L);
			std::cout << "Disparity Error Detected" << std::endl;
			++daqFailed_;
			return;
		}

		if (printMessages_)
		{
			std::cout << "Test Passed" << std::endl;
		}
		++daqPassed_;
	}
	catch (std::exception const& ex)
	{
		if (printMessages_)
		{
			std::cout << "Test failed with exception: " << ex.what() << std::endl;
		}
		++daqFailed_;
	}
	if (printMessages_)
	{
		std::cout << std::endl
				  << std::endl;
	}
}

bool DTCLib::DTCLibTest::DataPacketIntegrityCheck(DTC_DataPacket* packet)
{
	auto retCode = true;
	for (auto ii = 0; ii < packet->GetSize(); ++ii)
	{
		packet->SetByte(ii, ii);
	}

	for (auto ii = 0; ii < packet->GetSize(); ++ii)
	{
		retCode = packet->GetByte(ii) == ii;
		if (!retCode) break;
	}

	return retCode;
}
