
#include <vector>
#include <string>
#include <unordered_map>
#include <map>
#include <deque>
#include <fstream>

#include "mu2e_driver/mu2e_mmap_ioctl.h"
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

#include "TRACE/tracemf.h"
#define TRACE_NAME "binary_file_compare"

typedef std::map<uint8_t, std::list<std::vector<uint8_t>>> roc_map;
typedef std::map<uint8_t, roc_map> dtc_map;
typedef std::map<uint8_t, dtc_map> subsystem_map;
typedef std::map<uint64_t, subsystem_map> event_map;

void printHelpMsg()
{
	std::cout << "Compares DTC binary files to look for differences" << std::endl;
	std::cout << "Usage: DTC_binary_file_compare [options] file1 file2"
			  << std::endl;
	std::cout
		<< "Options are:" << std::endl
		<< "    -h, --help: This message." << std::endl
		<< std::endl;
	exit(0);
}

uint8_t get_byte(mu2e_databuff_t& buf, size_t offset, size_t index)
{
	return *(reinterpret_cast<uint8_t*>(reinterpret_cast<DataHeaderPacket*>(buf + offset) + 1) + index);
}

size_t add_to_map(event_map& map, mu2e_databuff_t& buf, size_t offset)
{
	auto header = *reinterpret_cast<DataHeaderPacket*>(buf + offset);
	auto blockByteSize = header.s.TransferByteCount;

	uint64_t timestamp = header.s.ts10 + (static_cast<uint64_t>(header.s.ts32) << 16) + (static_cast<uint64_t>(header.s.ts54) << 32);
	auto bufSize = blockByteSize - 0x10;

	// TLOG(TLVL_DEBUG) << "Adding TS " << timestamp << " SS " << header.s.SubsystemID << " DTC " << header.s.DTCID << " ROC " << header.s.LinkID << " to map with size " << bufSize;
	map[timestamp][header.s.SubsystemID][header.s.DTCID][header.s.LinkID].push_back(std::vector<uint8_t>(bufSize));
	for (auto jj = 0; jj < bufSize; ++jj)
	{
		map[timestamp][header.s.SubsystemID][header.s.DTCID][header.s.LinkID].back()[jj] = get_byte(buf, offset, jj);
	}

	return blockByteSize;
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

	if (binaryFiles.size() < 2 || binaryFiles.size() > 2)
	{
		TLOG(TLVL_ERROR) << "You must specify two files to compare!";
		printHelpMsg();
		exit(1);
	}

	// Theory of comparison: Load first file into event_map, then go through second file, removing equivalent items from event_map. At the end, print the remainder of event_map, plus any disagreements.
	event_map first_file_contents;
	event_map second_file_disagreements;

	std::ifstream is(binaryFiles[0]);
	if (is.bad() || !is)
	{
		TLOG(TLVL_ERROR) << "Cannot read file " << binaryFiles[0];
		exit(2);
	}
	TLOG(TLVL_INFO) << "Reading binary file " << binaryFiles[0];
	uint64_t total_size_read = 0;

	mu2e_databuff_t buf;

	while (is)
	{
		uint64_t dmaWriteSize;  // DMA Write buffer size
		is.read((char*)&dmaWriteSize, sizeof(dmaWriteSize));
		total_size_read += sizeof(dmaWriteSize);

		uint64_t dmaSize;  // DMA buffer size
		is.read((char*)&dmaSize, sizeof(dmaSize));
		total_size_read += sizeof(dmaSize);

		// Check that size of all DataBlocks = DMA Buffer Size
		is.read((char*)buf, dmaSize);
		total_size_read += dmaSize;

		size_t offset = 0;
		while (offset < dmaSize)
		{
			offset += add_to_map(first_file_contents, buf, offset);
		}
	}

	TLOG(TLVL_INFO) << "Finished with file " << binaryFiles[0] << ", bytes read " << total_size_read;

	std::ifstream is2(binaryFiles[1]);
	if (is2.bad() || !is2)
	{
		TLOG(TLVL_ERROR) << "Cannot read file " << binaryFiles[1];
		exit(2);
	}
	TLOG(TLVL_INFO) << "Reading binary file " << binaryFiles[1];
	total_size_read = 0;

	while (is2)
	{
		uint64_t dmaWriteSize;  // DMA Write buffer size
		is2.read((char*)&dmaWriteSize, sizeof(dmaWriteSize));
		total_size_read += sizeof(dmaWriteSize);

		uint64_t dmaSize;  // DMA buffer size
		is2.read((char*)&dmaSize, sizeof(dmaSize));
		total_size_read += sizeof(dmaSize);

		// Check that size of all DataBlocks = DMA Buffer Size
		is2.read((char*)buf, dmaSize);
		total_size_read += dmaSize;

		size_t offset = 0;
		while (offset < dmaSize)
		{
			auto header = *reinterpret_cast<DataHeaderPacket*>(buf + offset);
			size_t blockByteSize = header.s.TransferByteCount;

			uint64_t timestamp = (static_cast<uint64_t>(header.s.ts10) << 32) + (static_cast<uint64_t>(header.s.ts32) << 16) + (static_cast<uint64_t>(header.s.ts54));
			size_t bufSize = blockByteSize - 0x10;

			if (first_file_contents.count(timestamp) && first_file_contents[timestamp].count(header.s.SubsystemID) && first_file_contents[timestamp][header.s.SubsystemID].count(header.s.DTCID) && first_file_contents[timestamp][header.s.SubsystemID][header.s.DTCID].count(header.s.LinkID))
			{
				auto agree_it = first_file_contents[timestamp][header.s.SubsystemID][header.s.DTCID][header.s.LinkID].end();
				auto ii = first_file_contents[timestamp][header.s.SubsystemID][header.s.DTCID][header.s.LinkID].begin();
				for (; ii != agree_it; ++ii)
				{
					if (ii->size() != bufSize) continue;
					bool this_agree = true;
					for (size_t jj = 0; jj < bufSize; ++jj)
					{
						if (ii->at(jj) != get_byte(buf, offset, jj))
						{
							TLOG(TLVL_WARNING) << "Disagreement found! TS " << timestamp << " SS " << header.s.SubsystemID << " DTC " << header.s.DTCID << " ROC " << header.s.LinkID << " byte " << jj << ": " << ii->at(jj) << " != " << get_byte(buf, offset, jj);
							this_agree = false;
							break;
						}
					}
					if (this_agree)
					{
						agree_it = ii;
						break;
					}
				}
				if (agree_it != first_file_contents[timestamp][header.s.SubsystemID][header.s.DTCID][header.s.LinkID].end())
				{
					first_file_contents[timestamp][header.s.SubsystemID][header.s.DTCID][header.s.LinkID].erase(agree_it);
					if (first_file_contents[timestamp][header.s.SubsystemID][header.s.DTCID][header.s.LinkID].size() == 0)
					{
						first_file_contents[timestamp][header.s.SubsystemID][header.s.DTCID]
							.erase(header.s.LinkID);
						if (first_file_contents[timestamp][header.s.SubsystemID][header.s.DTCID].size() == 0)
						{
							first_file_contents[timestamp][header.s.SubsystemID].erase(header.s.DTCID);
							if (first_file_contents[timestamp][header.s.SubsystemID].size() == 0)
							{
								first_file_contents[timestamp].erase(header.s.SubsystemID);
								if (first_file_contents[timestamp].size() == 0)
								{
									first_file_contents.erase(timestamp);
								}
							}
						}
					}
				}
				else
				{
					TLOG(TLVL_WARNING) << "Disagreement found! TS " << timestamp << " SS " << header.s.SubsystemID << " DTC " << header.s.DTCID << " ROC " << header.s.LinkID << " no matching entries found";
					add_to_map(second_file_disagreements, buf, offset);
				}
			}
			else
			{
				TLOG(TLVL_WARNING) << "Disagreement found! TS " << timestamp << " SS " << header.s.SubsystemID << " DTC " << header.s.DTCID << " ROC " << header.s.LinkID << " Not found in first file map!";
				add_to_map(second_file_disagreements, buf, offset);
			}

			offset += blockByteSize;
		}
	}

	TLOG(TLVL_INFO) << "Finished with file " << binaryFiles[1] << ", bytes read " << total_size_read;
	TLOG(TLVL_INFO) << "There are " << first_file_contents.size() << " events remaining from the first file. There are " << second_file_disagreements.size() << " events with different data in the second file.";
	for (auto& ts : first_file_contents)
	{
		TLOG(TLVL_TRACE) << "First file remainder: Event " << ts.first;
		for (auto& subsys : ts.second)
		{
			for (auto& dtc : subsys.second)
			{
				for (auto& roc : dtc.second)
				{
					TLOG(TLVL_TRACE) << "\tTS " << ts.first << " SS " << subsys.first << " DTC " << dtc.first << " ROC " << roc.first << ": sz: " << roc.second.size();
					for (auto& it : roc.second)
					{
						TLOG(TLVL_TRACE) << "\t\tByte vector sz=" << it.size();
					}
				}
			}
		}
	}
	for (auto& ts : second_file_disagreements)
	{
		TLOG(TLVL_TRACE) << "Second file disagreements: Event " << ts.first;
		for (auto& subsys : ts.second)
		{
			for (auto& dtc : subsys.second)
			{
				for (auto& roc : dtc.second)
				{
					TLOG(TLVL_TRACE) << "\tTS " << ts.first << " SS " << subsys.first << " DTC " << dtc.first << " ROC " << roc.first << ": sz: " << roc.second.size();
					for (auto& it : roc.second)
					{
						TLOG(TLVL_TRACE) << "\t\tByte vector sz=" << it.size();
					}
				}
			}
		}
	}
}