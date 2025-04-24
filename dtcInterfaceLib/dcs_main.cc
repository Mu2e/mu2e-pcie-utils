// This file (util_main.cc) was created by Ron Rechenmacher <ron@fnal.gov> on
// May 13, 2015. "TERMS AND CONDITIONS" governing this file are in the README
// or COPYING file. If you do not have such a file, one can be obtained by
// contacting Ron or Fermi Lab in Batavia IL, 60510, phone: 630-840-3000.
// $RCSfile: .emacs.gnu,v $
// rev="$Revision: 1.23 $$Date: 2012/01/23 15:32:40 $";

#include <unistd.h>  // usleep
#include <chrono>
#include <cmath>
#include <cstdio>   // printf
#include <cstdlib>  // strtoul
#include <iomanip>
#include <iostream>
#include <string>

#include "TRACE/tracemf.h"
#define TRACE_NAME "rocUtil"

#include "DTC.h"

#define DCS_TLVL(b) b ? TLVL_DEBUG + 6 : TLVL_INFO

using namespace DTCLib;

void printHelpMsg()
{
	std::cout << "Usage: rocUtil [options] "
				 "[read_register,simple_read,reset_roc,write_register,read_extregister,write_extregister,test_read,read_release,"
				 "toggle_serdes,block_read,block_write,raw_block_read]"
			  << std::endl;
	std::cout << "Options are:" << std::endl
			  << " -h: This message." << std::endl
			  << " -l: Link to send requests on (Default: 0)" << std::endl
			  << " -n: Number of times to repeat test. (Default: 1)" << std::endl
			  << " -d: Delay between tests, in us (Default: 0)." << std::endl
			  << " -w: Data to write to address" << std::endl
			  << " -a: Address to write" << std::endl
			  << " -b: Block address (for write_rocext)" << std::endl
			  << " -q: Quiet mode (Don't print requests)" << std::endl
			  << " -Q: Really Quiet mode (Try not to print anything)" << std::endl
			  << " -v: Expected DTC Design version string (Default: \"\")" << std::endl
			  << " -c: Word count for Block Reads (Default: 0)" << std::endl
			  << " -i: Do not set the incrementAddress bit for block operations" << std::endl
			  << " --dtc: Use dtc <num> (Defaults to DTCLIB_DTC if set, 0 otherwise, see ls /dev/mu2e* for available DTCs)" << std::endl
			  << " --stop-on-error: Abort operation if an error occurs" << std::endl
			  << " --timeout-ms Try this long to read a DCS DMA from the DTC (Default: 10 ms)"
			  << " --link-mask ROC links to enable on DTC (Default: 0x111111)";
	exit(0);
}

int main(int argc, char* argv[])
{
	auto quiet = false;
	auto reallyQuiet = false;
	unsigned link = 0;
	unsigned delay = 0;
	unsigned number = 1;
	unsigned address = 0;
	unsigned data = 0;
	unsigned block = 0;
	unsigned tmo_ms = 10;
	unsigned link_mask = 0x111111;
	size_t count = 0;
	bool incrementAddress = true;
	std::string op = "";
	int dtc = -1;
	bool stopOnError = false;

	for (auto optind = 1; optind < argc; ++optind)
	{
		if (argv[optind][0] == '-')
		{
			switch (argv[optind][1])
			{
				case 'l':
					link = DTCLib::Utilities::getOptionValue(&optind, &argv);
					break;
				case 'd':
					delay = DTCLib::Utilities::getOptionValue(&optind, &argv);
					break;
				case 'n':
					number = DTCLib::Utilities::getOptionValue(&optind, &argv);
					break;
				case 'w':
					data = DTCLib::Utilities::getOptionValue(&optind, &argv);
					break;
				case 'a':
					address = DTCLib::Utilities::getOptionValue(&optind, &argv);
					break;
				case 'b':
					block = DTCLib::Utilities::getOptionValue(&optind, &argv);
					break;
				case 'c':
					count = DTCLib::Utilities::getOptionValue(&optind, &argv);
					break;
				case 'i':
					incrementAddress = !incrementAddress;
					break;
				case 'q':
					quiet = true;
					break;
				case 'Q':
					quiet = true;
					reallyQuiet = true;
					break;
				case '-':  // Long option
				{
					auto option = DTCLib::Utilities::getLongOptionOption(&optind, &argv);
					if (option == "--dtc")
					{
						dtc = DTCLib::Utilities::getLongOptionValue(&optind, &argv);
					}
					else if (option == "--stop-on-error")
					{
						stopOnError = true;
					}
					else if (option == "--timeout-ms")
					{
						tmo_ms = DTCLib::Utilities::getLongOptionValue(&optind, &argv);
					}
					else if (option == "--link-mask")
					{
						link_mask = DTCLib::Utilities::getLongOptionValue(&optind, &argv);
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

	if (op == "simple_read")
	{
		TRACE_CNTL("modeS", 0);
	}

	TLOG(TLVL_DEBUG) << "Options are: " << std::boolalpha
					 << "Operation: " << std::string(op)
					 << ", Num: " << number
					 << ", Link: " << link
					 << ", Link Mask: " << std::hex << std::showbase << link_mask << std::dec
					 << ", Delay: " << delay
					 << ", Address: " << address
					 << ", Data: " << data
					 << ", Block: " << block
					 << ", Quiet Mode: " << quiet
					 << ", Really Quiet Mode: " << reallyQuiet
					 << std::endl;

	auto dtc_link = static_cast<DTC_Link_ID>(link);
	link_mask |= 1 << (link * 4);                               // Always enable the link being tested
	auto thisDTC = new DTC(DTC_SimMode_NoCFO, dtc, link_mask);  // rocMask is in hex, not binary
	auto device = thisDTC->GetDevice();

	if (op == "read_register")
	{
		for (unsigned ii = 0; ii < number; ++ii)
		{
			TLOG(DCS_TLVL(reallyQuiet)) << "Operation \"read_register\" " << ii << std::endl;
			try
			{
				auto rocdata = thisDTC->ReadROCRegister(dtc_link, address, tmo_ms);
				TLOG(DCS_TLVL(reallyQuiet)) << "ROC " << dtc_link << " returned " << rocdata << " for address " << address;
			}
			catch (std::runtime_error& err)
			{
				TLOG(TLVL_ERROR) << "Error reading from ROC on iteration " << ii << ": " << err.what();
				if (stopOnError) break;
			}
		}
	}
	else if (op == "simple_read")
	{
		TLOG(TLVL_DEBUG) << "Operation \"simple_read\"" << std::endl;
		for (unsigned ii = 0; ii < number; ++ii)
		{
			TLOG(DCS_TLVL(reallyQuiet)) << "Simple Read " << ii << std::endl;
			try
			{
				auto rocdata = thisDTC->ReadROCRegister(dtc_link, address, tmo_ms);
				std::cout << std::dec << ii << " " << std::hex << std::showbase << rocdata << std::endl;
				TLOG(DCS_TLVL(reallyQuiet)) << "ROC " << dtc_link << " returned " << rocdata << " for address " << address;
			}
			catch (std::runtime_error& err)
			{
				TLOG(TLVL_ERROR) << "Error reading from ROC on iteration " << ii << ": " << err.what();
				if (stopOnError) break;
			}
		}
	}
	else if (op == "reset_roc")
	{
		TLOG(TLVL_DEBUG) << "Operation \"reset_roc\"" << std::endl;
		thisDTC->WriteExtROCRegister(dtc_link, 12, 1, 0x11, false, tmo_ms);
		thisDTC->WriteExtROCRegister(dtc_link, 11, 1, 0x11, false, tmo_ms);
		thisDTC->WriteExtROCRegister(dtc_link, 10, 1, 0x11, false, tmo_ms);
		thisDTC->WriteExtROCRegister(dtc_link, 9, 1, 0x11, false, tmo_ms);
		thisDTC->WriteExtROCRegister(dtc_link, 8, 1, 0x11, false, tmo_ms);
	}
	else if (op == "write_register")
	{
		for (unsigned ii = 0; ii < number; ++ii)
		{
			try
			{
				TLOG(TLVL_DEBUG) << "Operation \"write_register\" " << ii << std::endl;
				thisDTC->WriteROCRegister(dtc_link, address, data, false, tmo_ms);
			}
			catch (std::runtime_error& err)
			{
				TLOG(TLVL_ERROR) << "Error writing to ROC on iteration " << ii << ": " << err.what();
				if (stopOnError) break;
			}
		}
	}
	else if (op == "read_extregister")
	{
		for (unsigned ii = 0; ii < number; ++ii)
		{
			try
			{
				TLOG(DCS_TLVL(reallyQuiet)) << "Operation \"read_extregister\" " << ii << std::endl;
				auto rocdata = thisDTC->ReadExtROCRegister(dtc_link, block, address, tmo_ms);
				TLOG(DCS_TLVL(reallyQuiet)) << "ROC " << dtc_link << " returned " << rocdata << " for address " << address << ", block " << block;
			}
			catch (std::runtime_error& err)
			{
				TLOG(TLVL_ERROR) << "Error reading from ROC on iteration " << ii << ": " << err.what();
				if (stopOnError) break;
			}
		}
	}
	else if (op == "write_extregister")
	{
		for (unsigned ii = 0; ii < number; ++ii)
		{
			try
			{
				TLOG(TLVL_DEBUG) << "Operation \"write_extregister\" " << ii << std::endl;
				thisDTC->WriteExtROCRegister(dtc_link, block, address, data, false, tmo_ms);
			}
			catch (std::runtime_error& err)
			{
				TLOG(TLVL_ERROR) << "Error writing to ROC on iteration " << ii << ": " << err.what();
				if (stopOnError) break;
			}
		}
	}
	else if (op == "test_read")
	{
		TLOG(TLVL_DEBUG) << "Operation \"test_read\"" << std::endl;

		// device->begin_dcs_transaction();
		// thisDTC->SendDCSRequestPacket(dtc_link,  DTC_DCSOperationType_Read, address, quiet);

		for (unsigned ii = 0; ii < number; ++ii)
		{
			TLOG(DCS_TLVL(reallyQuiet)) << "Buffer Read " << ii << std::endl;
			mu2e_databuff_t* buffer;
			auto tmo_ms = 1500;
			device->begin_dcs_transaction();
			auto sts = device->read_data(DTC_DMA_Engine_DCS, reinterpret_cast<void**>(&buffer), tmo_ms);

			TLOG(TLVL_DEBUG) << "util - read for DCS - ii=" << ii << ", sts=" << sts << ", buffer=" << (void*)buffer;
			if (sts > 0)
			{
				auto bufSize = *reinterpret_cast<uint64_t*>(&buffer[0]);
				TLOG(TLVL_DEBUG) << "util - bufSize is " << bufSize;

				if (!reallyQuiet)
				{
					for (unsigned line = 0; line < static_cast<unsigned>(ceil((bufSize - 8) / 16)); ++line)
					{
						TLOG(TLVL_INFO) << "0x" << std::hex << std::setw(5) << std::setfill('0') << line << "0: ";
						// for (unsigned byte = 0; byte < 16; ++byte)
						for (unsigned byte = 0; byte < 8; ++byte)
						{
							if (line * 16 + 2 * byte < bufSize - 8u)
							{
								auto thisWord = reinterpret_cast<uint16_t*>(buffer)[4 + line * 8 + byte];
								// uint8_t thisWord = (((uint8_t*)buffer)[8 + (line * 16) + byte]);
								TLOG(TLVL_INFO) << std::setw(4) << static_cast<int>(thisWord) << " ";
							}
						}
						TLOG(TLVL_INFO) << std::endl;
					}
				}
			}
			device->read_release(DTC_DMA_Engine_DCS, 1);
			device->end_dcs_transaction();

			if (sts <= 0 && stopOnError)
			{
				TLOG(TLVL_ERROR) << "util - read for DCS: Error occurred, aborting!";
				break;
			}

			if (delay > 0) usleep(delay);
		}
	}
	else if (op == "toggle_serdes")
	{
		if (!thisDTC->ReadSERDESOscillatorClock())
		{
			TLOG(TLVL_INFO) << "Setting SERDES Oscillator Clock to 2.5 Gbps" << std::endl;
			thisDTC->SetSERDESOscillatorClock(DTC_SerdesClockSpeed_25Gbps);
		}
		else
		{
			TLOG(TLVL_INFO) << "Setting SERDES Oscillator Clock to 3.125 Gbps" << std::endl;
			thisDTC->SetSERDESOscillatorClock(DTC_SerdesClockSpeed_3125Gbps);
		}
	}
	else if (op == "read_release")
	{
		TLOG(TLVL_DEBUG) << "Operation \"read_release\"";
		for (unsigned ii = 0; ii < number; ++ii)
		{
			void* buffer;
			auto tmo_ms = 0;
			device->begin_dcs_transaction();
			auto stsRD = device->read_data(DTC_DMA_Engine_DCS, &buffer, tmo_ms);
			auto stsRL = device->read_release(DTC_DMA_Engine_DCS, 1);
			device->end_dcs_transaction();
			TLOG(TLVL_DEBUG + 7) << "dcs - release/read for DCS ii=" << ii << ", stsRD=" << stsRD << ", stsRL=" << stsRL << ", buffer=" << buffer;
			if (delay > 0) usleep(delay);
		}
	}
	else if (op == "block_read")
	{
		TLOG(TLVL_DEBUG) << "Operation \"block_read\"";
		for (unsigned ii = 0; ii < number; ++ii)
		{
			TLOG(DCS_TLVL(reallyQuiet)) << "Block Read " << ii << std::endl;
			try
			{
				std::vector<uint16_t> data(count);
				thisDTC->ReadROCBlock(data, dtc_link, address, count, incrementAddress, tmo_ms);
				DTCLib::Utilities::PrintBuffer(&data[0], data.size() * sizeof(uint16_t));
			}
			catch (std::runtime_error& err)
			{
				TLOG(TLVL_ERROR) << "Error reading from ROC on iteration " << ii << ": " << err.what();
				if (stopOnError) break;
			}
		}
	}
	else if (op == "block_write")
	{
		TLOG(TLVL_DEBUG) << "Operation \"block_write\"";
		for (unsigned ii = 0; ii < number; ++ii)
		{
			TLOG(DCS_TLVL(reallyQuiet)) << "Block Write " << ii << std::endl;
			try
			{
				std::vector<uint16_t> blockData;
				for (size_t ii = 0; ii < count; ++ii)
				{
					blockData.push_back(static_cast<uint16_t>(ii));
				}
				thisDTC->WriteROCBlock(dtc_link, address, blockData, false, incrementAddress, tmo_ms);
			}
			catch (std::runtime_error& err)
			{
				TLOG(TLVL_ERROR) << "Error writing to ROC on iteration " << ii << ": " << err.what();
				if (stopOnError) break;
			}
		}
	}
	else if (op == "raw_block_read")
	{
		TLOG(TLVL_DEBUG) << "Operation \"raw_block_read\"";
		for (unsigned ii = 0; ii < number; ++ii)
		{
			TLOG(DCS_TLVL(reallyQuiet)) << "Raw Block Read " << ii << std::endl;
			try
			{
				DTC_DCSRequestPacket req(dtc_link, DTC_DCSOperationType_BlockRead, false, incrementAddress, address, count);

				TLOG(TLVL_TRACE) << "rocUtil raw_block_read: before WriteDMADCSPacket - DTC_DCSRequestPacket";

				thisDTC->ReleaseAllBuffers(DTC_DMA_Engine_DCS);

				if (!thisDTC->ReadDCSReception()) thisDTC->EnableDCSReception();

				mu2e_databuff_t* buffer;
				auto tmo_ms = 1500;

				thisDTC->GetDevice()->begin_dcs_transaction();
				thisDTC->WriteDMAPacket(req);
				TLOG(TLVL_TRACE) << "rocUtil raw_block_read: after  WriteDMADCSPacket - DTC_DCSRequestPacket";
				usleep(2500);
				auto sts = device->read_data(DTC_DMA_Engine_DCS, reinterpret_cast<void**>(&buffer), tmo_ms);
				device->read_release(DTC_DMA_Engine_DCS, 1);
				thisDTC->GetDevice()->end_dcs_transaction();
				TLOG(TLVL_TRACE) << "rocUtil raw_block_read: after DCS read_data - "
								 << " sts=" << sts << ", buffer=" << (void*)buffer;

				if (sts > 0)
				{
					void* readPtr = &buffer[0];
					auto bufSize = static_cast<uint16_t>(*static_cast<uint64_t*>(readPtr));
					readPtr = static_cast<uint8_t*>(readPtr) + 8;
					TLOG((reallyQuiet ? TLVL_DEBUG + 5 : TLVL_INFO)) << "Buffer reports DMA size of " << std::dec << bufSize << " bytes. Device driver reports read of "
																	 << sts << " bytes," << std::endl;

					TLOG(TLVL_TRACE) << "util - bufSize is " << bufSize;

					if (!reallyQuiet)
					{
						DTCLib::Utilities::PrintBuffer(buffer, sts >= bufSize ? sts : bufSize);
					}
				}
				if (delay > 0) usleep(delay);
			}
			catch (std::runtime_error& err)
			{
				TLOG(TLVL_ERROR) << "Error reading from ROC on iteration " << ii << ": " << err.what();
				if (stopOnError) break;
			}
		}
	}
	else
	{
		TLOG(TLVL_ERROR) << "Unrecognized operation: " << op << std::endl;
		printHelpMsg();
	}

	delete thisDTC;

	if (op == "simple_read")
	{
		TRACE_CNTL("modeS", 1);
	}
	return 0;
}  // main
