// This file (mu2edev.cc) was created by Ron Rechenmacher <ron@fnal.gov> on
// Feb 13, 2014. "TERMS AND CONDITIONS" governing this file are in the README
// or COPYING file. If you do not have such a file, one can be obtained by
// contacting Ron or Fermi Lab in Batavia IL, 60510, phone: 630-840-3000.
// $RCSfile: .emacs.gnu,v $
// rev="$Revision: 1.23 $$Date: 2012/01/23 15:32:40 $";

/*
 *    make mu2edev.o CFLAGS='-g -Wall -std=c++0x'
 */

#include "TRACE/tracemf.h"
#define TRACE_NAME "mu2esim"

#define TLVL_Constructor TLVL_DEBUG + 2
#define TLVL_Init TLVL_DEBUG + 3
#define TLVL_ReadData TLVL_DEBUG + 4
#define TLVL_ReadData2 TLVL_DEBUG + 5
#define TLVL_WriteData TLVL_DEBUG + 6
#define TLVL_ReadRelease TLVL_DEBUG + 7
#define TLVL_ReadRegister TLVL_DEBUG + 8
#define TLVL_ReadRegister2 TLVL_DEBUG + 9
#define TLVL_WriteRegister TLVL_DEBUG + 10
#define TLVL_WriteRegister2 TLVL_DEBUG + 11
#define TLVL_CFOEmulator TLVL_DEBUG + 12
#define TLVL_CFOEmulator2 TLVL_DEBUG + 13
#define TLVL_DeltaChn TLVL_DEBUG + 14
#define TLVL_ClearBuffer TLVL_DEBUG + 15
#define TLVL_ClearBuffer2 TLVL_DEBUG + 16
#define TLVL_OpenEvent TLVL_DEBUG + 17
#define TLVL_CloseEvent TLVL_DEBUG + 18
#define TLVL_DCSPacketSimulator TLVL_DEBUG + 19
#define TLVL_PacketSimulator TLVL_DEBUG + 20
#define TLVL_EventSimulator TLVL_DEBUG + 21

#define CURRENT_EMULATED_TRACKER_VERSION 1
#define CURRENT_EMULATED_CALORIMETER_VERSION 0
#define CURRENT_EMULATED_CRV_VERSION 0

#include "mu2esim.h"
#include "DTC_Registers.h"

#include <cmath>
#include <vector>

#define THREADED_CFO_EMULATOR 0

mu2esim::mu2esim(std::string ddrFileName)
	: registers_()
	, swIdx_()
	/*, detSimLoopCount_(0)*/
	, dmaData_()
	, ddrFileName_(ddrFileName)
	, ddrFile_(nullptr)
	, mode_(DTCLib::DTC_SimMode_Disabled)
	, cancelCFO_(true)
	, readoutRequestReceived_()
	, event_(nullptr)
	, sub_event_(nullptr)
{
	TLOG(TLVL_Constructor) << "mu2esim::mu2esim BEGIN";
	for (unsigned ii = 0; ii < MU2E_MAX_CHANNELS; ++ii)
		swIdx_[ii] = 0;
	for (unsigned ii = 0; ii < MU2E_MAX_CHANNELS; ++ii)
		hwIdx_[ii] = 0;
	for (unsigned ii = 0; ii < SIM_BUFFCOUNT; ++ii)
	{
		dmaData_[0][ii] = reinterpret_cast<mu2e_databuff_t*>(new char[0x10000]);
		dmaData_[1][ii] = reinterpret_cast<mu2e_databuff_t*>(new char[0x10000]);
	}
	release_all(0);
	release_all(1);

	event_mode_num_tracker_blocks_ = 6;
	auto tracker_count_c = getenv("DTCLIB_NUM_TRACKER_BLOCKS");
	if (tracker_count_c != nullptr)
	{
		event_mode_num_tracker_blocks_ = std::atoi(tracker_count_c);
	}
	// Max is 134
	event_mode_num_calo_blocks_ = 6;
	auto calo_count_c = getenv("DTCLIB_NUM_CALORIMETER_BLOCKS");
	if (calo_count_c != nullptr)
	{
		event_mode_num_calo_blocks_ = std::atoi(calo_count_c);
	}
	event_mode_num_calo_hits_ = 6;
	auto calo_hit_count_c = getenv("DTCLIB_NUM_CALORIMETER_HITS");
	if (calo_hit_count_c != nullptr)
	{
		event_mode_num_calo_hits_ = std::atoi(calo_hit_count_c);
	}
	event_mode_num_crv_blocks_ = 0;
	auto crv_count_c = getenv("DTCLIB_NUM_CRV_BLOCKS");
	if (crv_count_c != nullptr)
	{
		event_mode_num_crv_blocks_ = std::atoi(crv_count_c);
	}

	reopenDDRFile_();

	TLOG(TLVL_Constructor) << "mu2esim::mu2esim END";
}

mu2esim::~mu2esim()
{
	cancelCFO_ = true;
	if (cfoEmulatorThread_.joinable()) cfoEmulatorThread_.join();
	for (unsigned ii = 0; ii < SIM_BUFFCOUNT; ++ii)
	{
		delete[] dmaData_[0][ii];
		delete[] dmaData_[1][ii];
	}
	ddrFile_.reset(nullptr);
}

int mu2esim::init(DTCLib::DTC_SimMode mode)
{
	TLOG(TLVL_Init) << "mu2e Simulator::init";
	mode_ = mode;

	TLOG(TLVL_Init) << "Initializing registers";
	// Set initial register values...
	registers_[DTCLib::CFOandDTC_Register_DesignVersion] = 0x00006363;           // v99.99
	registers_[DTCLib::CFOandDTC_Register_DesignDate] = 0xA5020796;              // A for sim mode, 6-ROC, and date sim functionaliy was last modified
	registers_[DTCLib::CFOandDTC_Register_Control] = 0x00000003;              // System Clock, Timing Enable
	registers_[DTCLib::CFOandDTC_Register_DMATransferLength] = 0x80000010;       // Default value from HWUG
	registers_[DTCLib::CFOandDTC_Register_SERDES_LoopbackEnable] = 0x00000000;    // SERDES Loopback Disabled
	registers_[DTCLib::CFOandDTC_Register_ClockOscillatorStatus] = 0x20002;       // Initialization Complete, no IIC Error
	registers_[DTCLib::DTC_Register_ROCEmulationEnable] = 0x3F;            // ROC Emulators enabled (of course!)
	registers_[DTCLib::CFOandDTC_Register_LinkEnable] = 0x3F3F;                  // All links Tx/Rx enabled, CFO and timing disabled
	registers_[DTCLib::CFOandDTC_Register_SERDES_PLLLocked] = 0x7F;              // SERDES PLL Locked
	registers_[DTCLib::CFOandDTC_Register_SERDES_ResetDone] = 0xFFFFFFFF;        // SERDES Resets Done
	registers_[DTCLib::DTC_Register_SERDES_RXCDRLockStatus] = 0x7F00007F;  // RX CDR Locked
	registers_[DTCLib::DTC_Register_DMATimeoutPreset] = 0x800;             // DMA Timeout Preset
	registers_[DTCLib::DTC_Register_ROCReplyTimeout] = 0x200000;           // ROC Timeout Preset
	registers_[DTCLib::CFOandDTC_Register_SERDESClock_IICBusLow] = 0xFFFFFFFF;
	registers_[DTCLib::CFOandDTC_Register_SERDESClock_IICBusHigh] = 0x77f3f;
	registers_[DTCLib::DTC_Register_DDRReferenceClockFrequency] = 0xbebc200;
	registers_[DTCLib::DTC_Register_DDRClock_IICBusLow] = 0x1074f43b;
	registers_[DTCLib::DTC_Register_DDRClock_IICBusHigh] = 0x30303;
	registers_[DTCLib::DTC_Register_DataPendingTimer] = 0x00002000;  // Data pending timeout preset
	registers_[DTCLib::DTC_Register_EthernetFramePayloadSize] = 0x5D4;
	registers_[DTCLib::DTC_Register_FPGAPROMProgramStatus] = 0x1;

	TLOG(TLVL_Init) << "mu2e Simulator::init finished";
	return 0;
}

/*****************************
   read_data
   returns number of bytes read; negative value indicates an error
   */
int mu2esim::read_data(int chn, void** buffer, int tmo_ms)
{
	auto start = std::chrono::steady_clock::now();
	size_t bytesReturned = 0;
	if (delta_(chn, C2S) == 0) //no data, generate if ddrFile
	{
		TLOG(TLVL_ReadData) << "mu2esim::read_data: Clearing output buffer";
		clearBuffer_(chn, false);

		if (chn == 0)
		{
			if (!ddrFile_->is_open() || ddrFile_->fail())
			{
				TLOG(TLVL_ReadData) << "mu2esim::read_data: Simulating DTC data DMA subevent trasfer";
				uint64_t size = sizeof(header_) + 6*16 + 6*3*16; //subevent header + ROC headers + ROC payloads

				auto ptr = reinterpret_cast<uint8_t*>(dmaData_[chn][swIdx_[chn]]);

				DTC_SubEventHeader header;				
				header.event_tag_low = simDataEWT_++;
				memcpy(ptr, &header_, sizeof(header_));				
				SetEventWindowTag(tag);
				SetEventMode(mode);
				header_.inclusive_subevent_byte_count = data_size;


				TLOG(TLVL_DEBUG + 6) << "Found sub event inclusive byte count as: " <<
					header_.inclusive_subevent_byte_count << " 0x" << 
					std::hex << std::setw(4) << std::setfill('0') << header_.inclusive_subevent_byte_count << ". i.e., " << std::dec << std::setw(0) << 
							(header_.inclusive_subevent_byte_count - sizeof(header_))/16 << " subevent packets.";

				ptr += sizeof(header_); //moving ptr past subevent header

				size_t num_of_packets = (header_.inclusive_subevent_byte_count - sizeof(header_))/16;
				size_t packets_per_roc = num_of_packets/6;
				TLOG(TLVL_DEBUG + 6) << "num_of_packets = " << num_of_packets;
				TLOG(TLVL_DEBUG + 6) << "packets_per_roc = " << packets_per_roc;

				for(int i=0; i>6; ++i)
				{
					size_t packets_this_roc = packets_per_roc;
					if(i == 5) packets_this_roc = num_of_packets;
					if(packets_this_roc > 2023) packets_this_roc = 0; //assume wrapped around negative
					TLOG(TLVL_TRACE) << "Setup ROC-" << i << " data header. packets_this_roc = " << 
						packets_this_roc;

					
					*((uint16_t *)(&(ptr[1*2]))) = 0x8000 | (i << 8) | (5<<4); //packet type Data Header 0x5
					*((uint16_t *)(&(ptr[2*2]))) = packets_this_roc; //packet count
					*((uint16_t *)(&(ptr[3*2]))) = tag.GetEventWindowTag(true); //packet count
					*((uint16_t *)(&(ptr[4*2]))) = tag.GetEventWindowTag(true)>>16; //packet count
					*((uint16_t *)(&(ptr[5*2]))) = tag.GetEventWindowTag(true)>>32; //packet count
				
					ptr += 16*1 + 16*packets_this_roc; //move ptr past ROC header + data
				} //end ROC loop

			} //end generate sim data 
			else 
			{
				TLOG(TLVL_ReadData) << "mu2esim::read_data: Reading size from memory file";
				uint64_t size;
				ddrFile_->read(reinterpret_cast<char*>(&size), sizeof(uint64_t) / sizeof(char));

				TLOG(TLVL_ReadData) << "mu2esim::read_data: Size is " << size;

				if (ddrFile_->eof() || size == 0)
				{
					TLOG(TLVL_ReadData) << "mu2esim::read_data: End of file reached, looping back to start";
					ddrFile_->clear();
					ddrFile_->seekg(std::ios::beg);

					TLOG(TLVL_ReadData) << " mu2esim::read_data: Re-reading size from memory file";
					ddrFile_->read(reinterpret_cast<char*>(&size), sizeof(uint64_t));
					TLOG(TLVL_ReadData) << "mu2esim::read_data: Size is " << size;
					if (ddrFile_->eof())
					{
						TLOG(TLVL_ReadData) << "mu2esim::read_data: 0-size file detected!";
						return -1;
					}
				}
				TLOG(TLVL_ReadData) << "Size of data is " << size - sizeof(uint64_t) << ", reading into buffer " << swIdx_[chn] << ", at "
								<< (void*)dmaData_[chn][swIdx_[chn]];
				memcpy(dmaData_[chn][swIdx_[chn]], &size, sizeof(uint64_t));
				ddrFile_->read(reinterpret_cast<char*>(dmaData_[chn][swIdx_[chn]]) + sizeof(uint64_t), size - sizeof(uint64_t));
				bytesReturned = size;
			} //end DDR File sim read			
		}
		else if (chn == 1)
		{
			// Data should already be in appropriate buffer, generated by dcsPacketSimulator_()			
		}
	}
	else //there is data to read, calculate size
	if (chn == 1)
	{
		uint64_t dcsSize = *reinterpret_cast<uint64_t*>(dmaData_[chn][swIdx_[chn]]);
		TLOG(TLVL_ReadData2) << "mu2esim::read_data: dcsSize = " << dcsSize;
		bytesReturned = (size_t)dcsSize;
	}

	*buffer = dmaData_[chn][swIdx_[chn]];
	TLOG(TLVL_ReadData2) << "mu2esim::read_data: *buffer (" << (void*)*buffer << ") should now be equal to dmaData_[" << chn << "]["
						 << swIdx_[chn] << "] (" << (void*)dmaData_[chn][swIdx_[chn]] << "), bytesReturned = " << bytesReturned <<
						 " data = 0x" << std::hex << *reinterpret_cast<uint64_t*>(dmaData_[chn][swIdx_[chn]]);
	swIdx_[chn] = (swIdx_[chn] + 1) % SIM_BUFFCOUNT;

	auto duration =
		std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
	TLOG(TLVL_ReadData2) << "mu2esim::read_data took " << duration << " milliseconds out of tmo_ms=" << tmo_ms;
	return static_cast<int>(bytesReturned);
}

int mu2esim::write_data(int chn, void* buffer, size_t bytes)
{
	if (chn == 0)
	{
		TLOG(TLVL_WriteData) << "mu2esim::write_data: adding buffer to simulated DDR memory sz=" << bytes
							 << ", *buffer=" << *((uint64_t*)buffer);
		if (bytes <= sizeof(mu2e_databuff_t))
		{
			if (event_) closeEvent_();

			// Strip off first 64-bit word
			auto writeBytes = *reinterpret_cast<uint64_t*>(buffer) - sizeof(uint64_t);
			auto ptr = reinterpret_cast<char*>(buffer) + (sizeof(uint64_t) / sizeof(char));
			ddrFile_->write(ptr, writeBytes);
			registers_[DTCLib::DTC_Register_DetEmulation_DataEndAddress] += static_cast<uint32_t>(writeBytes);
			ddrFile_->flush();
			return 0;
		}
		TLOG(TLVL_WriteData) << "mu2esim::write_data: I was asked to write more than one buffer's worth of data. Cowardly refusing.";
	}
	else if (chn == 1)
	{
		TLOG(TLVL_WriteData) << "mu2esim::write_data start: chn=" << chn << ", buf=" << buffer << ", bytes=" << bytes;

		//extract target ROC link "activeDAQLink" from bufer
		uint32_t worda;
		memcpy(&worda, reinterpret_cast<uint64_t*>(buffer) + 1, sizeof worda);
		auto word = static_cast<uint16_t>(worda >> 16);
		TLOG(TLVL_WriteData) << "mu2esim::write_data worda is 0x" << std::hex << worda << " and word is 0x" << std::hex << word;
		auto activeLink = static_cast<DTCLib::DTC_Link_ID>((word & 0x0F00) >> 8);

		DTCLib::DTC_EventWindowTag ts(reinterpret_cast<uint8_t*>(buffer) + 6);
		if ((word & 0x8010) == 0x8010)
		{
			TLOG(TLVL_WriteData) << "mu2esim::write_data: Readout Request: activeDAQLink=" << activeLink
								 << ", ts=" << ts.GetEventWindowTag(true);
			readoutRequestReceived_[ts.GetEventWindowTag(true)][activeLink] = true;
		}
		else if ((word & 0x8020) == 0x8020)
		{
			TLOG(TLVL_WriteData) << "mu2esim::write_data: Data Request: activeDAQLink=" << activeLink << ", ts=" << ts.GetEventWindowTag(true);
			if (activeLink != DTCLib::DTC_Link_Unused)
			{
				if (!readoutRequestReceived_[ts.GetEventWindowTag(true)][activeLink])
				{
					TLOG(TLVL_WriteData) << "mu2esim::write_data: Data Request Received but missing Readout Request!";
				}
				else
				{
					openEvent_(ts);

					if (mode_ == DTCLib::DTC_SimMode_Performance || mode_ == DTCLib::DTC_SimMode_Timeout)
					{
						auto packetCount = *(reinterpret_cast<uint16_t*>(buffer) + 7);
						packetSimulator_(ts, activeLink, packetCount);
					}
					else if (mode_ == DTCLib::DTC_SimMode_Tracker)
					{
						trackerBlockSimulator_(ts, activeLink, 0);
					}
					else if (mode_ == DTCLib::DTC_SimMode_Calorimeter)
					{
						calorimeterBlockSimulator_(ts, activeLink, 0);
					}
					else if (mode_ == DTCLib::DTC_SimMode_CosmicVeto)
					{
						crvBlockSimulator_(ts, activeLink, 0);
					}

					readoutRequestReceived_[ts.GetEventWindowTag(true)][activeLink] = false;
					if (readoutRequestReceived_[ts.GetEventWindowTag(true)].count() == 0)
					{
						closeEvent_();
						readoutRequestReceived_.erase(ts.GetEventWindowTag(true));
					}
				}
			}
		}
		if ((word & 0x0080) == 0)
		{
			TLOG(TLVL_WriteData) << "mu2esim::write_data activeDCSLink is " << activeLink;
			if (activeLink != DTCLib::DTC_Link_Unused)
			{
				DTCLib::DTC_DataPacket packet(reinterpret_cast<uint64_t*>(buffer) + 1); //skip 64-bit packet size
				TLOG(TLVL_WriteData) << "packet " << packet.toJSON();


				DTCLib::DTC_DCSRequestPacket thisPacket(packet);


				if(thisPacket.GetType() == DTCLib::DTC_DCSOperationType_Write || 
					thisPacket.GetType() == DTCLib::DTC_DCSOperationType_BlockWrite)
				{
					TLOG(TLVL_WriteData) << "mu2esim::write_data: Recieved DCS Write Request:" << thisPacket.toJSON();

					auto request1 = thisPacket.GetRequest(false);
					rocRegisters_[activeLink][request1.first] = request1.second;

					if(thisPacket.GetType() == DTCLib::DTC_DCSOperationType_BlockWrite)
					{
						auto request2 = thisPacket.GetRequest(true);
						rocRegisters_[activeLink][request2.first] = request2.second;
					}
				}

				//Note: write's can request ack
				if (thisPacket.GetType() == DTCLib::DTC_DCSOperationType_Read ||
					thisPacket.GetType() == DTCLib::DTC_DCSOperationType_BlockRead || thisPacket.RequestsAck())
				{
					TLOG(TLVL_WriteData) << "mu2esim::write_data: Recieved DCS Request:" << thisPacket.toJSON();					
					dcsPacketSimulator_(thisPacket, activeLink);
				}
				
			
			}
		}
	}

	return 0;
}

int mu2esim::read_release(int chn, unsigned num)
{
	// Always succeeds
	TLOG(TLVL_ReadRelease) << "mu2esim::read_release: Simulating a release of " << num << "u buffers of channel " << chn;
	for (unsigned ii = 0; ii < num; ++ii)
	{
		if (delta_(chn, C2S) != 0) swIdx_[chn] = (swIdx_[chn] + 1) % SIM_BUFFCOUNT;
	}
	TLOG(TLVL_DeltaChn) << "mu2esim::read_release: After release... chn=" << chn << 
		" hw=" << hwIdx_[chn] << " sw=" << swIdx_[chn];
	return 0;
}

int mu2esim::release_all(int chn)
{
	read_release(chn, SIM_BUFFCOUNT);
	// swIdx_[chn] = 0; //do not reset, just release up to hw pointer
	return 0;
}

int mu2esim::read_register(uint16_t address, int tmo_ms, uint32_t* output)
{
	auto start = std::chrono::steady_clock::now();
	*output = 0;
	if (registers_.count(address) > 0)
	{
		TLOG(TLVL_ReadRegister) << "mu2esim::read_register: Returning value 0x" << std::hex << registers_[address] << " for address 0x"
								<< std::hex << address;
		*output = registers_[address];
	}
	auto duration =
		std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
	TLOG(TLVL_ReadRegister2) << "mu2esim::read_register took " << duration << " milliseconds out of tmo_ms=" << tmo_ms;
	return 0;
}

int mu2esim::write_register(uint16_t address, int tmo_ms, uint32_t data)
{
	auto start = std::chrono::steady_clock::now();
	// Write the register!!!
	TLOG(TLVL_WriteRegister) << "mu2esim::write_register: Writing value 0x" << std::hex << data << " into address 0x" << std::hex
							 << address;
	registers_[address] = data;
	auto duration =
		std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
	TLOG(TLVL_WriteRegister2) << "mu2esim::write_register took " << duration << " milliseconds out of tmo_ms=" << tmo_ms;
	std::bitset<32> dataBS(data);
	if (address == DTCLib::CFOandDTC_Register_Control)
	{
		auto detectorEmulationMode = (registers_[DTCLib::DTC_Register_DetEmulation_Control0] & 0x3) != 0;
		if (dataBS[30] == 1 && !detectorEmulationMode)
		{
			TLOG(TLVL_WriteRegister2) << "mu2esim::write_register: CFO Emulator Enable Detected!";
			cancelCFO_ = true;
			if (cfoEmulatorThread_.joinable()) cfoEmulatorThread_.join();
			cancelCFO_ = false;
#if THREADED_CFO_EMULATOR
			if (registers_[0x91AC] > 10)
			{
				cfoEmulatorThread_ = std::thread(&mu2esim::CFOEmulator_, this);
			}
			else
#endif
			{
				CFOEmulator_();
			}
		}
		else if (dataBS[26] == 1 && detectorEmulationMode)
		{
			TLOG(TLVL_WriteRegister2) << "mu2esim::write_register: IGNORING CFO Emulator Enable because we're in Detector Simulator mode!";
		}
		if (dataBS[31] == 1)
		{
			TLOG(TLVL_WriteRegister2) << "mu2esim::write_register: RESETTING DTC EMULATOR!";
			init(mode_);
			reopenDDRFile_();
		}
	}
	if (address == DTCLib::DTC_Register_DetEmulation_Control0)
	{
		if (dataBS[0] == 0)
		{
			reopenDDRFile_();
		}
	}
	if (address == DTCLib::DTC_Register_DetEmulation_DataStartAddress)
	{
		ddrFile_->seekg(data);
	}
	return 0;
}

void mu2esim::CFOEmulator_()
{
	if (cancelCFO_)
	{
		std::bitset<32> ctrlReg(registers_[0x9100]);
		ctrlReg[30] = 0;
		registers_[0x9100] = ctrlReg.to_ulong();
		return;
	}
	DTCLib::DTC_EventWindowTag start(registers_[DTCLib::DTC_Register_CFOEmulation_TimestampLow],
									 static_cast<uint16_t>(registers_[DTCLib::DTC_Register_CFOEmulation_TimestampHigh]));
	auto count = registers_[DTCLib::DTC_Register_CFOEmulation_NumHeartbeats];
	auto ticksToWait = static_cast<long long>(registers_[DTCLib::DTC_Register_CFOEmulation_HeartbeatInterval] * 0.0064);
	TLOG(TLVL_CFOEmulator) << "mu2esim::CFOEmulator_ start timestamp=" << start.GetEventWindowTag(true) << ", count=" << count
						   << ", delayBetween=" << ticksToWait;
	bool linkEnabled[6];
	for (auto link : DTCLib::DTC_ROC_Links)
	{
		std::bitset<32> linkRocs(registers_[DTCLib::CFOandDTC_Register_LinkEnable]);
		auto number = linkRocs[link] + linkRocs[link + 8];
		TLOG(TLVL_CFOEmulator) << "mu2esim::CFOEmulator_ linkRocs[" << static_cast<int>(link) << "]=" << number;
		linkEnabled[link] = number != 0;
	}

	unsigned sentCount = 0;
	while (sentCount < count && !cancelCFO_)
	{
		if (mode_ == DTCLib::DTC_SimMode_Event)
		{
			TLOG(TLVL_CFOEmulator) << "Event mode enabled, calling eventSimulator_";
			eventSimulator_(start + sentCount);
		}
		else
		{
			openEvent_(start + sentCount);
			for (auto link : DTCLib::DTC_ROC_Links)
			{
				uint16_t packetCount;
				switch (link)
				{
					case DTCLib::DTC_Link_0:
						packetCount = static_cast<uint16_t>(registers_[DTCLib::DTC_Register_ROCEmulation_NumPacketsLinks10]);
						break;
					case DTCLib::DTC_Link_1:
						packetCount = static_cast<uint16_t>(registers_[DTCLib::DTC_Register_ROCEmulation_NumPacketsLinks10] >> 16);
						break;
					case DTCLib::DTC_Link_2:
						packetCount = static_cast<uint16_t>(registers_[DTCLib::DTC_Register_ROCEmulation_NumPacketsLinks32]);
						break;
					case DTCLib::DTC_Link_3:
						packetCount = static_cast<uint16_t>(registers_[DTCLib::DTC_Register_ROCEmulation_NumPacketsLinks32] >> 16);
						break;
					case DTCLib::DTC_Link_4:
						packetCount = static_cast<uint16_t>(registers_[DTCLib::DTC_Register_ROCEmulation_NumPacketsLinks54]);
						break;
					case DTCLib::DTC_Link_5:
						packetCount = static_cast<uint16_t>(registers_[DTCLib::DTC_Register_ROCEmulation_NumPacketsLinks54] >> 16);
						break;
					default:
						packetCount = 0;
						break;
				}
				if (linkEnabled[link] != 0)
				{
					TLOG(TLVL_CFOEmulator) << "mu2esim::CFOEmulator_ linkRocs[" << static_cast<int>(link) << "]=" << linkEnabled[link];
					if (mode_ == DTCLib::DTC_SimMode_Performance || mode_ == DTCLib::DTC_SimMode_Timeout)
					{
						TLOG(TLVL_CFOEmulator) << "Performance or Timeout mode enabled, calling packetSimulator_";
						packetSimulator_(start + sentCount, link, packetCount);
					}
					else if (mode_ == DTCLib::DTC_SimMode_Tracker)
					{
						TLOG(TLVL_CFOEmulator) << "Tracker mode enabled, calling trackerBlockSimulator_";
						trackerBlockSimulator_(start + sentCount, link, 0);
					}
					else if (mode_ == DTCLib::DTC_SimMode_Calorimeter)
					{
						TLOG(TLVL_CFOEmulator) << "Calorimeter mode enabled, calling calorimeterBlockSimulator_";
						calorimeterBlockSimulator_(start + sentCount, link, 0);
					}
					else if (mode_ == DTCLib::DTC_SimMode_CosmicVeto)
					{
						TLOG(TLVL_CFOEmulator) << "CRV mode enabled, calling crvBlockSimulator_";
						crvBlockSimulator_(start + sentCount, link, 0);
					}
				}
			}
			closeEvent_();
		}

		if (ticksToWait > 100)
		{
			usleep(ticksToWait);
		}
		sentCount++;
	}
	std::bitset<32> ctrlReg(registers_[0x9100]);
	ctrlReg[30] = 0;
	registers_[0x9100] = ctrlReg.to_ulong();
}

unsigned mu2esim::delta_(int chn, int dir)
{
	if (chn == 0) return 0;
	TLOG(TLVL_DeltaChn) << "delta_ " << chn << " " << dir << " = 0";
	unsigned hw = hwIdx_[chn];
	unsigned sw = swIdx_[chn];
	TLOG(TLVL_DeltaChn) << "mu2esim::delta_ chn=" << chn << " dir=" << dir << " hw=" << hw << " sw=" << sw
						<< " num_buffs=" << SIM_BUFFCOUNT;
	if (dir == C2S)
		return ((hw >= sw) ? hw - sw : SIM_BUFFCOUNT + hw - sw);
	else
		return ((sw >= hw) ? SIM_BUFFCOUNT - (sw - hw) : hw - sw);
}

void mu2esim::clearBuffer_(int chn, bool increment)
{
	TLOG(TLVL_ClearBuffer2) << "mu2esim::clearBuffer_(" << chn << ", " << increment << "): NOP";
}

void mu2esim::openEvent_(DTCLib::DTC_EventWindowTag ts)
{
	TLOG(TLVL_OpenEvent) << "mu2esim::openEvent_ Checking timestamp " << ts.GetEventWindowTag(true) << " vs current timestamp "
						 << (event_ ? std::to_string(event_->GetEventWindowTag().GetEventWindowTag(true)) : std::string(" NONE"));
	if (event_ && ts == event_->GetEventWindowTag()) return;
	if (event_) closeEvent_();

	TLOG(TLVL_OpenEvent) << "mu2esim::openEvent_: Setting up initial buffer";

	event_ = std::make_unique<DTCLib::DTC_Event>();
	event_->SetEventWindowTag(ts);
	event_->SetEventMode(getEventMode_());

	sub_event_ = std::make_unique<DTCLib::DTC_SubEvent>();
	sub_event_->SetEventWindowTag(ts);
	sub_event_->SetEventMode(getEventMode_());
}

void mu2esim::closeSubEvent_()
{
	if (!event_ || !sub_event_ || sub_event_->GetDataBlockCount() == 0) return;

	event_->AddSubEvent(*(sub_event_.release()));

	sub_event_ = std::make_unique<DTCLib::DTC_SubEvent>();
	sub_event_->SetEventWindowTag(event_->GetEventWindowTag());
	sub_event_->SetEventMode(getEventMode_());
}

DTCLib::DTC_EventMode mu2esim::getEventMode_()
{
	DTCLib::DTC_EventMode event_mode;

	event_mode.mode0 = static_cast<uint8_t>(registers_[DTCLib::DTC_Register_CFOEmulation_EventMode1] & 0xFF);
	event_mode.mode1 = static_cast<uint8_t>((registers_[DTCLib::DTC_Register_CFOEmulation_EventMode1] & 0xFF00) >> 8);
	event_mode.mode2 = static_cast<uint8_t>((registers_[DTCLib::DTC_Register_CFOEmulation_EventMode1] & 0xFF0000) >> 16);
	event_mode.mode3 = static_cast<uint8_t>((registers_[DTCLib::DTC_Register_CFOEmulation_EventMode1] & 0xFF000000) >> 24);
	event_mode.mode4 = static_cast<uint8_t>(registers_[DTCLib::DTC_Register_CFOEmulation_EventMode2] & 0xFF);

	return event_mode;
}

void mu2esim::closeEvent_()
{
	TLOG(TLVL_CloseEvent) << "mu2esim::closeEvent_: Checking current event";
	if (sub_event_) closeSubEvent_();
	if (event_ && ddrFile_)
	{
		event_->WriteEvent(*ddrFile_, false);
		ddrFile_->flush();

		event_.reset(nullptr);
		sub_event_.reset(nullptr);
	}
	TLOG(TLVL_CloseEvent) << "mu2esim::closeEvent_ FINISH";
}

void mu2esim::dcsPacketSimulator_(DTCLib::DTC_DCSRequestPacket in, DTCLib::DTC_Link_ID rocLink)
{
	auto packetCount = 0;
	if (in.GetType() == DTCLib::DTC_DCSOperationType_BlockRead)
	{
		packetCount = 1;
	}
	TLOG(TLVL_DCSPacketSimulator) << "mu2esim::dcsPacketSimulator_: Constructing DCS Response";
	DTCLib::DTC_DMAPacket packet(DTCLib::DTC_PacketType_DCSReply, in.GetLinkID(), (1 + packetCount) * 16, true);

	TLOG(TLVL_DCSPacketSimulator) << "mu2esim::dcsPacketSimulator_: copying response into new buffer";
	auto dataPacket = packet.ConvertToDataPacket();

	dataPacket.SetByte(4, static_cast<int>(in.GetType()) + (in.RequestsAck() ? 0x8 : 0) + (in.IsDoubleOp() ? 0x4 : 0) +
							  ((packetCount & 0x2) << 6));
	dataPacket.SetByte(5, (packetCount & 0x3FC) >> 2);

	auto request1 = in.GetRequest(false);
	dataPacket.SetByte(6, request1.first & 0xFF);
	dataPacket.SetByte(7, (request1.first & 0xFF00) >> 8);
	dataPacket.SetByte(8, rocRegisters_[rocLink][request1.first] & 0xFF);
	dataPacket.SetByte(9, (rocRegisters_[rocLink][request1.first] & 0xFF00) >> 8);

	if (in.GetType() != DTCLib::DTC_DCSOperationType_BlockRead)
	{
		auto request2 = in.GetRequest(true);
		dataPacket.SetByte(10, request2.first & 0xFF);
		dataPacket.SetByte(11, (request2.first & 0xFF00) >> 8);
		dataPacket.SetByte(12, rocRegisters_[rocLink][request2.first] & 0xFF);
		dataPacket.SetByte(13, (rocRegisters_[rocLink][request2.first] & 0xFF00) >> 8);
	}
	else
	{
		for (int ii = 10; ii < dataPacket.GetSize(); ++ii)
		{
			dataPacket.SetByte(ii, ii);
		}
	}

	TLOG(TLVL_DCSPacketSimulator) << "in.toJSON() " << in.toJSON();
	TLOG(TLVL_DCSPacketSimulator) << "dataPacket.toJSON() " << dataPacket.toJSON();

	size_t packetSize = dataPacket.GetSize();
	TLOG(TLVL_DCSPacketSimulator) << "packetSize = " << packetSize <<
		" into buffer hw=" << hwIdx_[1] << " sw=" << swIdx_[1];
	*reinterpret_cast<uint64_t*>(dmaData_[1][hwIdx_[1]]) = packetSize;
	memcpy(reinterpret_cast<uint64_t*>(dmaData_[1][hwIdx_[1]]) + 1, dataPacket.GetData(), packetSize);
	TLOG(TLVL_DCSPacketSimulator) << "mu2esim::dcsPacketSimulator_: copied packetSize=" << packetSize <<
		" into buffer #" << hwIdx_[1] << " data = 0x" << std::hex << *reinterpret_cast<uint64_t*>(dmaData_[1][hwIdx_[1]]);
	hwIdx_[1] = (hwIdx_[1] + 1) % SIM_BUFFCOUNT;

	TLOG(TLVL_DCSPacketSimulator) << "After... hw=" << hwIdx_[1] << " sw=" << swIdx_[1];
}

void mu2esim::eventSimulator_(DTCLib::DTC_EventWindowTag ts)
{
	openEvent_(ts);

	auto num_trk_dtcs = ceil(event_mode_num_tracker_blocks_ / 6);
	auto num_calo_dtcs = ceil(event_mode_num_calo_blocks_ / 6);
	auto num_crv_dtcs = ceil(event_mode_num_crv_blocks_ / 6);

	size_t generated_trk_blocks = 0;
	size_t generated_calo_blocks = 0;
	size_t generated_crv_blocks = 0;

	int DTCID = 0;

	for (auto ii = 0; ii < num_trk_dtcs; ++ii, ++DTCID)
	{
		TLOG(TLVL_EventSimulator) << "Generating Tracker data, DTCID " << DTCID << ", TRK DTC " << ii + 1 << "/" << num_trk_dtcs;
		sub_event_->SetDTCMAC( DTCID);
		sub_event_->SetSourceDTC(DTCID, DTCLib::DTC_Subsystem_Tracker);

		for (auto link : DTCLib::DTC_ROC_Links)
		{
			TLOG(TLVL_EventSimulator) << "Generating Tracker data block for DTC " << DTCID << " link " << static_cast<int>(link);
			trackerBlockSimulator_(ts, link, ii);
			if (++generated_trk_blocks >= event_mode_num_tracker_blocks_) break;
		}
		closeSubEvent_();
	}
	for (auto ii = 0; ii < num_calo_dtcs; ++ii, ++DTCID)
	{
		TLOG(TLVL_EventSimulator) << "Generating Calorimeter data, DTCID " << DTCID << ", Calo DTC " << ii + 1 << "/" << num_calo_dtcs;
		sub_event_->SetDTCMAC( DTCID);
		sub_event_->SetSourceDTC(DTCID, DTCLib::DTC_Subsystem_Calorimeter);

		for (auto link : DTCLib::DTC_ROC_Links)
		{
			TLOG(TLVL_EventSimulator) << "Generating Calorimeter data block for DTC " << DTCID << " link " << static_cast<int>(link);
			calorimeterBlockSimulator_(ts, link, ii);
			if (++generated_calo_blocks >= event_mode_num_calo_blocks_) break;
		}
		closeSubEvent_();
	}
	for (auto ii = 0; ii < num_crv_dtcs; ++ii, ++DTCID)
	{
		TLOG(TLVL_EventSimulator) << "Generating CRV data, DTCID " << DTCID << ", CRV DTC " << ii + 1 << "/" << num_crv_dtcs;
		sub_event_->SetDTCMAC( DTCID);
		sub_event_->SetSourceDTC(DTCID, DTCLib::DTC_Subsystem_CRV);

		for (auto link : DTCLib::DTC_ROC_Links)
		{
			TLOG(TLVL_EventSimulator) << "Generating CRV data block for DTC " << DTCID << " link " << static_cast<int>(link);
			crvBlockSimulator_(ts, link, ii);
			if (++generated_crv_blocks >= event_mode_num_crv_blocks_) break;
		}
		closeSubEvent_();
	}

	closeEvent_();
}

void mu2esim::trackerBlockSimulator_(DTCLib::DTC_EventWindowTag ts, DTCLib::DTC_Link_ID link, int DTCID)
{
	uint16_t buffer[24];

	size_t nPackets = 2;
	DTCLib::DTC_DataHeaderPacket header(link, nPackets, DTCLib::DTC_DataStatus_Valid, DTCID, DTCLib::DTC_Subsystem_Tracker, CURRENT_EMULATED_TRACKER_VERSION, ts, static_cast<uint8_t>(registers_[DTCLib::DTC_Register_CFOEmulation_EventMode1] & 0xFF));
	memcpy(&buffer[0], header.ConvertToDataPacket().GetData(), 16);

	uint16_t strawID = ((static_cast<int>(link) + (DTCID * 6)) << 7) + 1;
	buffer[8] = strawID;
	// TDC set to EventWindowTag
	buffer[9] = (ts.GetEventWindowTag(true) & 0xFFFF);
	buffer[10] = 0x1000 + 0xF00 + ((ts.GetEventWindowTag(true) & 0xFF0000) >> 16);

	buffer[11] = (ts.GetEventWindowTag(true) & 0xFFFF);
	buffer[12] = 0xF00 + ((ts.GetEventWindowTag(true) & 0xFF0000) >> 16);

	buffer[13] = (0x7 << 6) + 0x1;  // PMP = 7, 1 ADC packet

	buffer[14] = 0x1 + (0x2 << 10);
	buffer[15] = 0x3 << 4;

	buffer[16] = 0x4 + (0x5 << 10);
	buffer[17] = 0x6 << 4;
	buffer[18] = 0x7 + (0x8 << 10);
	buffer[19] = 0x9 << 4;
	buffer[20] = 0xA + (0xB << 10);
	buffer[21] = 0xC << 4;
	buffer[22] = 0xD + (0xE << 10);
	buffer[23] = 0xF << 4;

	DTCLib::DTC_DataBlock block(sizeof(buffer));
	memcpy(&(*block.allocBytes)[0], buffer, sizeof(buffer));
	sub_event_->AddDataBlock(block);
}

void mu2esim::calorimeterBlockSimulator_(DTCLib::DTC_EventWindowTag ts, DTCLib::DTC_Link_ID link, int DTCID)
{
	std::vector<uint16_t> buffer(9);

	// Index Packet
	buffer[8] = event_mode_num_calo_hits_;

	for (size_t idx = 0; idx < event_mode_num_calo_hits_; ++idx)
	{
		buffer.push_back(2 /* num hits */ + 2 * event_mode_num_calo_hits_ /* hit offsets */ + 4 /* board ID */ + idx * 24 /* hit readout */);  // Index of hit from start of index packet
	}

	// Board ID
	uint16_t roc_id = static_cast<uint8_t>(link) + (DTCID * 6);
	uint8_t board_number = (roc_id)&0x3F;  // 6 bits
	buffer.push_back(0xFC00 + (board_number << 3));
	buffer.push_back(0x3FFF);

	auto t0Step = 2000 / event_mode_num_calo_hits_;

	for (size_t idx = 0; idx < event_mode_num_calo_hits_; ++idx)
	{
		// Hit readout
		buffer.push_back(board_number);
		buffer.push_back(10 * roc_id);  // DIRAC B...do we need to put something here?

		buffer.push_back(0);                           // No error flags
		buffer.push_back((idx * t0Step) & 0xFFFF);  // Time
		buffer.push_back(0x0607);                      // Max sample/num samples

		// Digitizer samples
		buffer.push_back(0x1111);
		buffer.push_back(0x2222);
		buffer.push_back(0x3333);
		buffer.push_back(0x4444);
		buffer.push_back(0x5555);
		buffer.push_back(0x6666);
		buffer.push_back(0x7777);
	}

	while (buffer.size() % 8 != 0) {
		buffer.push_back(0);
	}

	size_t nPackets = (buffer.size() / 8) - 1;
	DTCLib::DTC_DataHeaderPacket header(link, nPackets, DTCLib::DTC_DataStatus_Valid, DTCID, DTCLib::DTC_Subsystem_Calorimeter, CURRENT_EMULATED_CALORIMETER_VERSION, ts, static_cast<uint8_t>(registers_[DTCLib::DTC_Register_CFOEmulation_EventMode1] & 0xFF));
	memcpy(&buffer[0], header.ConvertToDataPacket().GetData(), 16);

	DTCLib::DTC_DataBlock block(buffer.size() * sizeof(uint16_t));
	memcpy(&(*block.allocBytes)[0], &buffer[0], buffer.size() * sizeof(uint16_t));
	sub_event_->AddDataBlock(block);
}

void mu2esim::crvBlockSimulator_(DTCLib::DTC_EventWindowTag ts, DTCLib::DTC_Link_ID link, int DTCID)
{
	uint16_t buffer[24];

	size_t nPackets = 2;
	DTCLib::DTC_DataHeaderPacket header(link, nPackets, DTCLib::DTC_DataStatus_Valid, DTCID, DTCLib::DTC_Subsystem_CRV, CURRENT_EMULATED_CRV_VERSION, ts, static_cast<uint8_t>(registers_[DTCLib::DTC_Register_CFOEmulation_EventMode1] & 0xFF));
	memcpy(&buffer[0], header.ConvertToDataPacket().GetData(), 16);

	// ROC Status packet
	uint8_t board_number = static_cast<uint8_t>(link) + (DTCID * 6);
	buffer[8] = 0x60 + (board_number << 8);
	buffer[9] = 28;
	buffer[10] = 0;
	buffer[11] = 1;
	buffer[12] = 0;
	buffer[13] = 1;
	buffer[14] = 0;
	buffer[15] = static_cast<uint8_t>(registers_[DTCLib::DTC_Register_CFOEmulation_EventMode1] & 0xFF) << 8;

	// Hit Readout
	buffer[16] = 0;
	buffer[17] = 0;
	buffer[18] = 0x2211;
	buffer[19] = 0x4433;
	buffer[20] = 0x6655;
	buffer[21] = 0x8877;
	buffer[22] = 0;  // Zero padding
	buffer[23] = 0;

	DTCLib::DTC_DataBlock block(sizeof(buffer));
	memcpy(&(*block.allocBytes)[0], buffer, sizeof(buffer));
	sub_event_->AddDataBlock(block);
}

void mu2esim::reopenDDRFile_()
{
	if (!ddrFile_)
	{
		TLOG(TLVL_INFO) << "Going to open simulated RAM file " << ddrFileName_;
		ddrFile_.reset(new std::fstream(ddrFileName_, std::fstream::binary | std::fstream::in | std::fstream::out));
	}
	else
	{
		ddrFile_.reset(new std::fstream(ddrFileName_, std::fstream::trunc | std::fstream::binary | std::fstream::out | std::ios::in));
	}

	if (!ddrFile_->is_open() || ddrFile_->fail())
	{
		TLOG(TLVL_INFO) << "File " << ddrFileName_ << " does not exist, creating";
		ddrFile_.reset(new std::fstream(ddrFileName_, std::fstream::binary | std::fstream::trunc | std::fstream::out));
		// Reopen with original flags
		ddrFile_.reset(new std::fstream(ddrFileName_, std::fstream::binary | std::fstream::in | std::fstream::out));
	}
}

void mu2esim::packetSimulator_(DTCLib::DTC_EventWindowTag ts, DTCLib::DTC_Link_ID link, uint16_t packetCount)
{
	if (!sub_event_) return;

	TLOG(TLVL_PacketSimulator) << "mu2esim::packetSimulator_: Generating data for timestamp " << ts.GetEventWindowTag(true);

	if (mode_ == DTCLib::DTC_SimMode_Performance)
	{
		std::vector<uint8_t> packet(16 + packetCount * 16);

		// Add a Data Header packet to the reply
		packet[0] = static_cast<uint8_t>(packet.size());
		packet[1] = static_cast<uint8_t>(packet.size() >> 8);
		packet[2] = 0x50;
		packet[3] = 0x80 + (link & 0x0F);
		packet[4] = static_cast<uint8_t>(packetCount & 0xFF);
		packet[5] = static_cast<uint8_t>(packetCount >> 8);
		ts.GetEventWindowTag(&packet[0], 6);
		packet[12] = 0;
		packet[13] = 0;
		packet[14] = 0;
		packet[15] = 0;

		size_t offset = 16;

		for (uint16_t ii = 0; ii < packetCount; ++ii)
		{
			packet[offset] = static_cast<uint8_t>(ii);
			packet[offset + 1] = 0x11;
			packet[offset + 2] = 0x22;
			packet[offset + 3] = 0x33;
			packet[offset + 4] = 0x44;
			packet[offset + 5] = 0x55;
			packet[offset + 6] = 0x66;
			packet[offset + 7] = 0x77;
			packet[offset + 8] = 0x88;
			packet[offset + 9] = 0x99;
			packet[offset + 10] = 0xaa;
			packet[offset + 11] = 0xbb;
			packet[offset + 12] = 0xcc;
			packet[offset + 13] = 0xdd;
			packet[offset + 14] = 0xee;
			packet[offset + 15] = 0xff;

			offset += 16;
		}

		DTCLib::DTC_DataBlock block(packet.size());
		memcpy(&(*block.allocBytes)[0], &packet[0], packet.size());
		sub_event_->AddDataBlock(block);
	}
	else if (mode_ == DTCLib::DTC_SimMode_Timeout)
	{
		uint8_t packet[16];

		packet[0] = 0xfe;
		packet[1] = 0xca;
		packet[2] = 0xfe;
		packet[3] = 0xca;
		packet[4] = 0xfe;
		packet[5] = 0xca;
		packet[6] = 0xfe;
		packet[7] = 0xca;
		packet[8] = 0xfe;
		packet[9] = 0xca;
		packet[10] = 0xfe;
		packet[11] = 0xca;
		packet[12] = 0xfe;
		packet[13] = 0xca;
		packet[14] = 0xfe;
		packet[15] = 0xca;

		TLOG(TLVL_PacketSimulator) << "mu2esim::packetSimulator_ Writing Data Header packet to memory file, chn=0, packet=" << (void*)packet;

		DTCLib::DTC_DataBlock block(sizeof(packet));
		memcpy(&(*block.allocBytes)[0], packet, sizeof(packet));
		sub_event_->AddDataBlock(block);
	}

	ddrFile_->flush();
}
