
#include <bitset>
#include <fstream>
#include <unordered_map>
#include <vector>

// #include "artdaq-core-mu2e/Overlays/DTC_Packets.h"
#include "artdaq-core-mu2e/Overlays/DTC_Packets/DTC_DCSReplyPacket.h"
#include "artdaq-core-mu2e/Overlays/DTC_Packets/DTC_DCSRequestPacket.h"
#include "artdaq-core-mu2e/Overlays/DTC_Packets/DTC_DMAPacket.h"
#include "artdaq-core-mu2e/Overlays/DTC_Packets/DTC_DataBlock.h"
#include "artdaq-core-mu2e/Overlays/DTC_Packets/DTC_DataHeaderPacket.h"
#include "artdaq-core-mu2e/Overlays/DTC_Packets/DTC_DataPacket.h"
#include "artdaq-core-mu2e/Overlays/DTC_Packets/DTC_DataRequestPacket.h"
#include "artdaq-core-mu2e/Overlays/DTC_Packets/DTC_DataStatus.h"
#include "artdaq-core-mu2e/Overlays/DTC_Packets/DTC_Event.h"
#include "artdaq-core-mu2e/Overlays/DTC_Packets/DTC_EventHeader.h"
#include "artdaq-core-mu2e/Overlays/DTC_Packets/DTC_HeartbeatPacket.h"
#include "artdaq-core-mu2e/Overlays/DTC_Packets/DTC_PacketType.h"
#include "artdaq-core-mu2e/Overlays/DTC_Packets/DTC_SubEvent.h"
#include "artdaq-core-mu2e/Overlays/DTC_Packets/DTC_SubEventHeader.h"

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
#include "artdaq-core-mu2e/Overlays/DTC_Types/DTC_SERDESLoopbackMode.h"
#include "artdaq-core-mu2e/Overlays/DTC_Types/DTC_SERDESRXDisparityError.h"
#include "artdaq-core-mu2e/Overlays/DTC_Types/DTC_SerdesClockSpeed.h"
#include "artdaq-core-mu2e/Overlays/DTC_Types/DTC_SimMode.h"
#include "artdaq-core-mu2e/Overlays/DTC_Types/DTC_Subsystem.h"
#include "artdaq-core-mu2e/Overlays/DTC_Types/Exceptions.h"
#include "artdaq-core-mu2e/Overlays/DTC_Types/Utilities.h"

#include "mu2e_driver/mu2e_mmap_ioctl.h"

namespace DTCLib
{
class DTC_Data_Verifier
{
  public:
	bool VerifyTrackerDataBlock(DTCLib::DTC_DataBlock /*block*/)
	{
		return true;
	}

	bool VerifyCalorimeterDataBlock(DTCLib::DTC_DataBlock block)
	{
		auto blockByteSize = block.GetHeader()->GetByteCount();

		if(blockByteSize == 0x10)
		{
			// TLOG(TLVL_WARNING) << "VerifyCalorimeterDataBlock: Empty block encountered!";
			return true;
		}

		auto dataPtr       = reinterpret_cast<const uint16_t*>(block.GetData());
		auto hitCount      = *dataPtr;
		auto currentOffset = 0;

		std::vector<uint16_t> hitOffsets;
		for(int ii = 0; ii < hitCount; ++ii)
		{
			hitOffsets.push_back(*(++dataPtr));
			currentOffset += 2;
			if(currentOffset > blockByteSize)
			{
				TLOG(TLVL_ERROR) << "VerifyCalorimeterDataBlock: Calorimeter data extends past declared block size! (0x" << std::hex << currentOffset << " > 0x" << std::hex << blockByteSize << ")";
				return false;
			}
		}

		++dataPtr;
		currentOffset += 2;

		auto channelStatusA = (*dataPtr & 0xFC00) >> 10;

		++dataPtr;
		currentOffset += 2;

		auto channelStatusB = (*dataPtr & 0x3FFF);
		if((*dataPtr & 0xC000) != 0)
		{
			TLOG(TLVL_ERROR) << "VerifyCalorimeterDataBlock: Data present in BoardID Reserved field!";
			return false;
		}

		if(channelStatusA == 0x0 && channelStatusB != 0x0)
		{
			TLOG(TLVL_WARNING) << "VerifyCalorimeterDataBlock: None of the 20 channels are enabled! StsA: 0x" << std::hex << channelStatusA << ", stsB: 0x" << std::hex << channelStatusB;
			// Not sure if this is a fatal error or not, leaving it for now
			// return false;
		}

		if(hitCount == 0)
		{
			TLOG(TLVL_WARNING) << "VerifyCalorimeterDataBlock: There are zero hits in this block!";
		}

		for(int ii = 0; ii < hitCount; ++ii)
		{
			++dataPtr;
			currentOffset += 2;
			if(currentOffset != hitOffsets[ii])
			{
				TLOG(TLVL_ERROR) << "VerifyCalorimeterDataBlock: Hit " << ii << " index value " << hitOffsets[ii] << " does not agree with current offset " << currentOffset;
				return false;
			}

			// auto channelNumber = *dataPtr & 0x3F;
			// auto diracA = (*dataPtr >> 6) & 0x3FF;
			++dataPtr;
			auto diracB = *dataPtr;

			auto sipmID    = diracB >> 12;
			auto crystalID = diracB & 0xFFF;

			if(sipmID != 0 && sipmID != 1)
			{
				TLOG(TLVL_WARNING) << "Invalid sipmID " << sipmID << " detected!";
			}
			if(crystalID > 674 * 2)
			{
				TLOG(TLVL_WARNING) << "Invalid crystalID " << crystalID << " detected!";
			}

			dataPtr += 2;

			auto time = *dataPtr;
			if(time < 500)
			{
				TLOG(TLVL_WARNING) << "VerifyCalorimeterBlock: Suspicious time " << time << " detected!";
			}

			++dataPtr;
			currentOffset += 8;

			auto numSamples          = *dataPtr & 0xFF;
			auto maxSample           = (*dataPtr & 0xFF00) >> 8;
			auto currentMaximumValue = 0;
			auto currentMaximumIndex = 0;

			if(numSamples == 0)
			{
				TLOG(TLVL_WARNING) << "VerifyCalorimeterBlock: This hit has zero samples!";
			}

			for(int jj = 0; jj < numSamples; ++jj)
			{
				++dataPtr;
				currentOffset += 2;

				if(*dataPtr > currentMaximumValue)
				{
					currentMaximumValue = *dataPtr;
					currentMaximumIndex = jj;
				}
			}

			if(maxSample != currentMaximumIndex)
			{
				TLOG(TLVL_ERROR) << "VerifyCalorimeterDataBlock: Hit " << ii << " has mismatched maximum sample; expected " << maxSample << ", actual maximum " << currentMaximumIndex;
				return false;
			}
		}

		++dataPtr;
		currentOffset += 2;
		while(currentOffset % 16 != 0)
		{
			if(*dataPtr != 0)
			{
				TLOG(TLVL_ERROR) << "VerifyCalorimeterDataBlock: Data detected in end padding: 0x" << std::hex << *dataPtr;
				return false;
			}
			++dataPtr;
			currentOffset += 2;
		}

		return true;
	}

	bool VerifyCRVDataBlock(DTCLib::DTC_DataBlock /*block*/)
	{
		return true;
	}

	bool VerifyROCEmulatorBlock(DTCLib::DTC_DataBlock block)
	{
		auto headerDP = block.GetHeader()->ConvertToDataPacket();

		bool success = true;
		success      = headerDP.GetByte(12) == 0xEF;
		success &= headerDP.GetByte(13) == 0xBE;
		success &= headerDP.GetByte(15) == 0xBE;
		if(!success)
		{
			TLOG(TLVL_ERROR) << "VerifyROCEmulatorBlock: Header format is incorrect (check bytes)";
			return false;
		}

		auto packetCount = block.GetHeader()->GetPacketCount();
		auto roc         = block.GetHeader()->GetLinkID();
		auto dataPtr     = reinterpret_cast<uint16_t const*>(block.GetData());

		for(int ii = 0; ii < packetCount; ++ii)
		{
			success = *dataPtr == 0x1111;
			++dataPtr;
			success &= *dataPtr == 0x2222;
			++dataPtr;

			uint32_t roc_packet_counter_test = (*dataPtr << 16);
			++dataPtr;
			roc_packet_counter_test += *dataPtr;
			++dataPtr;

			if(roc_emulator_packet_counters_.count(roc) == 0)
			{
				roc_emulator_packet_counters_[roc] = roc_packet_counter_test;
			}

			if(roc_packet_counter_test != roc_emulator_packet_counters_[roc])
			{
				TLOG(TLVL_INFO) << "VerifyROCEmulatorBlock: ROC Emulator packet counter for roc " << static_cast<int>(roc) << " unexpected, shifting from " << roc_emulator_packet_counters_[roc] << " (expected) to " << roc_packet_counter_test << " (received) ";
				roc_emulator_packet_counters_[roc] = roc_packet_counter_test;
			}

			success &= *dataPtr == 0x3333;
			++dataPtr;
			success &= *dataPtr == 0x4444;
			++dataPtr;

			roc_packet_counter_test = (*dataPtr << 16);
			++dataPtr;
			roc_packet_counter_test += *dataPtr;
			success &= (roc_packet_counter_test == roc_emulator_packet_counters_[roc]);
			++dataPtr;

			roc_emulator_packet_counters_[roc]++;

			if(!success)
			{
				TLOG(TLVL_ERROR) << "VerifyROCEmulatorBlock: Data packet " << ii << " has format error";
				dataPtr -= 8;
				std::vector<uint16_t> pktData(8);
				for(size_t jj = 0; jj < 8; ++jj)
				{
					pktData[jj] = *dataPtr;
					++dataPtr;
				}
				TLOG(TLVL_ERROR) << "Packet data: " << std::hex << pktData[0] << " " << pktData[1]
				                 << " " << pktData[2] << " " << pktData[3]
				                 << " " << pktData[4] << " " << pktData[5]
				                 << " " << pktData[6] << " " << pktData[7];
				return false;
			}
		}

		return true;
	}

	bool VerifyBlock(DTCLib::DTC_DataBlock block)
	{
		auto header        = block.GetHeader();
		auto blockByteSize = header->GetByteCount();

		// Check that this is indeed a DataHeader packet
		auto     dataHeaderMask = 0x80F0;
		uint16_t dataHeaderTest = static_cast<uint16_t>(header->ConvertToDataPacket().GetByte(2)) + (static_cast<uint16_t>(header->ConvertToDataPacket().GetByte(3)) << 8);
		TLOG(TLVL_DEBUG + 5) << "Block size: 0x" << std::hex << blockByteSize << ", Test word: " << std::hex << dataHeaderTest << ", masked: " << (dataHeaderTest & dataHeaderMask) << " =?= 0x8050";
		if((dataHeaderTest & dataHeaderMask) != 0x8050)
		{
			auto offset = file_mode_ ? (current_buffer_offset_ + current_buffer_pos_) : current_buffer_pos_;
			TLOG(TLVL_ERROR) << "Encountered bad data at 0x" << std::hex << offset << ": expected DataHeader, got 0x" << std::hex << *reinterpret_cast<const uint64_t*>(block.blockPointer);

			Utilities::PrintBuffer(block.blockPointer, 16);

			// go to next file
			continueFile_ = false;
			return false;
		}

		auto packetCountTest = header->GetPacketCount();
		if((packetCountTest + 1) * 16 != blockByteSize)
		{
			TLOG(TLVL_ERROR) << "Block data packet count and byte count disagree! packetCount: " << packetCountTest << ", which implies block size of 0x" << std::hex << ((packetCountTest + 1) * 16) << ", blockSize: 0x" << std::hex << blockByteSize;

			Utilities::PrintBuffer(block.blockPointer, 16);
			// We don't have to skip to the next file, because we already know the data block integrity is fine.
			current_buffer_pos_ += blockByteSize;
			return false;
		}

		auto subsystemID    = header->GetSubsystemID();
		bool subsystemCheck = true;
		switch(subsystemID)
		{
		case 0:  // Tracker
			subsystemCheck = VerifyTrackerDataBlock(block);
			break;
		case 1:  // Calorimeter
			subsystemCheck = VerifyCalorimeterDataBlock(block);
			break;
		case 2:  // CRV
			subsystemCheck = VerifyCRVDataBlock(block);
			break;
		case 3:  // ROC Emulator
			subsystemCheck = VerifyROCEmulatorBlock(block);
			break;
		default:
			TLOG(TLVL_INFO) << "Data-level verification not implemented for subsystem ID " << subsystemID;
			break;
		}
		if(!subsystemCheck)
		{
			auto offset = file_mode_ ? (current_buffer_offset_ + current_buffer_pos_) : current_buffer_pos_;
			TLOG(TLVL_ERROR) << "Data block at 0x" << std::hex << offset << " is not a valid data block for subsystem ID " << static_cast<int>(subsystemID);

			current_buffer_pos_ += blockByteSize;

			return false;
			// We don't have to skip to the next file, because we already know the data block integrity is fine.
		}

		current_buffer_pos_ += blockByteSize;
		return true;
	}

	bool VerifySubEvent(DTCLib::DTC_SubEvent subevt, DTCLib::DTC_EventWindowTag eventTag)
	{
		if(subevt.GetEventWindowTag() != eventTag)
		{
			TLOG(TLVL_WARNING) << "Event Window Tag from Event does not agree with EWT from SubEvent! (" << eventTag.GetEventWindowTag(true) << " != " << subevt.GetEventWindowTag().GetEventWindowTag(true) << ")";
		}
		if(subevt.GetHeader()->num_rocs != subevt.GetDataBlockCount())
		{
			TLOG(TLVL_WARNING) << "SubEvent Header num_rocs field disagrees with number of DataBlocks! (" << subevt.GetHeader()->num_rocs << " != " << subevt.GetDataBlockCount() << ")";
		}
		TLOG(TLVL_DEBUG + 4) << subevt.GetHeader()->toJson();
		current_buffer_pos_ += sizeof(DTCLib::DTC_SubEventHeader);

		bool success = true;
		for(auto& block : subevt.GetDataBlocks())
		{
			auto blockSuccess = VerifyBlock(block);
			success &= blockSuccess;
			if(file_mode_ && !continueFile_)
				return false;
		}
		return success;
	}

	bool VerifyEvent(DTCLib::DTC_Event evt)
	{
		bool success        = true;
		current_buffer_pos_ = 0;

		auto eventTag = evt.GetEventWindowTag();

		if(evt.GetHeader()->num_dtcs != evt.GetSubEventCount())
		{
			TLOG(TLVL_WARNING) << "Event Header num_dtcs field (" << evt.GetHeader()->num_dtcs << ") disagrees with number of SubEvents! (" << evt.GetSubEventCount() << ")";
		}

		TLOG(TLVL_DEBUG + 3) << evt.GetHeader()->toJson();

		current_buffer_pos_ += sizeof(DTCLib::DTC_EventHeader);

		for(auto& subevt : evt.GetSubEvents())
		{
			auto subevtSuccess = VerifySubEvent(subevt, eventTag);
			success &= subevtSuccess;
			if(file_mode_ && !continueFile_)
				return false;
		}
		return success;
	}

	bool VerifyFile(std::string file)
	{
		std::ifstream is(file);
		if(is.bad() || !is)
		{
			TLOG(TLVL_ERROR) << "Cannot read file " << file;
			return false;
		}
		TLOG(TLVL_INFO) << "Reading binary file " << file;
		size_t total_size_read = 0;
		file_mode_             = true;

		mu2e_databuff_t buf;

		bool success  = true;
		continueFile_ = true;
		while(is && continueFile_)
		{
			uint64_t dmaWriteSize;  // DMA Write buffer size
			is.read((char*)&dmaWriteSize, sizeof(dmaWriteSize));
			if(!is || is.eof())
				break;

			total_size_read += sizeof(dmaWriteSize);

			uint64_t dmaSize;
			is.read((char*)&dmaSize, sizeof(dmaSize));

			// Check that DMA Write Buffer Size = DMA Buffer Size + 8
			if(dmaSize + 8 != dmaWriteSize)
			{
				TLOG(TLVL_ERROR) << "Buffer error detected: DMA Size mismatch at " << std::showbase << std::hex << total_size_read << ". Write size: " << static_cast<size_t>(dmaWriteSize) << ", DMA Size: " << static_cast<size_t>(dmaSize);
				success = false;
				break;
			}

			if(dmaSize > SET_DTC_MAX_DMA_SIZE)
			{
				TLOG(TLVL_WARNING) << "Over-size block detected! DTC has a limit of " << SET_DTC_MAX_DMA_SIZE << ", dma size is " << std::showbase << std::hex << dmaSize << "!";
			}

			// Check that size of all DataBlocks = DMA Buffer Size
			is.read((char*)buf, dmaSize - 8);
			total_size_read += dmaSize;
			current_buffer_offset_ = total_size_read;

			TLOG(TLVL_DEBUG + 1) << "Verifying event at offset " << std::showbase << std::hex << current_buffer_offset_;
			DTCLib::DTC_Event thisEvent(buf);

			TLOG(TLVL_DEBUG + 2) << "Getting Size of Event...";
			size_t eventByteCount = thisEvent.GetEventByteCount();
			if(eventByteCount > dmaSize - 8U)
			{
				TLOG(TLVL_DEBUG + 1) << "Event is continued in next DMA! (Event size " << std::showbase << std::hex << eventByteCount << ", first DMA size " << dmaSize - 8U << ")";
				DTC_Event newEvt(eventByteCount);
				memcpy(const_cast<void*>(newEvt.GetRawBufferPointer()), thisEvent.GetRawBufferPointer(), dmaSize - 8);
				size_t newEvtSize = dmaSize - 8;
				while(newEvtSize < eventByteCount)
				{
					is.read((char*)&dmaWriteSize, sizeof(dmaWriteSize));
					if(!is || is.eof())
					{
						TLOG(TLVL_ERROR) << "File ended while reading continued DMA!";
						break;
					}

					total_size_read += sizeof(dmaWriteSize);

					is.read((char*)&dmaSize, sizeof(dmaSize));

					// Check that DMA Write Buffer Size = DMA Buffer Size + 8
					if(dmaSize + 8 != dmaWriteSize)
					{
						TLOG(TLVL_ERROR) << "Buffer error detected: DMA Size mismatch at " << std::showbase << std::hex << total_size_read << ". Write size: " << static_cast<size_t>(dmaWriteSize) << ", DMA Size: " << static_cast<size_t>(dmaSize);
						success = false;
						break;
					}

					// Check that size of all DataBlocks = DMA Buffer Size
					TLOG(TLVL_DEBUG + 1) << "Reading continued DMA at offset " << std::showbase << std::hex << total_size_read << " of size " << dmaSize - 8 << " into event";
					is.read((char*)buf, dmaSize - 8);
					current_buffer_offset_ = total_size_read;
					total_size_read += dmaSize;

					size_t bytes_to_read = dmaSize - 8;
					if(newEvtSize + dmaSize - 8 > eventByteCount)
					{
						bytes_to_read = eventByteCount - newEvtSize;
					}

					memcpy(const_cast<uint8_t*>(static_cast<const uint8_t*>(newEvt.GetRawBufferPointer()) + newEvtSize), buf, bytes_to_read);
					newEvtSize += dmaSize - 8;
				}

				newEvt.SetupEvent();
				success = VerifyEvent(newEvt);
			}
			else
			{
				thisEvent.SetupEvent();
				success = VerifyEvent(thisEvent);
			}
		}

		if(success)
		{
			TLOG(TLVL_INFO) << "File " << file << " verified successfully!";
		}
		else
		{
			TLOG(TLVL_WARNING) << "File " << file << " had verification errors, see TRACE output for details";
		}

		return success;
	}

  private:
	bool                                              continueFile_{true};
	bool                                              file_mode_{false};
	uint64_t                                          current_buffer_offset_{0};
	uint64_t                                          current_buffer_pos_{0};
	std::unordered_map<DTCLib::DTC_Link_ID, uint32_t> roc_emulator_packet_counters_;
};
}  // namespace DTCLib
