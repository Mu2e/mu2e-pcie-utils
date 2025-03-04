// This file (util_main.cc) was created by Ron Rechenmacher <ron@fnal.gov> on
// May 13, 2015. "TERMS AND CONDITIONS" governing this file are in the README
// or COPYING file. If you do not have such a file, one can be obtained by
// contacting Ron or Fermi Lab in Batavia IL, 60510, phone: 630-840-3000.
// $RCSfile: .emacs.gnu,v $
// rev="$Revision: 1.23 $$Date: 2012/01/23 15:32:40 $";

#include <filesystem>

#include "dtcInterfaceLib/DTC_Data_Verifier.h"

#include "TRACE/tracemf.h"
#define TRACE_NAME "binary_dump_to_DTC"

void printHelpMsg()
{
	std::cout << "Converts binary data dump file(s) to DTC binary file(s)" << std::endl;
	std::cout << "Usage: binary_dump_to_DTC_file [options] file.."
			  << std::endl;
	std::cout
		<< "Options are:" << std::endl
		<< "    -h, --help: This message." << std::endl
		<< std::endl;
	exit(0);
}

int main(int argc, char* argv[])
{
	std::vector<std::string> binaryFiles;
	for (auto optind = 1; optind < argc; ++optind)
	{
		if (argv[optind][0] == '-')
		{
			switch (argv[optind][1])
			{
				case '-':  // Long option
				{
					auto option = DTCLib::Utilities::getLongOptionOption(&optind, &argv);
					if (option == "--help")
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
			binaryFiles.push_back(std::string(argv[optind]));
		}
	}

	for (auto& file : binaryFiles)
	{
		std::ifstream is(file);
		std::filesystem::path p(file);
		std::string ofilename = p.parent_path().string() + "/DTC_packets_" + p.filename().string();
		std::ofstream of(ofilename, std::ios::trunc | std::ios::binary);
		DTCLib::DTC_Data_Verifier verifier;

		if (is.bad() || !is)
		{
			TLOG(TLVL_ERROR) << "Cannot read file " << file;
			continue;
		}
		TLOG(TLVL_INFO) << "Reading binary file " << file << " and writing " << ofilename;

		mu2e_databuff_t buf;

		bool success = true;
		size_t total_size_read = 0;
		while (is)
		{
			uint64_t dmaSize;  // DMA buffer size
			is.read((char*)&dmaSize, sizeof(dmaSize));
			if (!is || is.eof()) break;

			uint64_t dmaReadSize = static_cast<uint16_t>(dmaSize);
			memcpy(buf, &dmaSize, sizeof(dmaSize));
			is.read(((char*)buf) + 8, dmaReadSize - 8);

			TLOG(TLVL_DEBUG + 1) << "Verifying event at offset 0x" << std::hex << total_size_read;
			auto thisEvent = std::make_unique<DTCLib::DTC_Event>(buf);

			auto eventByteCount = thisEvent->GetEventByteCount();
			if (eventByteCount == 0)
			{
				TLOG(TLVL_ERROR) << "Event byte count cannot be 0! Aborting file read at " << std::hex << total_size_read;
				break;
			}

			// Check for continued DMA
			if (eventByteCount > dmaReadSize)
			{
				auto inmem = std::make_unique<DTCLib::DTC_Event>(eventByteCount);
				memcpy(const_cast<void*>(inmem->GetRawBufferPointer()), thisEvent->GetRawBufferPointer(), dmaReadSize);

				auto bytes_read = dmaReadSize;

				while (bytes_read < eventByteCount)
				{
					total_size_read += dmaReadSize;  // Add here before we read the next DMA size
					TLOG(TLVL_DEBUG) << "Obtaining next DMA, bytes_read=" << bytes_read << ", eventByteCount=" << eventByteCount;

					is.read((char*)&dmaSize, sizeof(dmaSize));
					if (!is || is.eof()) break;

					dmaReadSize = static_cast<uint16_t>(dmaSize);
					memcpy(buf, &dmaSize, sizeof(dmaSize));
					is.read(((char*)buf) + 8, dmaReadSize - 8);

					size_t remainingEventSize = eventByteCount - bytes_read;
					size_t copySize = remainingEventSize < dmaReadSize ? remainingEventSize : dmaReadSize;
					memcpy(const_cast<uint8_t*>(static_cast<const uint8_t*>(inmem->GetRawBufferPointer()) + bytes_read), buf, copySize);
					bytes_read += dmaReadSize;
				}

				thisEvent.swap(inmem);
			}
			thisEvent->SetupEvent();

			success = verifier.VerifyEvent(*thisEvent);

			if (success)
			{
				thisEvent->WriteEvent(of, true);
			}
			else
			{
				TLOG(TLVL_ERROR) << "Error verifying event data. Aborting file read at " << std::hex << total_size_read;
				break;
			}

			total_size_read += dmaReadSize;
		}
		of.flush();
		of.close();
		if (success)
		{
			TLOG(TLVL_INFO) << "File " << file << " successfully written to " << ofilename;
		}
	}
}
