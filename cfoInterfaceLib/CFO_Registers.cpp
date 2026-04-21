#include "cfoInterfaceLib/CFO_Registers.h"

#include <assert.h>
#include <unistd.h>
#include <chrono>
#include <cmath>
#include <iomanip>  // std::setw, std::setfill
#include <sstream>  // Convert uint to hex stLink

#include "artdaq-core/Utilities/ExceptionHandler.hh"    /*for artdaq::ExceptionHandler*/
#include "artdaq-core/Utilities/ExceptionStackTrace.hh" /*for artdaq::ExceptionStackTrace*/
#include "cfoInterfaceLib/CFO_Compiler.hh"

#include "TRACE/tracemf.h"
#define TLVL_ResetCFO TLVL_DEBUG + 5
#define TLVL_AutogenDRP TLVL_DEBUG + 6
#define TLVL_SERDESReset TLVL_DEBUG + 7
#define TLVL_CalculateFreq TLVL_DEBUG + 8

#include "dtcInterfaceLib/otsStyleCoutMacros.h"

#undef __COUT_HDR__
#define __COUT_HDR__ "CFO " << this->getDeviceUID() << ": "

CFOLib::CFO_Registers::CFO_Registers(DTC_SimMode mode, int cfo, std::string expectedDesignVersion,
									 bool skipInit, const std::string& uid)
	: simMode_(mode), dmaSize_(16)
{
	TLOG(TLVL_INFO) << "CONSTRUCTOR";

	auto sim = getenv("CFOLIB_SIM_ENABLE");
	if (sim == nullptr)
	{
		// Give priority to CFOLIB_SIM_ENABLE, but go ahead and check DTCLIB_SIM_ENABLE, too
		sim = getenv("DTCLIB_SIM_ENABLE");
	}
	if (sim != nullptr)
	{
		switch (sim[0])
		{
			case '1':
			case 'e':
			case 'E':
				simMode_ = DTC_SimMode_Tracker;  // sim enabled
				break;
			case '2':
			case 'l':
			case 'L':
				simMode_ = DTC_SimMode_Loopback;
				break;
			case '0':
			default:
				simMode_ = DTC_SimMode_Disabled;
				break;
		}
	}

	if (cfo == -1)
	{
		auto CFOE = getenv("CFOLIB_CFO");
		if (CFOE != nullptr)
		{
			cfo = atoi(CFOE);
		}
		else
		{
			CFOE = getenv("DTCLIB_DTC");  // Check both environment variables for CFO
			if (CFOE != nullptr)
			{
				cfo = atoi(CFOE);
			}
			else
			{
				cfo = 0;
			}
		}
	}

	SetSimMode(expectedDesignVersion, simMode_, cfo, skipInit, (uid == "" ? ("CFO" + std::to_string(cfo)) : uid));
}  // end costructor()

CFOLib::CFO_Registers::~CFO_Registers()
{
	TLOG(TLVL_INFO) << "DESTRUCTOR";
}  // end destructor()

DTCLib::DTC_SimMode CFOLib::CFO_Registers::SetSimMode(std::string expectedDesignVersion, DTC_SimMode mode, int cfo,
													  bool skipInit, const std::string& uid)
{
	simMode_ = mode;

	TLOG(TLVL_INFO) << "Initializing CFO device, sim mode is " << DTC_SimModeConverter(simMode_).toString() << " for uid = " << uid << ", deviceIndex = " << cfo;

	device_.init(simMode_, cfo, /* simMemoryFile */ "", uid);
	if (expectedDesignVersion != "")
	{
		uint32_t parsedExpectedVersion = 0;
		std::string parsedDesignDate = "";
		try
		{
			parsedExpectedVersion = static_cast<uint32_t>(std::stoul(expectedDesignVersion, nullptr, 16));
			parsedDesignDate = ReadDesignDate(parsedExpectedVersion);
		}
		catch (...)  // illegal/non-hex expectedDesignVersion
		{
			__SS__;
			ss << "Version mismatch (is Expected Firmware Version string legal?)! Expected CFO (device index #" << cfo << ") version is '" << expectedDesignVersion << "' while the readback version was '" << ReadDesignVersion() << ".'" << __E__;
			__SS_THROW__;
		}

		if (parsedExpectedVersion != ReadRegister_(CFOandDTC_Register_DesignDate))
		{
			__SS__;
			ss << "Version mismatch! Expected CFO (device index #" << cfo << ") version is '" << parsedDesignDate << "' (0x" << std::hex << parsedExpectedVersion << " != 0x" << ReadRegister_(CFOandDTC_Register_DesignDate) << ") while the readback version was '" << ReadDesignVersion() << ".'" << __E__;
			__SS_THROW__;
			// throw new DTC_WrongVersionException(expectedDesignVersion, ReadDesignVersion());
		}
	}

	if (skipInit)
	{
		__COUT_INFO__ << "SKIPPING Initializing device";
		return simMode_;
	}

	__COUT__ << "Initialize requested, setting device registers acccording to sim mode " << DTC_SimModeConverter(simMode_).toString();
	for (auto link : CFO_Links)
	{
		bool LinkEnabled = ((maxDTCs_ >> (link * 4)) & 0xF) != 0;
		if (!LinkEnabled)
		{
			DisableLink(link);
		}
		else
		{
			int rocCount = (maxDTCs_ >> (link * 4)) & 0xF;
			EnableLink(link, DTC_LinkEnableMode(true, true), rocCount);
		}
		if (!LinkEnabled) SetSERDESLoopbackMode(link, DTC_SERDESLoopbackMode_Disabled);
	}

	if (simMode_ != DTC_SimMode_Disabled)
	{
		// Set up hardware simulation mode: Link 0 Tx/Rx Enabled, Loopback Enabled, ROC Emulator Enabled. All other Links
		// disabled.
		for (auto link : CFO_Links)
		{
			if (simMode_ == DTC_SimMode_Loopback)
			{
				SetSERDESLoopbackMode(link, DTC_SERDESLoopbackMode_NearPCS);
				//			SetMaxROCNumber(CFO_Link_0, CFO_ROC_0);
			}
		}
		// SetInternalSystemClock(); //RAR does not select clock to JA
		DisableEmbeddedClockMarker();
	}
	ReadMinDMATransferLength();

	__COUT__ << "Done setting device registers";
	return simMode_;
}

bool CFOLib::CFO_Registers::ReadDDRFIFOEmpty(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_DesignStatus);
	return data[2];
}

bool CFOLib::CFO_Registers::ReadDDRClockCalibrationDone(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_DesignStatus);
	return data[0];
}

DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatDesignStatus()
{
	auto form = CreateFormatter(CFOandDTC_Register_DesignStatus);
	form.description = "Design Status Register";
	form.vals.push_back("[ x = 1 (hi) ]");  // translation
	form.vals.push_back(std::string("DDR FIFO Empty:             [") + (ReadDDRFIFOEmpty(form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("DDR Clock Calibration Done: [") + (ReadDDRClockCalibrationDone(form.value) ? "x" : " ") + "]");
	return form;
}

// RAR: Now just Soft Reset resets run plan
//  void CFOLib::CFO_Registers::ResetCFORunPlan()
//  {
//  	TLOG(TLVL_ResetCFO) << __COUT_HDR__ << "SoftReset Run Plan start";
//  	std::bitset<32> data = ReadRegister_(CFOandDTC_Register_Control);
//  	data[27] = 1;  // CFO Run Plan Reset bit
//  	WriteRegister_(data.to_ulong(), CFOandDTC_Register_Control);
//  	data[27] = 0;  // Restore CFO Run Plan Reset bit
//  	WriteRegister_(data.to_ulong(), CFOandDTC_Register_Control);
//  }

// bool CFOLib::CFO_Registers::ReadResetCFORunPlan(std::optional<uint32_t> val)
// {
// 	std::bitset<32> dataSet = val.has_value()?*val:ReadRegister_(CFOandDTC_Register_Control);
// 	return dataSet[27];
// }

void CFOLib::CFO_Registers::EnableLED7()
{
	std::bitset<32> data = ReadRegister_(CFOandDTC_Register_Control);
	data[23] = 1;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_Control);
}

void CFOLib::CFO_Registers::DisableReadLED7()
{
	std::bitset<32> data = ReadRegister_(CFOandDTC_Register_Control);
	data[23] = 0;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_Control);
}

bool CFOLib::CFO_Registers::ReadLED7State(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_Control);
	return data[23];
}

void CFOLib::CFO_Registers::EnableAcceleratorRF0()
{
	std::bitset<32> data = ReadRegister_(CFOandDTC_Register_Control);
	data[2] = 1;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_Control);
}

void CFOLib::CFO_Registers::DisableAcceleratorRF0()
{
	std::bitset<32> data = ReadRegister_(CFOandDTC_Register_Control);
	data[2] = 0;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_Control);
}

bool CFOLib::CFO_Registers::ReadAcceleratorRF0Enable(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_Control);
	return data[2];
}

void CFOLib::CFO_Registers::EnableEmbeddedClockMarker()
{
	std::bitset<32> data = ReadRegister_(CFOandDTC_Register_Control);
	data[1] = 1;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_Control);
}


void CFOLib::CFO_Registers::DisableEmbeddedClockMarker()
{
	std::bitset<32> data = ReadRegister_(CFOandDTC_Register_Control);
	data[1] = 0;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_Control);
}

bool CFOLib::CFO_Registers::ReadEmbeddedClockMarkerEnable(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_Control);
	return data[1];
}

void CFOLib::CFO_Registers::EnablePunchedClock()
{
	std::bitset<32> data = ReadRegister_(CFOandDTC_Register_Control);
	data[9] = 1;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_Control);
}


void CFOLib::CFO_Registers::DisablePunchedClock()
{
	std::bitset<32> data = ReadRegister_(CFOandDTC_Register_Control);
	data[9] = 0;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_Control);
}

bool CFOLib::CFO_Registers::ReadPunchedClockEnable(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_Control);
	return data[9];
}

DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatCFOControl()
{
	auto form = CreateFormatter(CFOandDTC_Register_Control);
	form.description = "CFO Control";
	form.vals.push_back("[ x = 1 (hi) ]");  // translation
	// ~~~	CFO Control Register (0x9100) ~~~
	// Bit Position	Mode	Default Value	Description
	// 31	RW	0b0	CFO Soft Reset (Self-clearing)
	// 30-28	RO	0x0	Reserved
	// 27	RW	0b0	DDR Write Reset
	// 26-24	RO	0x0	Reserved
	// 23	RW	0b0	Reserved (Formerly DRP Auto Generate Enable)
	// 22-17	RO	0x00	Reserved
	// 16	RW	0b0	Led 7
	// 15-10	RO	0x0000	Reserved
	// 9	RW	0b0	Punched Clock Enable
	// 8	RW	0b0	SERDES Global Reset
	// 7-4	RO	0x0000	Reserved
	// 3	RW	0b0	CFO Loopback Test Launch Control
	// 2	RW	0x0	Accelerator RF-0 Event Window Input Enable
	// 1	RW	0b0	Embedded Clock Marker Enable
	// 0	RO	0b0	CFO Hard Reset (Self-clearing)

	form.vals.push_back(std::string("Bit-31 CFO Soft Reset (Self-clearing):  [") + (ReadSoftReset(form.value) ? "x" : " ") + "]");
	// RAR: not just Soft Reset for resetting run plan
	// form.vals.push_back(std::string("Bit-27 CFO Run Plan Reset:              [") + (ReadResetCFORunPlan(form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("Bit-16 LED 7:                           [") + (ReadLED7State(form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("Bit-09 Punched Clock Enable:            [") + (CFOandDTC_Registers::ReadPunchEnable(form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("Bit-08 SERDES Global Reset:             [") + (CFOandDTC_Registers::ReadResetSERDES(form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("Bit-02 Accelerator RF-0 Input Enable:   [") + (ReadAcceleratorRF0Enable(form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("Bit-01 Embedded Clock Marker Enable:    [") + (ReadEmbeddedClockMarkerEnable(form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("Bit-00 CFO Hard Reset (Self-clearing):  [") + (ReadHardReset(form.value) ? "x" : " ") + "]");
	return form;
}

// DMA Transfer Length Register
void CFOLib::CFO_Registers::SetTriggerDMATransferLength(uint16_t length)
{
	auto data = ReadRegister_(CFOandDTC_Register_DMATransferLength);
	data = (data & 0x0000FFFF) + (length << 16);
	WriteRegister_(data, CFOandDTC_Register_DMATransferLength);
}

uint16_t CFOLib::CFO_Registers::ReadTriggerDMATransferLength(std::optional<uint32_t> val)
{
	auto data = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_DMATransferLength);
	data >>= 16;
	return static_cast<uint16_t>(data);
}

void CFOLib::CFO_Registers::SetMinDMATransferLength(uint16_t length)
{
	auto data = ReadRegister_(CFOandDTC_Register_DMATransferLength);
	data = (data & 0xFFFF0000) + length;
	WriteRegister_(data, CFOandDTC_Register_DMATransferLength);
}

uint16_t CFOLib::CFO_Registers::ReadMinDMATransferLength(std::optional<uint32_t> val)
{
	auto data = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_DMATransferLength);
	data = data & 0x0000FFFF;
	dmaSize_ = static_cast<uint16_t>(data);
	return dmaSize_;
}

DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatDMATransferLength()
{
	auto form = CreateFormatter(CFOandDTC_Register_DMATransferLength);
	form.description = "DMA Transfer Length";
	form.vals.push_back("");  // translation
	std::stringstream o;
	o << "Trigger Length: 0x" << std::hex << ReadTriggerDMATransferLength(form.value);
	form.vals.push_back(o.str());
	std::stringstream p;
	p << "Minimum Length: 0x" << std::hex << ReadMinDMATransferLength(form.value);
	form.vals.push_back(p.str());
	return form;
}

// SERDES Loopback Enable Register
void CFOLib::CFO_Registers::SetSERDESLoopbackMode(const CFO_Link_ID& link, const DTC_SERDESLoopbackMode& mode)
{
	std::bitset<32> data = ReadRegister_(CFOandDTC_Register_SERDES_LoopbackEnable);
	std::bitset<3> modeSet = mode;
	data[3 * link] = modeSet[0];
	data[3 * link + 1] = modeSet[1];
	data[3 * link + 2] = modeSet[2];
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_SERDES_LoopbackEnable);
}

DTCLib::DTC_SERDESLoopbackMode CFOLib::CFO_Registers::ReadSERDESLoopback(const CFO_Link_ID& link, std::optional<uint32_t> val)
{
	std::bitset<3> dataSet = (val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_SERDES_LoopbackEnable)) >> (3 * link);
	return static_cast<DTC_SERDESLoopbackMode>(dataSet.to_ulong());
}

DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatSERDESLoopbackEnable()
{
	auto form = CreateFormatter(CFOandDTC_Register_SERDES_LoopbackEnable);
	form.description = "SERDES Loopback Enable";
	form.vals.push_back("");  // translation
	for (auto r : CFO_Links)
		form.vals.push_back(std::string("Link ") + std::to_string(r) + ": " +
							DTC_SERDESLoopbackModeConverter(ReadSERDESLoopback(r, form.value)).toString());
	return form;
}

// Clock Status Register
bool CFOLib::CFO_Registers::ReadSERDESOscillatorIICError(std::optional<uint32_t> val)
{
	std::bitset<32> dataSet = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_ClockOscillatorStatus);
	return dataSet[2];
}

bool CFOLib::CFO_Registers::ReadSERDESOscillatorInitializationComplete(std::optional<uint32_t> val)
{
	std::bitset<32> dataSet = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_ClockOscillatorStatus);
	return dataSet[1];
}

bool CFOLib::CFO_Registers::WaitForSERDESOscillatorInitializationComplete(double max_wait)
{
	auto start_time = std::chrono::steady_clock::now();
	while (
		!ReadSERDESOscillatorInitializationComplete() &&
		std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now() - start_time).count() <
			max_wait)
	{
		usleep(1000);
	}
	return ReadSERDESOscillatorInitializationComplete();
}

bool CFOLib::CFO_Registers::ReadTimingClockPLLLocked(std::optional<uint32_t> val)
{
	std::bitset<32> dataSet = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_ClockOscillatorStatus);
	return dataSet[0];
}

DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatClockOscillatorStatus()
{
	auto form = CreateFormatter(CFOandDTC_Register_ClockOscillatorStatus);
	form.description = "Clock Oscillator Status";
	form.vals.push_back("[ x = 1 (hi) ]");  // translation
	form.vals.push_back(std::string("SERDES IIC Error:      [") + (ReadSERDESOscillatorIICError(form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("SERDES Init.Complete:  [") +
						(ReadSERDESOscillatorInitializationComplete(form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("Timing Clock PLL Lock: [") + (ReadTimingClockPLLLocked(form.value) ? "x" : " ") + "]");
	return form;
}

// Link Enable Register
void CFOLib::CFO_Registers::EnableLink(const CFO_Link_ID& link, const DTC_LinkEnableMode& mode,
									   const uint8_t& dtcCount)
{
	std::bitset<32> data = ReadRegister_(CFOandDTC_Register_LinkEnable);
	if (link == CFO_Link_ALL)
	{
		for (uint8_t i = 0; i < 8; ++i)
		{
			data[i] = mode.TransmitEnable;
			data[i + 8] = mode.ReceiveEnable;
		}
	}
	else
	{
		data[link] = mode.TransmitEnable;
		data[link + 8] = mode.ReceiveEnable;
	}
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_LinkEnable);
	SetMaxDTCNumber(link, dtcCount);
}

void CFOLib::CFO_Registers::DisableLink(const CFO_Link_ID& link, const DTC_LinkEnableMode& mode)
{
	std::bitset<32> data = ReadRegister_(CFOandDTC_Register_LinkEnable);
	data[link] = data[link] && !mode.TransmitEnable;
	data[link + 8] = data[link + 8] && !mode.ReceiveEnable;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_LinkEnable);
}

DTCLib::DTC_LinkEnableMode CFOLib::CFO_Registers::ReadLinkEnabled(const CFO_Link_ID& link, std::optional<uint32_t> val)
{
	std::bitset<32> dataSet = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_LinkEnable);
	return DTC_LinkEnableMode(dataSet[link], dataSet[link + 8]);
}

DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatLinkEnable()
{
	auto form = CreateFormatter(CFOandDTC_Register_LinkEnable);
	form.description = "Link Enable";
	form.vals.push_back("       ([TX, RX, Timing])");
	for (auto r : CFO_Links)
	{
		auto re = ReadLinkEnabled(r);
		form.vals.push_back(std::string("Link ") + std::to_string(r) + ": [" + (re.TransmitEnable ? "x" : " ") + "," +
							(re.ReceiveEnable ? "x" : " ") + "]");
	}
	return form;
}

// SERDES Reset Register
void CFOLib::CFO_Registers::ResetSERDES(const CFO_Link_ID& link, int interval)
{
	TLOG(TLVL_SERDESReset) << __COUT_HDR__ << "Entering SERDES Reset Loop for Link " << link;
	std::bitset<32> data = ReadRegister_(CFOandDTC_Register_SERDES_Reset);
	if (link == CFO_Link_ALL)
	{
		for (uint8_t i = 0; i < 8; ++i)
			data[i] = 1;
	}
	else
		data[link] = 1;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_SERDES_Reset);

	// usleep(interval);

	data = ReadRegister_(CFOandDTC_Register_SERDES_Reset);
	if (link == CFO_Link_ALL)
	{
		for (uint8_t i = 0; i < 8; ++i)
			data[i] = 0;
	}
	else
		data[link] = 0;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_SERDES_Reset);

	auto resetDone = false;
	uint32_t loops = 0;
	while (!resetDone && ++loops < 100)
	{
		usleep(interval);

		if (link == CFO_Link_ALL)
		{
			uint32_t readData = ReadRegister_(CFOandDTC_Register_SERDES_ResetDone);
			resetDone = ((readData & 0xFF) == 0xFF);
		}
		else
			resetDone = ReadResetSERDESDone(link);
		TLOG(TLVL_SERDESReset) << __COUT_HDR__ << "End of SERDES Reset loop=" << loops << ", done=" << std::boolalpha << resetDone;
	}
	if (loops >= 100)
	{
		__SS__ << "Timeout waiting for SERDES Reset loop=" << loops;
		__SS_THROW__;
		// throw DTC_IOErrorException("Timeout waiting for SERDES Reset loop.");
	}
}

void CFOLib::CFO_Registers::ResetAllSERDESPlls()
{
	WriteRegister_(0x0000ff00, CFOandDTC_Register_SERDES_Reset);
	WriteRegister_(0x0, CFOandDTC_Register_SERDES_Reset);
	sleep(3);
}
void CFOLib::CFO_Registers::ResetAllSERDESTx()
{
	WriteRegister_(0x00ff0000, CFOandDTC_Register_SERDES_Reset);
	WriteRegister_(0x0, CFOandDTC_Register_SERDES_Reset);
	sleep(3);
}

bool CFOLib::CFO_Registers::ReadResetSERDES(const CFO_Link_ID& link, std::optional<uint32_t> val)
{
	std::bitset<32> dataSet = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_SERDES_Reset);
	return dataSet[link];
}

DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatSERDESReset()
{
	auto form = CreateFormatter(CFOandDTC_Register_SERDES_Reset);
	form.description = "SERDES Reset";
	form.vals.push_back("[ x = 1 (hi) ]");  // translation
	for (auto r : CFO_Links)
		form.vals.push_back(std::string("Link ") + std::to_string(r) + ": [" +
							(ReadResetSERDES(r, form.value) ? "x" : " ") + "]");

	return form;
}

// SERDES RX Disparity Error Register
DTCLib::DTC_SERDESRXDisparityError CFOLib::CFO_Registers::ReadSERDESRXDisparityError(const CFO_Link_ID& link, std::optional<uint32_t> val)
{
	return DTC_SERDESRXDisparityError(val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_SERDES_RXDisparityError), static_cast<DTC_Link_ID>(link));
}

DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatSERDESRXDisparityError()
{
	auto form = CreateFormatter(CFOandDTC_Register_SERDES_RXDisparityError);
	form.description = "SERDES RX Disparity Error";
	form.vals.push_back("       ([H,L])");
	for (auto r : CFO_Links)
	{
		auto re = ReadSERDESRXDisparityError(r, form.value);
		form.vals.push_back(std::string("Link ") + std::to_string(r) + ": [" + std::to_string(re.GetData()[1]) + "," +
							std::to_string(re.GetData()[0]) + "]");
	}
	return form;
}

// SERDES RX Character Not In Table Error Register
DTCLib::DTC_CharacterNotInTableError CFOLib::CFO_Registers::ReadSERDESRXCharacterNotInTableError(const CFO_Link_ID& link, std::optional<uint32_t> val)
{
	return DTC_CharacterNotInTableError(val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_SERDES_RXCharacterNotInTableError),
										static_cast<DTC_Link_ID>(link));
}

DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatSERDESRXCharacterNotInTableError()
{
	auto form = CreateFormatter(CFOandDTC_Register_SERDES_RXCharacterNotInTableError);
	form.description = "SERDES RX CNIT Error";
	form.vals.push_back("       ([H,L])");
	for (auto r : CFO_Links)
	{
		auto re = ReadSERDESRXCharacterNotInTableError(r, form.value);
		form.vals.push_back(std::string("Link ") + std::to_string(r) + ": [" + std::to_string(re.GetData()[1]) + "," +
							std::to_string(re.GetData()[0]) + "]");
	}
	return form;
}

// SERDES Unlock Error Register
bool CFOLib::CFO_Registers::ReadSERDESUnlockError(const CFO_Link_ID& link, std::optional<uint32_t> val)
{
	std::bitset<32> dataSet = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_SERDES_UnlockError);
	return dataSet[link];
}

DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatSERDESUnlockError()
{
	auto form = CreateFormatter(CFOandDTC_Register_SERDES_UnlockError);
	form.description = "SERDES Unlock Error";
	form.vals.push_back("[ x = 1 (hi) ]");  // translation
	for (auto r : CFO_Links)
		form.vals.push_back(std::string("Link ") + std::to_string(r) + ": [" +
							(ReadSERDESUnlockError(r, form.value) ? "x" : " ") +
							"]");
	return form;
}

// SERDES PLL Locked Register
bool CFOLib::CFO_Registers::ReadSERDESPLLLocked(const CFO_Link_ID& link, std::optional<uint32_t> val)
{
	std::bitset<32> dataSet = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_SERDES_PLLLocked);
	return dataSet[link];
}

DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatSERDESPLLLocked()
{
	auto form = CreateFormatter(CFOandDTC_Register_SERDES_PLLLocked);
	form.description = "SERDES PLL Locked";
	form.vals.push_back("[ x = 1 (hi) ]");  // translation
	for (auto r : CFO_Links)
		form.vals.push_back(std::string("Link ") + std::to_string(r) + ": [" + (ReadSERDESPLLLocked(r, form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("Clock to JA:    [") + ((((form.value) >> 8) & 1) ? "x" : " ") + "]");
	form.vals.push_back(std::string("Clock from JA:  [") + ((((form.value) >> 9) & 1) ? "x" : " ") + "]");
	return form;
}

// SERDES RX Status Register
DTCLib::DTC_RXStatus CFOLib::CFO_Registers::ReadSERDESRXStatus(const CFO_Link_ID& link, std::optional<uint32_t> val)
{
	auto data = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_SERDES_RXStatus);
	data = (data >> (3 * link)) & 0x7;
	return static_cast<DTC_RXStatus>(data);
}

DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatSERDESRXStatus()
{
	auto form = CreateFormatter(CFOandDTC_Register_SERDES_RXStatus);
	form.description = "SERDES RX Status";
	form.vals.push_back("");  // translation
	for (auto r : CFO_Links)
	{
		auto re = ReadSERDESRXStatus(r, form.value);
		form.vals.push_back(std::string("Link ") + std::to_string(r) + ": " + DTC_RXStatusConverter(re).toString());
	}

	return form;
}

// SERDES Reset Done Register
bool CFOLib::CFO_Registers::ReadResetSERDESDone(const CFO_Link_ID& link, std::optional<uint32_t> val)
{
	std::bitset<32> dataSet = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_SERDES_ResetDone);
	return dataSet[link];
}

DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatSERDESResetDone()
{
	auto form = CreateFormatter(CFOandDTC_Register_SERDES_ResetDone);
	form.description = "SERDES Reset Done";
	form.vals.push_back("[ x = 1 (hi) ]");  // translation
	for (auto r : CFO_Links)
		form.vals.push_back(std::string("Link ") + std::to_string(r) + ": [" +
							(ReadResetSERDESDone(r, form.value) ? "x" : " ") + "]");
	return form;
}

// SFP / SERDES Status Register

bool CFOLib::CFO_Registers::ReadSERDESRXCDRLock(const CFO_Link_ID& link, std::optional<uint32_t> val)
{
	std::bitset<32> dataSet = val.has_value() ? *val : ReadRegister_(CFO_Register_SFPSERDESStatus);
	return dataSet[link];
}

DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatSERDESRXCDRLock()
{
	auto form = CreateFormatter(CFO_Register_SFPSERDESStatus);
	form.description = "SERDES CDR Lock";
	form.vals.push_back("[ x = 1 (hi) ]");  // translation
	for (auto r : CFO_Links)
		form.vals.push_back(std::string("Link ") + std::to_string(r) + ": [" +
							(ReadSERDESRXCDRLock(r, form.value) ? "x" : " ") + "]");
	return form;
}

void CFOLib::CFO_Registers::SetBeamOnTimerPreset(uint32_t preset)
{
	WriteRegister_(preset, CFO_Register_BeamOnTimerPreset);
}

uint32_t CFOLib::CFO_Registers::ReadBeamOnTimerPreset(std::optional<uint32_t> val) { return val.has_value() ? *val : ReadRegister_(CFO_Register_BeamOnTimerPreset); }

DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatBeamOnTimerPreset()
{
	auto form = CreateFormatter(CFO_Register_BeamOnTimerPreset);
	form.description = "Beam On Timer Preset Register";
	form.vals.push_back(std::to_string(ReadBeamOnTimerPreset(form.value)));
	return form;
}

void CFOLib::CFO_Registers::EnableBeamOnMode(const CFO_Link_ID& link)
{
	SetLinuxTimestampPreset();
	std::bitset<32> data = ReadRegister_(CFO_Register_EnableBeamOnMode);
	data[0] = 1;  // Enable beam on processing a single global flag as of December 2023
	WriteRegister_(data.to_ulong(), CFO_Register_EnableBeamOnMode);
}

void CFOLib::CFO_Registers::DisableBeamOnMode(const CFO_Link_ID& link)
{
	std::bitset<32> data = ReadRegister_(CFO_Register_EnableBeamOnMode);
	data[0] = 0;  // Enable beam on processing a single global flag as of December 2023
	WriteRegister_(data.to_ulong(), CFO_Register_EnableBeamOnMode);
}

bool CFOLib::CFO_Registers::ReadBeamOnMode(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(CFO_Register_EnableBeamOnMode);
	return data[0];  // Enable beam on processing a single global flag as of December 2023
}

DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatBeamOnMode()
{
	auto form = CreateFormatter(CFO_Register_EnableBeamOnMode);
	form.description = "Enable Beam On Mode Register";
	form.vals.push_back("[ x = 1 (hi) ]");  // translation
											// Enable beam on processing a single global flag as of December 2023
	form.vals.push_back(std::string("Beam On Processing ") + ": [" + (ReadBeamOnMode(form.value) ? "x" : " ") + "]");
	return form;
}

void CFOLib::CFO_Registers::EnableBeamOffMode(const CFO_Link_ID& link)
{
	SetLinuxTimestampPreset();
	std::bitset<32> data = ReadRegister_(CFO_Register_EnableBeamOffMode);
	data[0] = 1;  // Enable beam off processing a single global flag as of December 2023
	WriteRegister_(data.to_ulong(), CFO_Register_EnableBeamOffMode);
}

void CFOLib::CFO_Registers::DisableBeamOffMode(const CFO_Link_ID& link)
{
	std::bitset<32> data = ReadRegister_(CFO_Register_EnableBeamOffMode);
	data[0] = 0;  // Enable off processing a single global flag as of December 2023
	WriteRegister_(data.to_ulong(), CFO_Register_EnableBeamOffMode);
}

bool CFOLib::CFO_Registers::ReadBeamOffMode(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(CFO_Register_EnableBeamOffMode);
	return data[0];  // Enable beam on processing a single global flag as of December 2023
}

DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatBeamOffMode()
{
	auto form = CreateFormatter(CFO_Register_EnableBeamOffMode);
	form.description = "Enable Beam Off Mode Register";
	form.vals.push_back("[ x = 1 (hi) ]");  // translation
	// Enable off processing a single global flag as of December 2023
	form.vals.push_back(std::string("Beam Off Processing ") + ": [" + (ReadBeamOffMode(form.value) ? "x" : " ") + "]");
	return form;
}

// LEGACY: register 0x9154 repurposed as RunPlanSubrunEvtLimit; these functions are no longer valid
// void CFOLib::CFO_Registers::SetClockMarkerIntervalCount(uint32_t data)
// {
// 	WriteRegister_(data, CFO_Register_ClockMarkerIntervalCount);
// }

// LEGACY: register 0x9154 repurposed as RunPlanSubrunEvtLimit; these functions are no longer valid
// uint32_t CFOLib::CFO_Registers::ReadClockMarkerIntervalCount(std::optional<uint32_t> val)
// {
// 	return val.has_value() ? *val : ReadRegister_(CFO_Register_ClockMarkerIntervalCount);
// }

// LEGACY: register 0x9154 repurposed as RunPlanSubrunEvtLimit; these functions are no longer valid
// DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatClockMarkerIntervalCount()
// {
// 	auto form = CreateFormatter(CFO_Register_ClockMarkerIntervalCount);
// 	form.description = "40 MHz Clock Marker Interval Count Register";
// 	form.vals.push_back(std::to_string(ReadClockMarkerIntervalCount()));
// 	return form;
// }

// SEREDES Oscillator Registers
uint32_t CFOLib::CFO_Registers::ReadSERDESOscillatorFrequency(std::optional<uint32_t> val)
{
	return val.has_value() ? *val : ReadRegister_(CFO_Register_SERDESOscillatorFrequency);
}
void CFOLib::CFO_Registers::SetSERDESOscillatorFrequency(uint32_t freq)
{
	WriteRegister_(freq, CFO_Register_SERDESOscillatorFrequency);
}
bool CFOLib::CFO_Registers::ReadSERDESOscillatorIICInterfaceReset(std::optional<uint32_t> val)
{
	auto dataSet = std::bitset<32>(val.has_value() ? *val : ReadRegister_(CFO_Register_SERDESClock_IICBusControl));
	return dataSet[31];
}

void CFOLib::CFO_Registers::ResetSERDESOscillatorIICInterface()
{
	auto bs = std::bitset<32>();
	bs[31] = 1;
	WriteRegister_(bs.to_ulong(), CFO_Register_SERDESClock_IICBusControl);
	while (ReadSERDESOscillatorIICInterfaceReset())
	{
		usleep(1000);
	}
}

// Jitter Attenuator CSR Register
/// <summary>
/// Read the value of the Jitter Attenuator Select
/// </summary>
/// <returns>Jitter Attenuator Select value</returns>
std::bitset<2> CFOLib::CFO_Registers::ReadJitterAttenuatorSelect(std::optional<uint32_t> val) { return CFOandDTC_Registers::ReadJitterAttenuatorSelect(CFO_Register_JitterAttenuatorCSR, val); }

/// <summary>
/// Set the Jitter Attenuator Select bits
/// </summary>
/// <param name="data">Value to set</param>
void CFOLib::CFO_Registers::SetJitterAttenuatorSelect(std::bitset<2> data, bool alsoResetJA /* = false */) { CFOandDTC_Registers::SetJitterAttenuatorSelect(CFO_Register_JitterAttenuatorCSR, data, alsoResetJA); }

/// <summary>
/// Read the Jitter Attenuator Reset bit
/// </summary>
/// <returns>Value of the Jitter Attenuator Reset bit</returns>
bool CFOLib::CFO_Registers::ReadJitterAttenuatorReset(std::optional<uint32_t> val) { return CFOandDTC_Registers::ReadJitterAttenuatorReset(CFO_Register_JitterAttenuatorCSR, val); }
bool CFOLib::CFO_Registers::ReadJitterAttenuatorLocked(std::optional<uint32_t> val) { return CFOandDTC_Registers::ReadJitterAttenuatorLocked(CFO_Register_JitterAttenuatorCSR, val); }

/// <summary>
/// Reset the Jitter Attenuator
/// </summary>
void CFOLib::CFO_Registers::ResetJitterAttenuator() { CFOandDTC_Registers::ResetJitterAttenuator(CFO_Register_JitterAttenuatorCSR); }

/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatJitterAttenuatorCSR() { return CFOandDTC_Registers::FormatJitterAttenuatorCSR(CFO_Register_JitterAttenuatorCSR); }

uint64_t CFOLib::CFO_Registers::ReadSERDESOscillatorParameters(std::optional<uint32_t> val)
{
	uint64_t data = (static_cast<uint64_t>(ReadSERDESIICInterface(DTC_IICSERDESBusAddress_EVB, 7)) << 40) +
					(static_cast<uint64_t>(ReadSERDESIICInterface(DTC_IICSERDESBusAddress_EVB, 8)) << 32) +
					(static_cast<uint64_t>(ReadSERDESIICInterface(DTC_IICSERDESBusAddress_EVB, 9)) << 24) +
					(static_cast<uint64_t>(ReadSERDESIICInterface(DTC_IICSERDESBusAddress_EVB, 10)) << 16) +
					(static_cast<uint64_t>(ReadSERDESIICInterface(DTC_IICSERDESBusAddress_EVB, 11)) << 8) +
					static_cast<uint64_t>(ReadSERDESIICInterface(DTC_IICSERDESBusAddress_EVB, 12));
	return data;
}
void CFOLib::CFO_Registers::SetSERDESOscillatorParameters(uint64_t program)
{
	WriteSERDESIICInterface(DTC_IICSERDESBusAddress_EVB, 0x89, 0x10);

	WriteSERDESIICInterface(DTC_IICSERDESBusAddress_EVB, 7, static_cast<uint8_t>(program >> 40));
	WriteSERDESIICInterface(DTC_IICSERDESBusAddress_EVB, 8, static_cast<uint8_t>(program >> 32));
	WriteSERDESIICInterface(DTC_IICSERDESBusAddress_EVB, 9, static_cast<uint8_t>(program >> 24));
	WriteSERDESIICInterface(DTC_IICSERDESBusAddress_EVB, 10, static_cast<uint8_t>(program >> 16));
	WriteSERDESIICInterface(DTC_IICSERDESBusAddress_EVB, 11, static_cast<uint8_t>(program >> 8));
	WriteSERDESIICInterface(DTC_IICSERDESBusAddress_EVB, 12, static_cast<uint8_t>(program));

	WriteSERDESIICInterface(DTC_IICSERDESBusAddress_EVB, 0x89, 0);
	WriteSERDESIICInterface(DTC_IICSERDESBusAddress_EVB, 0x87, 0x40);
}

DTCLib::DTC_SerdesClockSpeed CFOLib::CFO_Registers::ReadSERDESOscillatorClock(std::optional<uint32_t> val)
{
	auto freq = ReadSERDESOscillatorFrequency(val);

	// Clocks should be accurate to 30 ppm
	if (freq > 156250000 - 4687.5 && freq < 156250000 + 4687.5) return DTC_SerdesClockSpeed_3125Gbps;
	if (freq > 125000000 - 3750 && freq < 125000000 + 3750) return DTC_SerdesClockSpeed_25Gbps;
	return DTC_SerdesClockSpeed_Unknown;
}
void CFOLib::CFO_Registers::SetSERDESOscillatorClock(DTC_SerdesClockSpeed speed)
{
	double targetFreq;
	switch (speed)
	{
		case DTC_SerdesClockSpeed_25Gbps:
			targetFreq = 125000000.0;
			break;
		case DTC_SerdesClockSpeed_3125Gbps:
			targetFreq = 156250000.0;
			break;
		case DTC_SerdesClockSpeed_48Gbps:
			targetFreq = 240000000.0;
			break;
		default:
			targetFreq = 0.0;
			break;
	}
	if (SetNewOscillatorFrequency(targetFreq))
	{
		for (auto& Link : CFO_Links)
		{
			ResetSERDES(Link, 1000);
		}
	}
}

DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatSERDESOscillatorFrequency()
{
	auto form = CreateFormatter(CFO_Register_SERDESOscillatorFrequency);
	form.description = "SERDES Oscillator Frequency";
	std::stringstream o;
	o << std::dec << ReadSERDESOscillatorFrequency(form.value);
	form.vals.push_back(o.str());
	return form;
}
DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatSERDESOscillatorControl()
{
	auto form = CreateFormatter(CFO_Register_SERDESClock_IICBusControl);
	form.description = "SERDES Oscillator IIC Bus Control";
	form.vals.push_back("[ x = 1 (hi) ]");  // translation
	form.vals.push_back(std::string("Reset:  [") + (ReadSERDESOscillatorIICInterfaceReset() ? "x" : " ") + "]");
	return form;
}

/// Timestamp Preset Registers
void CFOLib::CFO_Registers::SetEventWindowTagPreset(const DTC_EventWindowTag& preset)
{
	auto timestamp = preset.GetEventWindowTag();
	auto timestampLow = static_cast<uint32_t>(timestamp.to_ulong());
	timestamp >>= 32;
	auto timestampHigh = static_cast<uint16_t>(timestamp.to_ulong());

	WriteRegister_(timestampLow, CFO_Register_TimestampPreset0);
	WriteRegister_(timestampHigh, CFO_Register_TimestampPreset1);
}

DTCLib::DTC_EventWindowTag CFOLib::CFO_Registers::ReadTimestampPreset(std::optional<uint32_t> val)
{
	auto timestampLow = val.has_value() ? *val : ReadRegister_(CFO_Register_TimestampPreset0);
	DTC_EventWindowTag output;
	output.SetEventWindowTag(timestampLow, static_cast<uint16_t>(ReadRegister_(CFO_Register_TimestampPreset1)));
	return output;
}

DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatTimestampPreset0()
{
	auto form = CreateFormatter(CFO_Register_TimestampPreset0);
	form.description = "Timestamp Preset 0";
	std::stringstream o;
	o << "0x" << std::hex << ReadRegister_(CFO_Register_TimestampPreset0);
	form.vals.push_back(o.str());
	return form;
}

DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatTimestampPreset1()
{
	auto form = CreateFormatter(CFO_Register_TimestampPreset1);
	form.description = "Timestamp Preset 1";
	std::stringstream o;
	o << "0x" << std::hex << ReadRegister_(CFO_Register_TimestampPreset1);
	form.vals.push_back(o.str());
	return form;
}

/// Linux Timestamp Register
void CFOLib::CFO_Registers::SetLinuxTimestampPreset()
{
	WriteRegister_(time(0), CFO_Register_LinuxTimestamp);
}

time_t CFOLib::CFO_Registers::ReadLinuxTimestamp()
{
	auto val = ReadRegister_(CFO_Register_LinuxTimestamp);
	return val;
}

// NUMDTCs Register
void CFOLib::CFO_Registers::SetMaxDTCNumber(const CFO_Link_ID& link, const uint8_t& dtcCount)
{
	ReadLinkDTCCount(link, false);
	if (link == CFO_Link_ALL)
	{
		for (uint8_t i = 0; i < 8; ++i)
		{
			uint32_t mask = ~(0xF << (i * 4));
			maxDTCs_ = (maxDTCs_ & mask) + ((dtcCount & 0xF) << (i * 4));
		}
	}
	else
	{
		uint32_t mask = ~(0xF << (link * 4));
		maxDTCs_ = (maxDTCs_ & mask) + ((dtcCount & 0xF) << (link * 4));
	}
	WriteRegister_(maxDTCs_, CFO_Register_NUMDTCs);
}

uint8_t CFOLib::CFO_Registers::ReadLinkDTCCount(const CFO_Link_ID& link, bool local, std::optional<uint32_t> val)
{
	if (!local)
	{
		auto data = val.has_value() ? *val : ReadRegister_(CFO_Register_NUMDTCs);
		maxDTCs_ = data;
	}
	return (maxDTCs_ >> (link * 4)) & 0xF;
}

DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatNUMDTCs()
{
	auto form = CreateFormatter(CFO_Register_NUMDTCs);
	form.description = "Number of DTCs Register";
	form.vals.push_back("[ x = 1 (hi) ]");  // translation
	for (auto r : CFO_Links)
		form.vals.push_back(std::string("Link ") + std::to_string(r) + ": [" +
							(ReadLinkDTCCount(r, false, form.value) ? "x" : " ") + "]");
	return form;
}

// FIFO Full Error Flags Registers
void CFOLib::CFO_Registers::ClearFIFOFullErrorFlags(const CFO_Link_ID& link)
{
	auto flags = ReadFIFOFullErrorFlags(link);
	std::bitset<32> data0 = 0;

	data0[link] = flags.CFOLinkInput;

	WriteRegister_(data0.to_ulong(), CFO_Register_FIFOFullErrorFlag0);
}

DTCLib::DTC_FIFOFullErrorFlags CFOLib::CFO_Registers::ReadFIFOFullErrorFlags(const CFO_Link_ID& link, std::optional<uint32_t> val)
{
	std::bitset<32> data0 = val.has_value() ? *val : ReadRegister_(CFO_Register_FIFOFullErrorFlag0);
	DTC_FIFOFullErrorFlags flags;

	flags.CFOLinkInput = data0[link];

	return flags;
}

DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatFIFOFullErrorFlag0()
{
	auto form = CreateFormatter(CFO_Register_FIFOFullErrorFlag0);
	form.description = "FIFO Full Error Flags 0";
	form.vals.push_back("[ x = 1 (hi) ]");  // translation
	for (auto r : CFO_Links)
	{
		auto re = ReadFIFOFullErrorFlags(r, form.value);
		form.vals.push_back(std::string("Link ") + std::to_string(r) + ": [" + (re.CFOLinkInput ? "x" : " ") + "]");
	}
	return form;
}

// Receive Packet Error Register
void CFOLib::CFO_Registers::ClearRXElasticBufferUnderrun(const CFO_Link_ID& link)
{
	std::bitset<32> data = ReadRegister_(CFO_Register_ReceivePacketError);
	data[static_cast<int>(link) + 24] = 0;
	WriteRegister_(data.to_ulong(), CFO_Register_ReceivePacketError);
}

bool CFOLib::CFO_Registers::ReadRXElasticBufferUnderrun(const CFO_Link_ID& link, std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(CFO_Register_ReceivePacketError);
	return data[static_cast<int>(link) + 24];
}

void CFOLib::CFO_Registers::ClearRXElasticBufferOverrun(const CFO_Link_ID& link)
{
	std::bitset<32> data = ReadRegister_(CFO_Register_ReceivePacketError);
	data[static_cast<int>(link) + 16] = 0;
	WriteRegister_(data.to_ulong(), CFO_Register_ReceivePacketError);
}

bool CFOLib::CFO_Registers::ReadRXElasticBufferOverrun(const CFO_Link_ID& link, std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(CFO_Register_ReceivePacketError);
	return data[static_cast<int>(link) + 16];
}

void CFOLib::CFO_Registers::ClearPacketError(const CFO_Link_ID& link)
{
	std::bitset<32> data = ReadRegister_(CFO_Register_ReceivePacketError);
	data[static_cast<int>(link) + 8] = 0;
	WriteRegister_(data.to_ulong(), CFO_Register_ReceivePacketError);
}

bool CFOLib::CFO_Registers::ReadPacketError(const CFO_Link_ID& link, std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(CFO_Register_ReceivePacketError);
	return data[static_cast<int>(link) + 8];
}

void CFOLib::CFO_Registers::ClearPacketCRCError(const CFO_Link_ID& link)
{
	std::bitset<32> data = ReadRegister_(CFO_Register_ReceivePacketError);
	data[static_cast<int>(link)] = 0;
	WriteRegister_(data.to_ulong(), CFO_Register_ReceivePacketError);
}

bool CFOLib::CFO_Registers::ReadPacketCRCError(const CFO_Link_ID& link, std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(CFO_Register_ReceivePacketError);
	return data[static_cast<int>(link)];
}

DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatReceivePacketError()
{
	auto form = CreateFormatter(CFO_Register_ReceivePacketError);
	form.description = "Receive Packet Error";
	form.vals.push_back("       ([CRC, PacketError, RX Overrun, RX Underrun])");
	for (auto r : CFO_Links)
		form.vals.push_back(std::string("Link ") + std::to_string(r) + ": [" +
							(ReadPacketCRCError(r, form.value) ? "x" : " ") + "," +
							(ReadPacketError(r, form.value) ? "x" : " ") + "," +
							(ReadRXElasticBufferOverrun(r, form.value) ? "x" : " ") + "," +
							(ReadRXElasticBufferUnderrun(r, form.value) ? "x" : " ") + "]");
	return form;
}

void CFOLib::CFO_Registers::SetEventWindowEmulatorInterval(const uint32_t& data)
{
	__SS__ << "Access attempt of CFO_Register_EventWindowEmulatorIntervalTime = 0x91A0,.."
			  "this register was deleted in Firmware version: Nov/09/2023 11:00   raw-data: 0x23110911; "
			  "please update the software to use the CFO Run Plan to control the Event Window duration."
		   << __E__;
	__SS_THROW__;
	// WriteRegister_(data, CFO_Register_EventWindowEmulatorIntervalTime);
}

uint32_t CFOLib::CFO_Registers::ReadEventWindowEmulatorInterval(std::optional<uint32_t> val)
{
	__SS__ << "Access attempt of CFO_Register_EventWindowEmulatorIntervalTime = 0x91A0,.."
			  "this register was deleted in Firmware version: Nov/09/2023 11:00   raw-data: 0x23110911; "
			  "please update the software to use the CFO Run Plan to control the Event Window duration."
		   << __E__;
	__SS_THROW__;
	// return val.has_value()?*val:ReadRegister_(CFO_Register_EventWindowEmulatorIntervalTime);
}

DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatEventWindowEmulatorIntervalTime()
{
	__SS__ << "Access attempt of CFO_Register_EventWindowEmulatorIntervalTime = 0x91A0,.."
			  "this register was deleted in Firmware version: Nov/09/2023 11:00   raw-data: 0x23110911; "
			  "please update the software to use the CFO Run Plan to control the Event Window duration."
		   << __E__;
	__SS_THROW__;
	// auto form = CreateFormatter(CFO_Register_EventWindowEmulatorIntervalTime);
	// form.description = "Event Window Emulator Interval Time";
	// form.vals.push_back(std::to_string(ReadEventWindowEmulatorInterval()));
	// return form;
}

void CFOLib::CFO_Registers::SetEventWindowHoldoffTime(const uint32_t& data)
{
	WriteRegister_(data, CFO_Register_EventWindowHoldoffTime);
}

uint32_t CFOLib::CFO_Registers::ReadEventWindowHoldoffTime(std::optional<uint32_t> val)
{
	return val.has_value() ? *val : ReadRegister_(CFO_Register_EventWindowHoldoffTime);
}

DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatEventWindowHoldoffTime()
{
	auto form = CreateFormatter(CFO_Register_EventWindowHoldoffTime);
	form.description = "Event Window Holdoff Time";
	form.vals.push_back(std::to_string(ReadEventWindowHoldoffTime(form.value)));
	return form;
}

bool CFOLib::CFO_Registers::ReadEventWindowTimeoutError(const CFO_Link_ID& link, std::optional<uint32_t> val)
{
	std::bitset<32> dataSet = val.has_value() ? *val : ReadRegister_(CFO_Register_EventWindowTimeoutError);
	return dataSet[link];
}

void CFOLib::CFO_Registers::ClearEventWindowTimeoutError(const CFO_Link_ID& link)
{
	std::bitset<32> data = ReadRegister_(CFO_Register_EventWindowTimeoutError);
	data[link] = 1;
	WriteRegister_(data.to_ulong(), CFO_Register_EventWindowTimeoutError);
}

DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatEventWindowTimeoutError()
{
	auto form = CreateFormatter(CFO_Register_EventWindowTimeoutError);
	form.description = "Event Window Timeout Error";
	form.vals.push_back("[ x = 1 (hi) ]");  // translation
	for (auto r : CFO_Links)
		form.vals.push_back(std::string("Link ") + std::to_string(r) + ": [" +
							(ReadEventWindowTimeoutError(r, form.value) ? "x" : " ") + "]");
	return form;
}

void CFOLib::CFO_Registers::SetEventWindowTimeoutInterval(const uint32_t& data)
{
	WriteRegister_(data, CFO_Register_EventWindowTimeoutValue);
}

uint32_t CFOLib::CFO_Registers::ReadEventWindowTimeoutInterval(std::optional<uint32_t> val)
{
	return val.has_value() ? *val : ReadRegister_(CFO_Register_EventWindowTimeoutValue);
}

DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatEventWindowTimeoutInterval()
{
	auto form = CreateFormatter(CFO_Register_EventWindowTimeoutValue);
	form.description = "Event Window Timeout Value";
	form.vals.push_back(std::to_string(ReadEventWindowTimeoutInterval()));
	return form;
}

// // SERDES Counter Registers
// void CFOLib::CFO_Registers::ClearReceiveByteCount(const CFO_Link_ID& link)
// {
// 	CFO_Register reg;
// 	switch (link)
// 	{
// 		case CFO_Link_0:
// 			reg = CFO_Register_ReceiveByteCountDataLink0;
// 			break;
// 		case CFO_Link_1:
// 			reg = CFO_Register_ReceiveByteCountDataLink1;
// 			break;
// 		case CFO_Link_2:
// 			reg = CFO_Register_ReceiveByteCountDataLink2;
// 			break;
// 		case CFO_Link_3:
// 			reg = CFO_Register_ReceiveByteCountDataLink3;
// 			break;
// 		case CFO_Link_4:
// 			reg = CFO_Register_ReceiveByteCountDataLink4;
// 			break;
// 		case CFO_Link_5:
// 			reg = CFO_Register_ReceiveByteCountDataLink5;
// 			break;
// 		case CFO_Link_6:
// 			reg = CFO_Register_ReceiveByteCountDataLink6;
// 			break;
// 		case CFO_Link_7:
// 			reg = CFO_Register_ReceiveByteCountDataLink7;
// 			break;
// 		default: {
// 			__SS__ << "Illegal link index provided: " << link << __E__;
// 			__SS_THROW__;
// 		}
// 	}
// 	WriteRegister_(0, reg);
// }

// uint32_t CFOLib::CFO_Registers::ReadReceiveByteCount(const CFO_Link_ID& link, std::optional<uint32_t> val)
// {
// 	CFO_Register reg;
// 	switch (link)
// 	{
// 		case CFO_Link_0:
// 			reg = CFO_Register_ReceiveByteCountDataLink0;
// 			break;
// 		case CFO_Link_1:
// 			reg = CFO_Register_ReceiveByteCountDataLink1;
// 			break;
// 		case CFO_Link_2:
// 			reg = CFO_Register_ReceiveByteCountDataLink2;
// 			break;
// 		case CFO_Link_3:
// 			reg = CFO_Register_ReceiveByteCountDataLink3;
// 			break;
// 		case CFO_Link_4:
// 			reg = CFO_Register_ReceiveByteCountDataLink4;
// 			break;
// 		case CFO_Link_5:
// 			reg = CFO_Register_ReceiveByteCountDataLink5;
// 			break;
// 		case CFO_Link_6:
// 			reg = CFO_Register_ReceiveByteCountDataLink6;
// 			break;
// 		case CFO_Link_7:
// 			reg = CFO_Register_ReceiveByteCountDataLink7;
// 			break;
// 		default: {
// 			__SS__ << "Illegal link index provided: " << link << __E__;
// 			__SS_THROW__;
// 		}
// 	}
// 	return val.has_value() ? *val : ReadRegister_(reg);
// }

// void CFOLib::CFO_Registers::ClearReceivePacketCount(const CFO_Link_ID& link)
// {
// 	CFO_Register reg;
// 	switch (link)
// 	{
// 		case CFO_Link_0:
// 			reg = CFO_Register_ReceivePacketCountDataLink0;
// 			break;
// 		case CFO_Link_1:
// 			reg = CFO_Register_ReceivePacketCountDataLink1;
// 			break;
// 		case CFO_Link_2:
// 			reg = CFO_Register_ReceivePacketCountDataLink2;
// 			break;
// 		case CFO_Link_3:
// 			reg = CFO_Register_ReceivePacketCountDataLink3;
// 			break;
// 		case CFO_Link_4:
// 			reg = CFO_Register_ReceivePacketCountDataLink4;
// 			break;
// 		case CFO_Link_5:
// 			reg = CFO_Register_ReceivePacketCountDataLink5;
// 			break;
// 		case CFO_Link_6:
// 			reg = CFO_Register_ReceivePacketCountDataLink6;
// 			break;
// 		case CFO_Link_7:
// 			reg = CFO_Register_ReceivePacketCountDataLink7;
// 			break;
// 		default: {
// 			__SS__ << "Illegal link index provided: " << link << __E__;
// 			__SS_THROW__;
// 		}
// 	}
// 	WriteRegister_(0, reg);
// }

// uint32_t CFOLib::CFO_Registers::ReadReceivePacketCount(const CFO_Link_ID& link, std::optional<uint32_t> val)
// {
// 	CFO_Register reg;
// 	switch (link)
// 	{
// 		case CFO_Link_0:
// 			reg = CFO_Register_ReceivePacketCountDataLink0;
// 			break;
// 		case CFO_Link_1:
// 			reg = CFO_Register_ReceivePacketCountDataLink1;
// 			break;
// 		case CFO_Link_2:
// 			reg = CFO_Register_ReceivePacketCountDataLink2;
// 			break;
// 		case CFO_Link_3:
// 			reg = CFO_Register_ReceivePacketCountDataLink3;
// 			break;
// 		case CFO_Link_4:
// 			reg = CFO_Register_ReceivePacketCountDataLink4;
// 			break;
// 		case CFO_Link_5:
// 			reg = CFO_Register_ReceivePacketCountDataLink5;
// 			break;
// 		case CFO_Link_6:
// 			reg = CFO_Register_ReceivePacketCountDataLink6;
// 			break;
// 		case CFO_Link_7:
// 			reg = CFO_Register_ReceivePacketCountDataLink7;
// 			break;
// 		default: {
// 			__SS__ << "Illegal link index provided: " << link << __E__;
// 			__SS_THROW__;
// 		}
// 	}
// 	return val.has_value() ? *val : ReadRegister_(reg);
// }

// void CFOLib::CFO_Registers::ClearTransmitByteCount(const CFO_Link_ID& link)
// {
// 	CFO_Register reg;
// 	switch (link)
// 	{
// 		case CFO_Link_0:
// 			reg = CFO_Register_TransmitByteCountDataLink0;
// 			break;
// 		case CFO_Link_1:
// 			reg = CFO_Register_TransmitByteCountDataLink1;
// 			break;
// 		case CFO_Link_2:
// 			reg = CFO_Register_TransmitByteCountDataLink2;
// 			break;
// 		case CFO_Link_3:
// 			reg = CFO_Register_TransmitByteCountDataLink3;
// 			break;
// 		case CFO_Link_4:
// 			reg = CFO_Register_TransmitByteCountDataLink4;
// 			break;
// 		case CFO_Link_5:
// 			reg = CFO_Register_TransmitByteCountDataLink5;
// 			break;
// 		case CFO_Link_6:
// 			reg = CFO_Register_TransmitByteCountDataLink6;
// 			break;
// 		case CFO_Link_7:
// 			reg = CFO_Register_TransmitByteCountDataLink7;
// 			break;
// 		default: {
// 			__SS__ << "Illegal link index provided: " << link << __E__;
// 			__SS_THROW__;
// 		}
// 	}
// 	WriteRegister_(0, reg);
// }

// uint32_t CFOLib::CFO_Registers::ReadTransmitByteCount(const CFO_Link_ID& link, std::optional<uint32_t> val)
// {
// 	CFO_Register reg;
// 	switch (link)
// 	{
// 		case CFO_Link_0:
// 			reg = CFO_Register_TransmitByteCountDataLink0;
// 			break;
// 		case CFO_Link_1:
// 			reg = CFO_Register_TransmitByteCountDataLink1;
// 			break;
// 		case CFO_Link_2:
// 			reg = CFO_Register_TransmitByteCountDataLink2;
// 			break;
// 		case CFO_Link_3:
// 			reg = CFO_Register_TransmitByteCountDataLink3;
// 			break;
// 		case CFO_Link_4:
// 			reg = CFO_Register_TransmitByteCountDataLink4;
// 			break;
// 		case CFO_Link_5:
// 			reg = CFO_Register_TransmitByteCountDataLink5;
// 			break;
// 		case CFO_Link_6:
// 			reg = CFO_Register_TransmitByteCountDataLink6;
// 			break;
// 		case CFO_Link_7:
// 			reg = CFO_Register_TransmitByteCountDataLink7;
// 			break;
// 		default: {
// 			__SS__ << "Illegal link index provided: " << link << __E__;
// 			__SS_THROW__;
// 		}
// 	}
// 	return val.has_value() ? *val : ReadRegister_(reg);
// }

// void CFOLib::CFO_Registers::ClearTransmitPacketCount(const CFO_Link_ID& link)
// {
// 	CFO_Register reg;
// 	switch (link)
// 	{
// 		case CFO_Link_0:
// 			reg = CFO_Register_TransmitPacketCountDataLink0;
// 			break;
// 		case CFO_Link_1:
// 			reg = CFO_Register_TransmitPacketCountDataLink1;
// 			break;
// 		case CFO_Link_2:
// 			reg = CFO_Register_TransmitPacketCountDataLink2;
// 			break;
// 		case CFO_Link_3:
// 			reg = CFO_Register_TransmitPacketCountDataLink3;
// 			break;
// 		case CFO_Link_4:
// 			reg = CFO_Register_TransmitPacketCountDataLink4;
// 			break;
// 		case CFO_Link_5:
// 			reg = CFO_Register_TransmitPacketCountDataLink5;
// 			break;
// 		case CFO_Link_6:
// 			reg = CFO_Register_TransmitPacketCountDataLink6;
// 			break;
// 		case CFO_Link_7:
// 			reg = CFO_Register_TransmitPacketCountDataLink7;
// 			break;
// 		default: {
// 			__SS__ << "Illegal link index provided: " << link << __E__;
// 			__SS_THROW__;
// 		}
// 	}
// 	WriteRegister_(0, reg);
// }

// uint32_t CFOLib::CFO_Registers::ReadTransmitPacketCount(const CFO_Link_ID& link, std::optional<uint32_t> val)
// {
// 	CFO_Register reg;
// 	switch (link)
// 	{
// 		case CFO_Link_0:
// 			reg = CFO_Register_TransmitPacketCountDataLink0;
// 			break;
// 		case CFO_Link_1:
// 			reg = CFO_Register_TransmitPacketCountDataLink1;
// 			break;
// 		case CFO_Link_2:
// 			reg = CFO_Register_TransmitPacketCountDataLink2;
// 			break;
// 		case CFO_Link_3:
// 			reg = CFO_Register_TransmitPacketCountDataLink3;
// 			break;
// 		case CFO_Link_4:
// 			reg = CFO_Register_TransmitPacketCountDataLink4;
// 			break;
// 		case CFO_Link_5:
// 			reg = CFO_Register_TransmitPacketCountDataLink5;
// 			break;
// 		case CFO_Link_6:
// 			reg = CFO_Register_TransmitPacketCountDataLink6;
// 			break;
// 		case CFO_Link_7:
// 			reg = CFO_Register_TransmitPacketCountDataLink7;
// 			break;
// 		default: {
// 			__SS__ << "Illegal link index provided: " << link << __E__;
// 			__SS_THROW__;
// 		}
// 	}
// 	return val.has_value() ? *val : ReadRegister_(reg);
// }

uint32_t CFOLib::CFO_Registers::ReadReceiveRF0MarkerCount(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(CFO_Register_ReceiveRF0MarkerCount);
	return static_cast<uint32_t>(data.to_ulong());
}
uint32_t CFOLib::CFO_Registers::ReadTransmitHeartbeatPacketCount(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(CFO_Register_TransmitHeartbeatPacketCount);
	return static_cast<uint32_t>(data.to_ulong());
}
uint32_t CFOLib::CFO_Registers::ReadTransmitEventWindowMarkerCount(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(CFO_Register_TransmitEventWindowMarkerCount);
	return static_cast<uint32_t>(data.to_ulong());
}

DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatReceiveRF0MarkerCount()
{
	auto form = CreateFormatter(CFO_Register_ReceiveRF0MarkerCount);
	form.description = "Receive RF0 Marker Count";
	std::stringstream o;
	o << "0x" << std::hex << ReadReceiveRF0MarkerCount(form.value);
	form.vals.push_back(o.str());
	return form;
}

DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatTransmitHeartbeatPacketCount()
{
	auto form = CreateFormatter(CFO_Register_TransmitHeartbeatPacketCount);
	form.description = "Transmit Heartbeat Packet Count";
	std::stringstream o;
	o << "0x" << std::hex << ReadTransmitHeartbeatPacketCount(form.value);
	form.vals.push_back(o.str());
	return form;
}

DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatTransmitEventWindowMarkerCount()
{
	auto form = CreateFormatter(CFO_Register_TransmitEventWindowMarkerCount);
	form.description = "Transmit Event Window Marker Count";
	std::stringstream o;
	o << "0x" << std::hex << ReadTransmitEventWindowMarkerCount(form.value);
	form.vals.push_back(o.str());
	return form;
}

// DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatReceiveByteCountLink0()
// {
// 	auto form = CreateFormatter(CFO_Register_ReceiveByteCountDataLink0);
// 	form.description = "Receive Byte Count: Link 0";
// 	std::stringstream o;
// 	o << "0x" << std::hex << ReadReceiveByteCount(CFO_Link_0, form.value);
// 	form.vals.push_back(o.str());
// 	return form;
// }

// DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatReceiveByteCountLink1()
// {
// 	auto form = CreateFormatter(CFO_Register_ReceiveByteCountDataLink1);
// 	form.description = "Receive Byte Count: Link 1";
// 	std::stringstream o;
// 	o << "0x" << std::hex << ReadReceiveByteCount(CFO_Link_1, form.value);
// 	form.vals.push_back(o.str());
// 	return form;
// }

// DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatReceiveByteCountLink2()
// {
// 	auto form = CreateFormatter(CFO_Register_ReceiveByteCountDataLink2);
// 	form.description = "Receive Byte Count: Link 2";
// 	std::stringstream o;
// 	o << "0x" << std::hex << ReadReceiveByteCount(CFO_Link_2, form.value);
// 	form.vals.push_back(o.str());
// 	return form;
// }

// DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatReceiveByteCountLink3()
// {
// 	auto form = CreateFormatter(CFO_Register_ReceiveByteCountDataLink3);
// 	form.description = "Receive Byte Count: Link 3";
// 	std::stringstream o;
// 	o << "0x" << std::hex << ReadReceiveByteCount(CFO_Link_3, form.value);
// 	form.vals.push_back(o.str());
// 	return form;
// }

// DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatReceiveByteCountLink4()
// {
// 	auto form = CreateFormatter(CFO_Register_ReceiveByteCountDataLink4);
// 	form.description = "Receive Byte Count: Link 4";
// 	std::stringstream o;
// 	o << "0x" << std::hex << ReadReceiveByteCount(CFO_Link_4, form.value);
// 	form.vals.push_back(o.str());
// 	return form;
// }

// DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatReceiveByteCountLink5()
// {
// 	auto form = CreateFormatter(CFO_Register_ReceiveByteCountDataLink5);
// 	form.description = "Receive Byte Count: Link 5";
// 	std::stringstream o;
// 	o << "0x" << std::hex << ReadReceiveByteCount(CFO_Link_5, form.value);
// 	form.vals.push_back(o.str());
// 	return form;
// }

// DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatReceiveByteCountLink6()
// {
// 	auto form = CreateFormatter(CFO_Register_ReceiveByteCountDataLink6);
// 	form.description = "Receive Byte Count: Link 6";
// 	std::stringstream o;
// 	o << "0x" << std::hex << ReadReceiveByteCount(CFO_Link_6, form.value);
// 	form.vals.push_back(o.str());
// 	return form;
// }

// DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatReceiveByteCountLink7()
// {
// 	auto form = CreateFormatter(CFO_Register_ReceiveByteCountDataLink7);
// 	form.description = "Receive Byte Count: Link 7";
// 	std::stringstream o;
// 	o << "0x" << std::hex << ReadReceiveByteCount(CFO_Link_7, form.value);
// 	form.vals.push_back(o.str());
// 	return form;
// }

// DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatReceivePacketCountLink0()
// {
// 	auto form = CreateFormatter(CFO_Register_ReceivePacketCountDataLink0);
// 	form.description = "Receive Packet Count: Link 0";
// 	std::stringstream o;
// 	o << "0x" << std::hex << ReadReceivePacketCount(CFO_Link_0);
// 	form.vals.push_back(o.str());
// 	return form;
// }

// DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatReceivePacketCountLink1()
// {
// 	auto form = CreateFormatter(CFO_Register_ReceivePacketCountDataLink1);
// 	form.description = "Receive Packet Count: Link 1";
// 	std::stringstream o;
// 	o << "0x" << std::hex << ReadReceivePacketCount(CFO_Link_1);
// 	form.vals.push_back(o.str());
// 	return form;
// }

// DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatReceivePacketCountLink2()
// {
// 	auto form = CreateFormatter(CFO_Register_ReceivePacketCountDataLink2);
// 	form.description = "Receive Packet Count: Link 2";
// 	std::stringstream o;
// 	o << "0x" << std::hex << ReadReceivePacketCount(CFO_Link_2);
// 	form.vals.push_back(o.str());
// 	return form;
// }

// DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatReceivePacketCountLink3()
// {
// 	auto form = CreateFormatter(CFO_Register_ReceivePacketCountDataLink3);
// 	form.description = "Receive Packet Count: Link 3";
// 	std::stringstream o;
// 	o << "0x" << std::hex << ReadReceivePacketCount(CFO_Link_3);
// 	form.vals.push_back(o.str());
// 	return form;
// }

// DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatReceivePacketCountLink4()
// {
// 	auto form = CreateFormatter(CFO_Register_ReceivePacketCountDataLink4);
// 	form.description = "Receive Packet Count: Link 4";
// 	std::stringstream o;
// 	o << "0x" << std::hex << ReadReceivePacketCount(CFO_Link_4);
// 	form.vals.push_back(o.str());
// 	return form;
// }

// DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatReceivePacketCountLink5()
// {
// 	auto form = CreateFormatter(CFO_Register_ReceivePacketCountDataLink5);
// 	form.description = "Receive Packet Count: Link 5";
// 	std::stringstream o;
// 	o << "0x" << std::hex << ReadReceivePacketCount(CFO_Link_5);
// 	form.vals.push_back(o.str());
// 	return form;
// }

// DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatReceivePacketCountLink6()
// {
// 	auto form = CreateFormatter(CFO_Register_ReceivePacketCountDataLink6);
// 	form.description = "Receive Packet Count: Link 6";
// 	std::stringstream o;
// 	o << "0x" << std::hex << ReadReceivePacketCount(CFO_Link_6);
// 	form.vals.push_back(o.str());
// 	return form;
// }

// DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatReceivePacketCountLink7()
// {
// 	auto form = CreateFormatter(CFO_Register_ReceivePacketCountDataLink7);
// 	form.description = "Receive Packet Count: Link 7";
// 	std::stringstream o;
// 	o << "0x" << std::hex << ReadReceivePacketCount(CFO_Link_7);
// 	form.vals.push_back(o.str());
// 	return form;
// }

// DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatTramsitByteCountLink0()
// {
// 	auto form = CreateFormatter(CFO_Register_TransmitByteCountDataLink0);
// 	form.description = "Transmit Byte Count: Link 0";
// 	std::stringstream o;
// 	o << "0x" << std::hex << ReadTransmitByteCount(CFO_Link_0);
// 	form.vals.push_back(o.str());
// 	return form;
// }

// DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatTramsitByteCountLink1()
// {
// 	auto form = CreateFormatter(CFO_Register_TransmitByteCountDataLink1);
// 	form.description = "Transmit Byte Count: Link 1";
// 	std::stringstream o;
// 	o << "0x" << std::hex << ReadTransmitByteCount(CFO_Link_1);
// 	form.vals.push_back(o.str());
// 	return form;
// }

// DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatTramsitByteCountLink2()
// {
// 	auto form = CreateFormatter(CFO_Register_TransmitByteCountDataLink2);
// 	form.description = "Transmit Byte Count: Link 2";
// 	std::stringstream o;
// 	o << "0x" << std::hex << ReadTransmitByteCount(CFO_Link_2);
// 	form.vals.push_back(o.str());
// 	return form;
// }

// DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatTramsitByteCountLink3()
// {
// 	auto form = CreateFormatter(CFO_Register_TransmitByteCountDataLink3);
// 	form.description = "Transmit Byte Count: Link 3";
// 	std::stringstream o;
// 	o << "0x" << std::hex << ReadTransmitByteCount(CFO_Link_3);
// 	form.vals.push_back(o.str());
// 	return form;
// }

// DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatTramsitByteCountLink4()
// {
// 	auto form = CreateFormatter(CFO_Register_TransmitByteCountDataLink4);
// 	form.description = "Transmit Byte Count: Link 4";
// 	std::stringstream o;
// 	o << "0x" << std::hex << ReadTransmitByteCount(CFO_Link_4);
// 	form.vals.push_back(o.str());
// 	return form;
// }

// DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatTramsitByteCountLink5()
// {
// 	auto form = CreateFormatter(CFO_Register_TransmitByteCountDataLink5);
// 	form.description = "Transmit Byte Count: Link 5";
// 	std::stringstream o;
// 	o << "0x" << std::hex << ReadTransmitByteCount(CFO_Link_5);
// 	form.vals.push_back(o.str());
// 	return form;
// }

// DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatTramsitByteCountLink6()
// {
// 	auto form = CreateFormatter(CFO_Register_TransmitByteCountDataLink6);
// 	form.description = "Transmit Byte Count: Link 6";
// 	std::stringstream o;
// 	o << "0x" << std::hex << ReadTransmitByteCount(CFO_Link_6);
// 	form.vals.push_back(o.str());
// 	return form;
// }

// DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatTramsitByteCountLink7()
// {
// 	auto form = CreateFormatter(CFO_Register_TransmitByteCountDataLink7);
// 	form.description = "Transmit Byte Count: Link 7";
// 	std::stringstream o;
// 	o << "0x" << std::hex << ReadTransmitByteCount(CFO_Link_7);
// 	form.vals.push_back(o.str());
// 	return form;
// }

// DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatTransmitPacketCountLink0()
// {
// 	auto form = CreateFormatter(CFO_Register_TransmitPacketCountDataLink0);
// 	form.description = "Transmit Packet Count: Link 0";
// 	std::stringstream o;
// 	o << "0x" << std::hex << ReadTransmitPacketCount(CFO_Link_0);
// 	form.vals.push_back(o.str());
// 	return form;
// }

// DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatTransmitPacketCountLink1()
// {
// 	auto form = CreateFormatter(CFO_Register_TransmitPacketCountDataLink1);
// 	form.description = "Transmit Packet Count: Link 1";
// 	std::stringstream o;
// 	o << "0x" << std::hex << ReadTransmitPacketCount(CFO_Link_1);
// 	form.vals.push_back(o.str());
// 	return form;
// }

// DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatTransmitPacketCountLink2()
// {
// 	auto form = CreateFormatter(CFO_Register_TransmitPacketCountDataLink2);
// 	form.description = "Transmit Packet Count: Link 2";
// 	std::stringstream o;
// 	o << "0x" << std::hex << ReadTransmitPacketCount(CFO_Link_2);
// 	form.vals.push_back(o.str());
// 	return form;
// }

// DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatTransmitPacketCountLink3()
// {
// 	auto form = CreateFormatter(CFO_Register_TransmitPacketCountDataLink3);
// 	form.description = "Transmit Packet Count: Link 3";
// 	std::stringstream o;
// 	o << "0x" << std::hex << ReadTransmitPacketCount(CFO_Link_3);
// 	form.vals.push_back(o.str());
// 	return form;
// }

// DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatTransmitPacketCountLink4()
// {
// 	auto form = CreateFormatter(CFO_Register_TransmitPacketCountDataLink4);
// 	form.description = "Transmit Packet Count: Link 4";
// 	std::stringstream o;
// 	o << "0x" << std::hex << ReadTransmitPacketCount(CFO_Link_4);
// 	form.vals.push_back(o.str());
// 	return form;
// }

// DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatTransmitPacketCountLink5()
// {
// 	auto form = CreateFormatter(CFO_Register_TransmitPacketCountDataLink5);
// 	form.description = "Transmit Packet Count: Link 5";
// 	std::stringstream o;
// 	o << "0x" << std::hex << ReadTransmitPacketCount(CFO_Link_5);
// 	form.vals.push_back(o.str());
// 	return form;
// }

// DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatTransmitPacketCountLink6()
// {
// 	auto form = CreateFormatter(CFO_Register_TransmitPacketCountDataLink6);
// 	form.description = "Transmit Packet Count: Link 6";
// 	std::stringstream o;
// 	o << "0x" << std::hex << ReadTransmitPacketCount(CFO_Link_6);
// 	form.vals.push_back(o.str());
// 	return form;
// }

// DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatTransmitPacketCountLink7()
// {
// 	auto form = CreateFormatter(CFO_Register_TransmitPacketCountDataLink7);
// 	form.description = "Transmit Packet Count: Link 7";
// 	std::stringstream o;
// 	o << "0x" << std::hex << ReadTransmitPacketCount(CFO_Link_7);
// 	form.vals.push_back(o.str());
// 	return form;
// }

// DMA Address Registers
void CFOLib::CFO_Registers::SetDMAWriteStartAddress(const uint32_t& address)
{
	WriteRegister_(address, CFO_Register_DDRMemoryDMAWriteStartAddress);
}

uint32_t CFOLib::CFO_Registers::ReadDMAWriteStartAddress(std::optional<uint32_t> val)
{
	return val.has_value() ? *val : ReadRegister_(CFO_Register_DDRMemoryDMAWriteStartAddress);
}

DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatDMAWriteStartAddress()
{
	auto form = CreateFormatter(CFO_Register_DDRMemoryDMAWriteStartAddress);
	form.description = "DDR Memory DMA Write Start Address";
	form.vals.push_back(std::to_string(ReadDMAWriteStartAddress(form.value)));
	return form;
}

void CFOLib::CFO_Registers::SetDMAReadStartAddress(const uint32_t& address)
{
	WriteRegister_(address, CFO_Register_DDRMemoryDMAReadStartAddress);
}

uint32_t CFOLib::CFO_Registers::ReadDMAReadStartAddress(std::optional<uint32_t> val)
{
	return val.has_value() ? *val : ReadRegister_(CFO_Register_DDRMemoryDMAReadStartAddress);
}

DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatDMAReadStartAddress()
{
	auto form = CreateFormatter(CFO_Register_DDRMemoryDMAWriteStartAddress);
	form.description = "DDR Memory DMA Read Start Address";
	form.vals.push_back(std::to_string(ReadDMAReadStartAddress(form.value)));
	return form;
}

void CFOLib::CFO_Registers::SetDMAReadByteCount(const uint32_t& bytes)
{
	WriteRegister_(bytes, CFO_Register_DDRMemoryDMAReadByteCount);
}

uint32_t CFOLib::CFO_Registers::ReadDMAReadByteCount(std::optional<uint32_t> val) { return val.has_value() ? *val : ReadRegister_(CFO_Register_DDRMemoryDMAReadByteCount); }

DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatDMAReadByteCount()
{
	auto form = CreateFormatter(CFO_Register_DDRMemoryDMAWriteStartAddress);
	form.description = "DDR Memory DMA Read Byte Count/Enable";
	form.vals.push_back(std::to_string(ReadDMAReadByteCount(form.value)));
	return form;
}

void CFOLib::CFO_Registers::SetRunPlanBeamOnBaseAddress(const uint32_t& address)
{
	WriteRegister_(address, CFO_Register_RunPlanBeamOnBaseAddress);
}

uint32_t CFOLib::CFO_Registers::ReadRunPlanBeamOnBaseAddress(std::optional<uint32_t> val) { return val.has_value() ? *val : ReadRegister_(CFO_Register_RunPlanBeamOnBaseAddress); }

DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatRunPlanBeamOnBaseAddress()
{
	auto form = CreateFormatter(CFO_Register_RunPlanBeamOnBaseAddress);
	form.description = "DDR Memory Beam On Base Address";
	form.vals.push_back(std::to_string(ReadRunPlanBeamOnBaseAddress(form.value)));
	return form;
}

void CFOLib::CFO_Registers::SetRunPlanBeamOffBaseAddress(const uint32_t& address)
{
	WriteRegister_(address, CFO_Register_RunPlanBeamOffBaseAddress);
}

uint32_t CFOLib::CFO_Registers::ReadRunPlanBeamOffBaseAddress(std::optional<uint32_t> val)
{
	return val.has_value() ? *val : ReadRegister_(CFO_Register_RunPlanBeamOffBaseAddress);
}

DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatRunPlanBeamOffBaseAddress()
{
	auto form = CreateFormatter(CFO_Register_RunPlanBeamOffBaseAddress);
	form.description = "DDR Memory Beam Off Base Address";
	form.vals.push_back(std::to_string(ReadRunPlanBeamOffBaseAddress()));
	return form;
}

void CFOLib::CFO_Registers::SetRunPlanData(const std::string& inputData, const uint32_t& runPlanBaseAddress)
{
	__COUTTV__(inputData.size());
	__COUT__ << "Writing run plan of size " << inputData.size() << " to base address 0x" << std::hex << std::setw(8) << std::setfill('0') << runPlanBaseAddress << __E__;

	auto dataPtr = reinterpret_cast<const uint8_t*>(&inputData[0]);

	// primary run plan write loop
	WriteRegister_(runPlanBaseAddress, CFO_Register_RunPlan_Address);  // resets run plan BRAM write address
	for (uint32_t l = 0; l < inputData.size(); l += 4)
	{
		// CFO_Register_RunPlan_Address = 0x9314,
		// CFO_Register_RunPlan_Data = 0x9318,
		WriteRegister_(*((uint32_t*)(&(dataPtr[l]))), CFO_Register_RunPlan_Data);  // write data then increment BRAM address
		__COUTT__ << std::hex << std::setw(8) << std::setfill('0') << "addr 0x" << (runPlanBaseAddress + l / 4) << " data 0x" << *((uint32_t*)(&(dataPtr[l]))) << __E__;
	}  // end primary run plan write loop

	__COUT__ << "Verifying run plan of size " << inputData.size() << " to base address 0x" << std::hex << std::setw(8) << std::setfill('0') << runPlanBaseAddress << __E__;
	// now verify run plan w/readback (throws exception on mismatch)
	CompareRunPlanData(inputData, runPlanBaseAddress);

}  // end SetRunPlanData()

/// @brief  Read back run plan data from BRAM and compare to input data for validation.
///		Note that the run plan BRAM read address auto-increments with each read, so we just need to set it once at the beginning of the function.
///		If mismatches has a value, it will be filled with a map of mismatches instead of throwing an exception on the first mismatch.
///		Otherwise (std::nullopt), an exception will be thrown on the first mismatch with details of the failure.
/// @param inputData  The binary run plan data to compare against the readback.
/// @param runPlanBaseAddress  The base BRAM address at which the run plan was written.
/// @param mismatches  Optional reference to a map that will be filled with any mismatches found, where the key is the
///		address of the mismatch and the value is a pair of (expected, actual) data values.
///		Pass std::nullopt (default) to throw an exception on the first mismatch instead of collecting them.
/// @param andMasks  Optional reference to a vector that will be filled with the 48-bit mask values found in
///		AND_MODE_BITS instructions during the readback scan. Pass std::nullopt (default) to skip collection.
/// @param orMasks   Optional reference to a vector that will be filled with the 48-bit mask values found in
///		OR_MODE_BITS instructions during the readback scan. Pass std::nullopt (default) to skip collection.
void CFOLib::CFO_Registers::CompareRunPlanData(const std::string& inputData, const uint32_t& runPlanBaseAddress,
											   std::optional<std::reference_wrapper<std::map<uint32_t /* address */,
																							 std::pair<uint32_t /* expected */,
																									   uint32_t /* actual */>>>>
												   mismatches,
											   std::optional<std::reference_wrapper<std::vector<uint64_t>>> andMasks /* = std::nullopt */,
											   std::optional<std::reference_wrapper<std::vector<uint64_t>>> orMasks /* = std::nullopt */)
{
	auto dataPtr = reinterpret_cast<const uint8_t*>(&inputData[0]);

	WriteRegister_(runPlanBaseAddress, CFO_Register_RunPlan_Address);  // resets run plan BRAM write address
	uint32_t val, lastVal = 0;
	for (uint32_t l = 0; l < inputData.size(); l += 4)
	{
		val = ReadRegister_(CFO_Register_RunPlan_Data);

		__COUTT__ << std::hex << std::setw(8) << std::setfill('0') << "addr 0x" << (runPlanBaseAddress + l / 4) << " data 0x" << *((uint32_t*)(&(dataPtr[l]))) << " =? rdata 0x" << val << __E__;

		if ((andMasks || orMasks) && l % 8 == 4)  // check opcodes
		{
			uint8_t opCode = ((*((uint32_t*)(&(dataPtr[l])))) >> 24) & 0xFF;
			if (andMasks && opCode == (uint8_t)CFOLib::CFO_Compiler::CFO_INSTR::AND_MODE_BITS)
			{
				andMasks->get().push_back((uint64_t)lastVal | ((uint64_t)(val & 0xFFFF) << 32));
				__COUTT__ << "Found AND line = " << l / 8 << " --> 0x" << std::hex << andMasks->get().back() << __E__;
			}
			if (orMasks && opCode == (uint8_t)CFOLib::CFO_Compiler::CFO_INSTR::OR_MODE_BITS)
			{
				orMasks->get().push_back((uint64_t)lastVal | ((uint64_t)(val & 0xFFFF) << 32));
				__COUTT__ << "Found OR line = " << l / 8 << " --> 0x" << std::hex << orMasks->get().back() << __E__;
			}
		}

		if (val != *((uint32_t*)(&(dataPtr[l]))))
		{
			uint8_t opCode = -1;
			if (l % 8 == 4)
				opCode = ((*((uint32_t*)(&(dataPtr[l])))) >> 24) & 0xFF;
			else if (l + 4 < inputData.size())
				opCode = ((*((uint32_t*)(&(dataPtr[l + 4])))) >> 24) & 0xFF;
			if (opCode == (uint8_t)CFOLib::CFO_Compiler::CFO_INSTR::OR_SINGLESHOT_MODE_BITS)
			{
				if (l % 8 == 4 && opCode == (val >> 24))  // if opcode matches, ignore mismatch since single-shot mode bit instructions fire once and then clear 48-bit value
				{
					__COUTT__ << "Ignoring mismatch at line #" << l / 8 << " for OR_SINGLESHOT_MODE_BITS instruction since opcode matches and data auto-clears after firing." << __E__;
					continue;
				}
				if (l % 8 == 0)
				{
					__COUTT__ << "Ignoring mismatch at line #" << l / 8 << " for OR_SINGLESHOT_MODE_BITS instruction since opcode matches for this data and data auto-clears after firing." << __E__;
					continue;
				}
			}

			__SS__ << "Run plan validation failed at line #" << l / 8 << " "
				   << "addr 0x" << std::hex << std::setw(8) << std::setfill('0') << (runPlanBaseAddress + l / 4)
				   << " expected data 0x" << std::hex << std::setw(8) << std::setfill('0') << *((uint32_t*)(&(dataPtr[l])))
				   << " != readback data 0x" << std::hex << std::setw(8) << std::setfill('0') << val << __E__;
			if (!mismatches)
				__SS_THROW__;

			__COUTT__ << ss.str() << __E__;
			mismatches->get()[runPlanBaseAddress + l / 4] = std::make_pair(*((uint32_t*)(&(dataPtr[l]))), val);
		}

		lastVal = val;
	}  // end run plan validation

}  // end CompareRunPlanData()

/// Read current tag bits from two registers and concatenate the full 48-bit value
uint64_t CFOLib::CFO_Registers::ReadRunPlanCurrentTag()
{
	uint64_t val = ReadRegister_(CFO_Register_RunPlan_EventTag0);
	val |= (uint64_t(ReadRegister_(CFO_Register_RunPlan_EventTag1))) << 32;
	return val;
}  // end ReadRunPlanCurrentTag()

DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatRunPlanCurrentTag()
{
	uint64_t currentTag = ReadRunPlanCurrentTag();
	auto form = CreateFormatter(CFO_Register_RunPlan_EventTag1, false /* getValue */);
	form.value = uint32_t(currentTag >> 32);  // show high 32-bits of tag in hex value field
	form.description = "Run Plan Current Tag";
	{
		std::stringstream oss;
		oss << "0x" << std::hex << std::setw(8) << std::setfill('0') << uint32_t(currentTag);  // show hex format low 32-bits of tag
		form.vals.push_back(oss.str());
	}
	{  // show full 64-bit number
		std::stringstream oss;
		oss << std::dec << currentTag << " " << std::scientific << static_cast<double>(currentTag) << " (0x" << std::hex << currentTag << ")";
		form.vals.push_back(oss.str());
	}
	return form;
}  // end FormatRunPlanCurrentTag()

/// Read current mode bits from two registers and concatenate the full 48-bit value
uint64_t CFOLib::CFO_Registers::ReadRunPlanCurrentMode()
{
	uint64_t val = ReadRegister_(CFO_Register_RunPlan_EventMode0);
	val |= (uint64_t(ReadRegister_(CFO_Register_RunPlan_EventMode1))) << 32;
	return val;
}  // end ReadRunPlanCurrentMode()

DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatRunPlanCurrentMode()
{
	auto form = CreateFormatter(CFO_Register_RunPlan_EventMode0, false /* getValue */);
	form.description = "Run Plan Current Mode";

	uint64_t mode = ReadRunPlanCurrentMode();

	// -- parse for each subsystem according docdb 4914 --
	// Mode Packet Definition:
	// Event Mode Byte 1 (Resrv’d Trk)	Event Mode Byte 0 [7:3] 	Pattern Mode [2:1]	Injection Data Source [0]
	// Event Mode Byte 3 (Resrv’d CRV)	Event Mode Byte 2 (Resrv’d Calo) [7:1]	Calo Laser Injection [0]
	// Delivery Ring RF-0 Marker TDC [15:8]	Resrv’d (TEM) [7:6] (STM) [5:4] 	Subrun Handling [3:1]	On-spill Flag [0]
	//
	// The high Event Mode bit, for example bit index 7 of a subsystem’s mode byte (or bit 1 of a subsystem’s 2-bit mode,
	// is considered the active bit.  If set, the corresponding subsystem is expected to record data for that Event Window.

	std::map<std::string, uint16_t> subsystemModeMap;
	subsystemModeMap["Tracker"] = (mode >> 8) & 0xFF;  // bits [7:0] of Event Mode Byte-1
	subsystemModeMap["Calo"] = (mode >> 16) & 0xFF;    // bits [7:0] of Event Mode Byte-2
	subsystemModeMap["CRV"] = (mode >> 32) & 0xFF;     // bits [7:0] of Event Mode Byte-3
	subsystemModeMap["STM"] = (mode >> 36) & 0x3;      // bits [1:0] of Event Mode Byte-4 upper nibble
	subsystemModeMap["ExtMon"] = (mode >> 38) & 0x3;   // bits [3:2] of Event Mode Byte-4 upper nibble

	subsystemModeMap["Tracker active"] = (subsystemModeMap["Tracker"] >> 7) & 1;  // high Event Mode bit is active bit
	subsystemModeMap["Calo active"] = (subsystemModeMap["Calo"] >> 7) & 1;        // high Event Mode bit is active bit
	subsystemModeMap["CRV active"] = (subsystemModeMap["CRV"] >> 7) & 1;          // high Event Mode bit is active bit
	subsystemModeMap["STM active"] = (subsystemModeMap["STM"] >> 1) & 1;          // high Event Mode bit is active bit
	subsystemModeMap["ExtMon active"] = (subsystemModeMap["ExtMon"] >> 1) & 1;    // high Event Mode bit is active bit

	std::stringstream oss;
	oss << "0x" << std::hex << std::setw(8) << std::setfill('0') << ReadRegister_(CFO_Register_RunPlan_EventMode0);
	form.vals.push_back(oss.str());             // show hex format low 32-bits of mode
	form.vals.push_back(std::to_string(mode));  // show decimal value
	form.vals.push_back("");                    // spacer for readability
	for (auto pair : subsystemModeMap)
	{
		if (pair.first.find("active") != std::string::npos) continue;

		std::stringstream o;
		if (pair.first.find("STM") != std::string::npos || pair.first.find("ExtMon") != std::string::npos)
			o << pair.first + ": " << std::string(12 - pair.first.size(), ' ') <<  // pad for alignment
				"b" << std::bitset<2>(pair.second) << std::string(6, ' ') << " (0x" << std::hex << pair.second << ")" << (subsystemModeMap[pair.first + " active"] ? " in" : " out");
		else
			o << pair.first + ": " << std::string(12 - pair.first.size(), ' ') <<  // pad for alignment
				"b" << std::bitset<8>(pair.second) << " (0x" << std::hex << pair.second << ")" << (subsystemModeMap[pair.first + " active"] ? " in" : " out");
		form.vals.push_back(o.str());
	}
	return form;
}  // end FormatRunPlanCurrentMode()

// Run Plan Subrun Event Limit Register
void CFOLib::CFO_Registers::SetRunPlanSubrunEvtLimit(uint32_t limit)
{
	WriteRegister_(limit, CFO_Register_RunPlanSubrunEvtLimit);
}

uint32_t CFOLib::CFO_Registers::ReadRunPlanSubrunEvtLimit(std::optional<uint32_t> val)
{
	return val.has_value() ? *val : ReadRegister_(CFO_Register_RunPlanSubrunEvtLimit);
}

DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatRunPlanSubrunEvtLimit()
{
	auto form = CreateFormatter(CFO_Register_RunPlanSubrunEvtLimit);
	form.description = "Run Plan Subrun Event Limit";
	form.vals.push_back(std::to_string(ReadRunPlanSubrunEvtLimit(form.value)));
	return form;
}

// Run Plan Subrun Prediction Offset Register
void CFOLib::CFO_Registers::SetRunPlanSubrunPredOffset(uint32_t offset)
{
	WriteRegister_(offset, CFO_Register_RunPlanSubrunPredOffset);
}

uint32_t CFOLib::CFO_Registers::ReadRunPlanSubrunPredOffset(std::optional<uint32_t> val)
{
	return val.has_value() ? *val : ReadRegister_(CFO_Register_RunPlanSubrunPredOffset);
}

DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatRunPlanSubrunPredOffset()
{
	auto form = CreateFormatter(CFO_Register_RunPlanSubrunPredOffset);
	form.description = "Run Plan Subrun Prediction Offset";
	form.vals.push_back(std::to_string(ReadRunPlanSubrunPredOffset(form.value)));
	return form;
}

// Firefly CSR Register
bool CFOLib::CFO_Registers::ReadFireflyTXRXPresent(std::optional<uint32_t> val)
{
	std::bitset<32> dataSet = val.has_value() ? *val : ReadRegister_(CFO_Register_FireflyCSRRegister);
	return dataSet[26];
}

bool CFOLib::CFO_Registers::ReadFireflyRXPresent(std::optional<uint32_t> val)
{
	std::bitset<32> dataSet = val.has_value() ? *val : ReadRegister_(CFO_Register_FireflyCSRRegister);
	return dataSet[25];
}

bool CFOLib::CFO_Registers::ReadFireflyTXPresent(std::optional<uint32_t> val)
{
	std::bitset<32> dataSet = val.has_value() ? *val : ReadRegister_(CFO_Register_FireflyCSRRegister);
	return dataSet[24];
}

bool CFOLib::CFO_Registers::ReadFireflyTXRXInterrupt(std::optional<uint32_t> val)
{
	std::bitset<32> dataSet = val.has_value() ? *val : ReadRegister_(CFO_Register_FireflyCSRRegister);
	return dataSet[18];
}

bool CFOLib::CFO_Registers::ReadFireflyRXInterrupt(std::optional<uint32_t> val)
{
	std::bitset<32> dataSet = val.has_value() ? *val : ReadRegister_(CFO_Register_FireflyCSRRegister);
	return dataSet[17];
}

bool CFOLib::CFO_Registers::ReadFireflyTXInterrupt(std::optional<uint32_t> val)
{
	std::bitset<32> dataSet = val.has_value() ? *val : ReadRegister_(CFO_Register_FireflyCSRRegister);
	return dataSet[16];
}

void CFOLib::CFO_Registers::SetFireflyTXRXSelect(bool select)
{
	std::bitset<32> dataSet = ReadRegister_(CFO_Register_FireflyCSRRegister);
	dataSet[10] = select;
	WriteRegister_(dataSet.to_ulong(), CFO_Register_FireflyCSRRegister);
}

void CFOLib::CFO_Registers::SetFireflyRXSelect(bool select)
{
	std::bitset<32> dataSet = ReadRegister_(CFO_Register_FireflyCSRRegister);
	dataSet[9] = select;
	WriteRegister_(dataSet.to_ulong(), CFO_Register_FireflyCSRRegister);
}

void CFOLib::CFO_Registers::SetFireflyTXSelect(bool select)
{
	std::bitset<32> dataSet = ReadRegister_(CFO_Register_FireflyCSRRegister);
	dataSet[8] = select;
	WriteRegister_(dataSet.to_ulong(), CFO_Register_FireflyCSRRegister);
}

bool CFOLib::CFO_Registers::ReadFireflyTXRXSelect(std::optional<uint32_t> val)
{
	std::bitset<32> dataSet = val.has_value() ? *val : ReadRegister_(CFO_Register_FireflyCSRRegister);
	return dataSet[10];
}

bool CFOLib::CFO_Registers::ReadFireflyRXSelect(std::optional<uint32_t> val)
{
	std::bitset<32> dataSet = val.has_value() ? *val : ReadRegister_(CFO_Register_FireflyCSRRegister);
	return dataSet[9];
}

bool CFOLib::CFO_Registers::ReadFireflyTXSelect(std::optional<uint32_t> val)
{
	std::bitset<32> dataSet = val.has_value() ? *val : ReadRegister_(CFO_Register_FireflyCSRRegister);
	return dataSet[8];
}

void CFOLib::CFO_Registers::SetFireflyTXRXReset(bool reset)
{
	std::bitset<32> dataSet = ReadRegister_(CFO_Register_FireflyCSRRegister);
	dataSet[2] = reset;
	WriteRegister_(dataSet.to_ulong(), CFO_Register_FireflyCSRRegister);
}

void CFOLib::CFO_Registers::SetFireflyRXReset(bool reset)
{
	std::bitset<32> dataSet = ReadRegister_(CFO_Register_FireflyCSRRegister);
	dataSet[1] = reset;
	WriteRegister_(dataSet.to_ulong(), CFO_Register_FireflyCSRRegister);
}

void CFOLib::CFO_Registers::SetFireflyTXReset(bool reset)
{
	std::bitset<32> dataSet = ReadRegister_(CFO_Register_FireflyCSRRegister);
	dataSet[0] = reset;
	WriteRegister_(dataSet.to_ulong(), CFO_Register_FireflyCSRRegister);
}

bool CFOLib::CFO_Registers::ReadFireflyTXRXReset(std::optional<uint32_t> val)
{
	std::bitset<32> dataSet = val.has_value() ? *val : ReadRegister_(CFO_Register_FireflyCSRRegister);
	return dataSet[2];
}

bool CFOLib::CFO_Registers::ReadFireflyRXReset(std::optional<uint32_t> val)
{
	std::bitset<32> dataSet = val.has_value() ? *val : ReadRegister_(CFO_Register_FireflyCSRRegister);
	return dataSet[1];
}

bool CFOLib::CFO_Registers::ReadFireflyTXReset(std::optional<uint32_t> val)
{
	std::bitset<32> dataSet = val.has_value() ? *val : ReadRegister_(CFO_Register_FireflyCSRRegister);
	return dataSet[0];
}

DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatFireflyCSR()
{
	auto form = CreateFormatter(CFO_Register_FireflyCSRRegister);
	form.description = "Firefly CSR Register";
	form.vals.push_back("      ([Present, Interrupt, Select, Reset])");
	form.vals.push_back(std::string("TX/RX: [") +
						(ReadFireflyTXRXPresent(form.value) ? "x" : " ") +
						(ReadFireflyTXRXInterrupt(form.value) ? "x" : " ") +
						(ReadFireflyTXRXSelect(form.value) ? "x" : " ") +
						(ReadFireflyTXRXReset(form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("RX:    [") +
						(ReadFireflyRXPresent(form.value) ? "x" : " ") +
						(ReadFireflyRXInterrupt(form.value) ? "x" : " ") +
						(ReadFireflyRXSelect(form.value) ? "x" : " ") +
						(ReadFireflyRXReset(form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("TX:    [") +
						(ReadFireflyTXPresent(form.value) ? "x" : " ") +
						(ReadFireflyTXInterrupt(form.value) ? "x" : " ") +
						(ReadFireflyTXSelect(form.value) ? "x" : " ") +
						(ReadFireflyTXReset(form.value) ? "x" : " ") + "]");

	return form;
}

// SERDES PRBS Control Registers
bool CFOLib::CFO_Registers::ReadSERDESPRBSErrorFlag(const CFO_Link_ID& link, std::optional<uint32_t> val)
{
	CFO_Register reg;
	switch (link)
	{
		case CFO_Link_0:
			reg = CFO_Register_SERDESPRBSControlLink0;
			break;
		case CFO_Link_1:
			reg = CFO_Register_SERDESPRBSControlLink1;
			break;
		case CFO_Link_2:
			reg = CFO_Register_SERDESPRBSControlLink2;
			break;
		case CFO_Link_3:
			reg = CFO_Register_SERDESPRBSControlLink3;
			break;
		case CFO_Link_4:
			reg = CFO_Register_SERDESPRBSControlLink4;
			break;
		case CFO_Link_5:
			reg = CFO_Register_SERDESPRBSControlLink5;
			break;
		case CFO_Link_6:
			reg = CFO_Register_SERDESPRBSControlLink6;
			break;
		case CFO_Link_7:
			reg = CFO_Register_SERDESPRBSControlLink7;
			break;
		default: {
			__SS__ << "Illegal link index provided: " << link << __E__;
			__SS_THROW__;
		}
	}

	std::bitset<32> dataSet = val.has_value() ? *val : ReadRegister_(reg);
	return dataSet[31];
}

uint8_t CFOLib::CFO_Registers::ReadSERDESTXPRBSSEL(const CFO_Link_ID& link, std::optional<uint32_t> val)
{
	CFO_Register reg;
	switch (link)
	{
		case CFO_Link_0:
			reg = CFO_Register_SERDESPRBSControlLink0;
			break;
		case CFO_Link_1:
			reg = CFO_Register_SERDESPRBSControlLink1;
			break;
		case CFO_Link_2:
			reg = CFO_Register_SERDESPRBSControlLink2;
			break;
		case CFO_Link_3:
			reg = CFO_Register_SERDESPRBSControlLink3;
			break;
		case CFO_Link_4:
			reg = CFO_Register_SERDESPRBSControlLink4;
			break;
		case CFO_Link_5:
			reg = CFO_Register_SERDESPRBSControlLink5;
			break;
		case CFO_Link_6:
			reg = CFO_Register_SERDESPRBSControlLink6;
			break;
		case CFO_Link_7:
			reg = CFO_Register_SERDESPRBSControlLink7;
			break;
		default: {
			__SS__ << "Illegal link index provided: " << link << __E__;
			__SS_THROW__;
		}
	}
	auto data = val.has_value() ? *val : ReadRegister_(reg);
	return (data >> 12) & 0xF;
}

void CFOLib::CFO_Registers::SetSERDESTXPRBSSEL(const CFO_Link_ID& link, uint8_t byte)
{
	CFO_Register reg;
	switch (link)
	{
		case CFO_Link_0:
			reg = CFO_Register_SERDESPRBSControlLink0;
			break;
		case CFO_Link_1:
			reg = CFO_Register_SERDESPRBSControlLink1;
			break;
		case CFO_Link_2:
			reg = CFO_Register_SERDESPRBSControlLink2;
			break;
		case CFO_Link_3:
			reg = CFO_Register_SERDESPRBSControlLink3;
			break;
		case CFO_Link_4:
			reg = CFO_Register_SERDESPRBSControlLink4;
			break;
		case CFO_Link_5:
			reg = CFO_Register_SERDESPRBSControlLink5;
			break;
		case CFO_Link_6:
			reg = CFO_Register_SERDESPRBSControlLink6;
			break;
		case CFO_Link_7:
			reg = CFO_Register_SERDESPRBSControlLink7;
			break;
		default: {
			__SS__ << "Illegal link index provided: " << link << __E__;
			__SS_THROW__;
		}
	}
	WriteRegister_(byte, reg);
}

uint8_t CFOLib::CFO_Registers::ReadSERDESRXPRBSSEL(const CFO_Link_ID& link, std::optional<uint32_t> val)
{
	CFO_Register reg;
	switch (link)
	{
		case CFO_Link_0:
			reg = CFO_Register_SERDESPRBSControlLink0;
			break;
		case CFO_Link_1:
			reg = CFO_Register_SERDESPRBSControlLink1;
			break;
		case CFO_Link_2:
			reg = CFO_Register_SERDESPRBSControlLink2;
			break;
		case CFO_Link_3:
			reg = CFO_Register_SERDESPRBSControlLink3;
			break;
		case CFO_Link_4:
			reg = CFO_Register_SERDESPRBSControlLink4;
			break;
		case CFO_Link_5:
			reg = CFO_Register_SERDESPRBSControlLink5;
			break;
		case CFO_Link_6:
			reg = CFO_Register_SERDESPRBSControlLink6;
			break;
		case CFO_Link_7:
			reg = CFO_Register_SERDESPRBSControlLink7;
			break;
		default: {
			__SS__ << "Illegal link index provided: " << link << __E__;
			__SS_THROW__;
		}
	}
	auto data = val.has_value() ? *val : ReadRegister_(reg);
	return (data >> 8) & 0xF;
}

void CFOLib::CFO_Registers::SetSERDESRXPRBSSEL(const CFO_Link_ID& link, uint8_t byte)
{
	CFO_Register reg;
	switch (link)
	{
		case CFO_Link_0:
			reg = CFO_Register_SERDESPRBSControlLink0;
			break;
		case CFO_Link_1:
			reg = CFO_Register_SERDESPRBSControlLink1;
			break;
		case CFO_Link_2:
			reg = CFO_Register_SERDESPRBSControlLink2;
			break;
		case CFO_Link_3:
			reg = CFO_Register_SERDESPRBSControlLink3;
			break;
		case CFO_Link_4:
			reg = CFO_Register_SERDESPRBSControlLink4;
			break;
		case CFO_Link_5:
			reg = CFO_Register_SERDESPRBSControlLink5;
			break;
		case CFO_Link_6:
			reg = CFO_Register_SERDESPRBSControlLink6;
			break;
		case CFO_Link_7:
			reg = CFO_Register_SERDESPRBSControlLink7;
			break;
		default: {
			__SS__ << "Illegal link index provided: " << link << __E__;
			__SS_THROW__;
		}
	}
	WriteRegister_(byte, reg);
}

bool CFOLib::CFO_Registers::ReadSERDESTXPRBSForceError(const CFO_Link_ID& link, std::optional<uint32_t> val)
{
	CFO_Register reg;
	switch (link)
	{
		case CFO_Link_0:
			reg = CFO_Register_SERDESPRBSControlLink0;
			break;
		case CFO_Link_1:
			reg = CFO_Register_SERDESPRBSControlLink1;
			break;
		case CFO_Link_2:
			reg = CFO_Register_SERDESPRBSControlLink2;
			break;
		case CFO_Link_3:
			reg = CFO_Register_SERDESPRBSControlLink3;
			break;
		case CFO_Link_4:
			reg = CFO_Register_SERDESPRBSControlLink4;
			break;
		case CFO_Link_5:
			reg = CFO_Register_SERDESPRBSControlLink5;
			break;
		case CFO_Link_6:
			reg = CFO_Register_SERDESPRBSControlLink6;
			break;
		case CFO_Link_7:
			reg = CFO_Register_SERDESPRBSControlLink7;
			break;
		default: {
			__SS__ << "Illegal link index provided: " << link << __E__;
			__SS_THROW__;
		}
	}
	std::bitset<32> dataSet = val.has_value() ? *val : ReadRegister_(reg);
	return dataSet[1];
}

void CFOLib::CFO_Registers::SetSERDESTXPRBSForceError(const CFO_Link_ID& link, bool flag)
{
	CFO_Register reg;
	switch (link)
	{
		case CFO_Link_0:
			reg = CFO_Register_SERDESPRBSControlLink0;
			break;
		case CFO_Link_1:
			reg = CFO_Register_SERDESPRBSControlLink1;
			break;
		case CFO_Link_2:
			reg = CFO_Register_SERDESPRBSControlLink2;
			break;
		case CFO_Link_3:
			reg = CFO_Register_SERDESPRBSControlLink3;
			break;
		case CFO_Link_4:
			reg = CFO_Register_SERDESPRBSControlLink4;
			break;
		case CFO_Link_5:
			reg = CFO_Register_SERDESPRBSControlLink5;
			break;
		case CFO_Link_6:
			reg = CFO_Register_SERDESPRBSControlLink6;
			break;
		case CFO_Link_7:
			reg = CFO_Register_SERDESPRBSControlLink7;
			break;
		default: {
			__SS__ << "Illegal link index provided: " << link << __E__;
			__SS_THROW__;
		}
	}
	std::bitset<32> dataSet = ReadRegister_(reg);
	dataSet[1] = flag;
	WriteRegister_(dataSet.to_ulong(), reg);
}

void CFOLib::CFO_Registers::ToggleSERDESTXPRBSForceError(const CFO_Link_ID& link)
{
	CFO_Register reg;
	switch (link)
	{
		case CFO_Link_0:
			reg = CFO_Register_SERDESPRBSControlLink0;
			break;
		case CFO_Link_1:
			reg = CFO_Register_SERDESPRBSControlLink1;
			break;
		case CFO_Link_2:
			reg = CFO_Register_SERDESPRBSControlLink2;
			break;
		case CFO_Link_3:
			reg = CFO_Register_SERDESPRBSControlLink3;
			break;
		case CFO_Link_4:
			reg = CFO_Register_SERDESPRBSControlLink4;
			break;
		case CFO_Link_5:
			reg = CFO_Register_SERDESPRBSControlLink5;
			break;
		case CFO_Link_6:
			reg = CFO_Register_SERDESPRBSControlLink6;
			break;
		case CFO_Link_7:
			reg = CFO_Register_SERDESPRBSControlLink7;
			break;
		default: {
			__SS__ << "Illegal link index provided: " << link << __E__;
			__SS_THROW__;
		}
	}
	std::bitset<32> dataSet = ReadRegister_(reg);
	dataSet[1] = !dataSet[1];
	WriteRegister_(dataSet.to_ulong(), reg);
}

bool CFOLib::CFO_Registers::ReadSERDESRXPRBSCountReset(const CFO_Link_ID& link, std::optional<uint32_t> val)
{
	CFO_Register reg;
	switch (link)
	{
		case CFO_Link_0:
			reg = CFO_Register_SERDESPRBSControlLink0;
			break;
		case CFO_Link_1:
			reg = CFO_Register_SERDESPRBSControlLink1;
			break;
		case CFO_Link_2:
			reg = CFO_Register_SERDESPRBSControlLink2;
			break;
		case CFO_Link_3:
			reg = CFO_Register_SERDESPRBSControlLink3;
			break;
		case CFO_Link_4:
			reg = CFO_Register_SERDESPRBSControlLink4;
			break;
		case CFO_Link_5:
			reg = CFO_Register_SERDESPRBSControlLink5;
			break;
		case CFO_Link_6:
			reg = CFO_Register_SERDESPRBSControlLink6;
			break;
		case CFO_Link_7:
			reg = CFO_Register_SERDESPRBSControlLink7;
			break;
		default: {
			__SS__ << "Illegal link index provided: " << link << __E__;
			__SS_THROW__;
		}
	}
	std::bitset<32> dataSet = val.has_value() ? *val : ReadRegister_(reg);
	return dataSet[0];
}

void CFOLib::CFO_Registers::SetSERDESRXPRBSCountReset(const CFO_Link_ID& link, bool flag)
{
	CFO_Register reg;
	switch (link)
	{
		case CFO_Link_0:
			reg = CFO_Register_SERDESPRBSControlLink0;
			break;
		case CFO_Link_1:
			reg = CFO_Register_SERDESPRBSControlLink1;
			break;
		case CFO_Link_2:
			reg = CFO_Register_SERDESPRBSControlLink2;
			break;
		case CFO_Link_3:
			reg = CFO_Register_SERDESPRBSControlLink3;
			break;
		case CFO_Link_4:
			reg = CFO_Register_SERDESPRBSControlLink4;
			break;
		case CFO_Link_5:
			reg = CFO_Register_SERDESPRBSControlLink5;
			break;
		case CFO_Link_6:
			reg = CFO_Register_SERDESPRBSControlLink6;
			break;
		case CFO_Link_7:
			reg = CFO_Register_SERDESPRBSControlLink7;
			break;
		default:
			return;
	}
	std::bitset<32> dataSet = ReadRegister_(reg);
	dataSet[0] = flag;
	WriteRegister_(dataSet.to_ulong(), reg);
}

void CFOLib::CFO_Registers::ToggleSERDESRXPRBSCountReset(const CFO_Link_ID& link)
{
	CFO_Register reg;
	switch (link)
	{
		case CFO_Link_0:
			reg = CFO_Register_SERDESPRBSControlLink0;
			break;
		case CFO_Link_1:
			reg = CFO_Register_SERDESPRBSControlLink1;
			break;
		case CFO_Link_2:
			reg = CFO_Register_SERDESPRBSControlLink2;
			break;
		case CFO_Link_3:
			reg = CFO_Register_SERDESPRBSControlLink3;
			break;
		case CFO_Link_4:
			reg = CFO_Register_SERDESPRBSControlLink4;
			break;
		case CFO_Link_5:
			reg = CFO_Register_SERDESPRBSControlLink5;
			break;
		case CFO_Link_6:
			reg = CFO_Register_SERDESPRBSControlLink6;
			break;
		case CFO_Link_7:
			reg = CFO_Register_SERDESPRBSControlLink7;
			break;
		default:
			return;
	}
	std::bitset<32> dataSet = ReadRegister_(reg);
	dataSet[0] = !dataSet[0];
	WriteRegister_(dataSet.to_ulong(), reg);
}

DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatSERDESPRBSControlLink0()
{
	auto form = CreateFormatter(CFO_Register_SERDESPRBSControlLink0);
	form.description = "SERDES PRBS Control Link 0";
	form.vals.push_back("");  // translation
	std::ostringstream o;
	o << "RX PRBS Error:              " << std::boolalpha << ReadSERDESPRBSErrorFlag(CFO_Link_0, form.value);
	form.vals.push_back(o.str());
	o.flush();
	o.str("");
	o << "TX PRBS Select:             " << std::to_string(ReadSERDESTXPRBSSEL(CFO_Link_0, form.value));
	form.vals.push_back(o.str());
	o.flush();
	o.str("");
	o << "RX PRBS Select:             " << std::to_string(ReadSERDESRXPRBSSEL(CFO_Link_0, form.value));
	form.vals.push_back(o.str());
	o.flush();
	o.str("");
	o << "TX PRBS Enable Force Error: " << std::boolalpha << ReadSERDESTXPRBSForceError(CFO_Link_0, form.value);
	form.vals.push_back(o.str());
	o.flush();
	o.str("");
	o << "RX PRBS Count Reset:        " << std::boolalpha << ReadSERDESRXPRBSCountReset(CFO_Link_0, form.value);
	form.vals.push_back(o.str());
	o.flush();

	return form;
}

DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatSERDESPRBSControlLink1()
{
	auto form = CreateFormatter(CFO_Register_SERDESPRBSControlLink1);
	form.description = "SERDES PRBS Control Link 1";
	form.vals.push_back("");  // translation
	std::ostringstream o;
	o << "RX PRBS Error:              " << std::boolalpha << ReadSERDESPRBSErrorFlag(CFO_Link_1, form.value);
	form.vals.push_back(o.str());
	o.flush();
	o.str("");
	o << "TX PRBS Select:             " << std::to_string(ReadSERDESTXPRBSSEL(CFO_Link_1, form.value));
	form.vals.push_back(o.str());
	o.flush();
	o.str("");
	o << "RX PRBS Select:             " << std::to_string(ReadSERDESRXPRBSSEL(CFO_Link_1, form.value));
	form.vals.push_back(o.str());
	o.flush();
	o.str("");
	o << "TX PRBS Enable Force Error: " << std::boolalpha << ReadSERDESTXPRBSForceError(CFO_Link_1, form.value);
	form.vals.push_back(o.str());
	o.flush();
	o.str("");
	o << "RX PRBS Count Reset:        " << std::boolalpha << ReadSERDESRXPRBSCountReset(CFO_Link_1, form.value);
	form.vals.push_back(o.str());
	o.flush();

	return form;
}

DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatSERDESPRBSControlLink2()
{
	auto form = CreateFormatter(CFO_Register_SERDESPRBSControlLink2);
	form.description = "SERDES PRBS Control Link 2";
	form.vals.push_back("");  // translation
	std::ostringstream o;
	o << "RX PRBS Error:              " << std::boolalpha << ReadSERDESPRBSErrorFlag(CFO_Link_2, form.value);
	form.vals.push_back(o.str());
	o.flush();
	o.str("");
	o << "TX PRBS Select:             " << std::to_string(ReadSERDESTXPRBSSEL(CFO_Link_2, form.value));
	form.vals.push_back(o.str());
	o.flush();
	o.str("");
	o << "RX PRBS Select:             " << std::to_string(ReadSERDESRXPRBSSEL(CFO_Link_2, form.value));
	form.vals.push_back(o.str());
	o.flush();
	o.str("");
	o << "TX PRBS Enable Force Error: " << std::boolalpha << ReadSERDESTXPRBSForceError(CFO_Link_2, form.value);
	form.vals.push_back(o.str());
	o.flush();
	o.str("");
	o << "RX PRBS Count Reset:        " << std::boolalpha << ReadSERDESRXPRBSCountReset(CFO_Link_2, form.value);
	form.vals.push_back(o.str());
	o.flush();

	return form;
}

DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatSERDESPRBSControlLink3()
{
	auto form = CreateFormatter(CFO_Register_SERDESPRBSControlLink3);
	form.description = "SERDES PRBS Control Link 3";
	form.vals.push_back("");  // translation
	std::ostringstream o;
	o << "RX PRBS Error:              " << std::boolalpha << ReadSERDESPRBSErrorFlag(CFO_Link_3, form.value);
	form.vals.push_back(o.str());
	o.flush();
	o.str("");
	o << "TX PRBS Select:             " << std::to_string(ReadSERDESTXPRBSSEL(CFO_Link_3, form.value));
	form.vals.push_back(o.str());
	o.flush();
	o.str("");
	o << "RX PRBS Select:             " << std::to_string(ReadSERDESRXPRBSSEL(CFO_Link_3, form.value));
	form.vals.push_back(o.str());
	o.flush();
	o.str("");
	o << "TX PRBS Enable Force Error: " << std::boolalpha << ReadSERDESTXPRBSForceError(CFO_Link_3, form.value);
	form.vals.push_back(o.str());
	o.flush();
	o.str("");
	o << "RX PRBS Count Reset:        " << std::boolalpha << ReadSERDESRXPRBSCountReset(CFO_Link_3, form.value);
	form.vals.push_back(o.str());
	o.flush();

	return form;
}

DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatSERDESPRBSControlLink4()
{
	auto form = CreateFormatter(CFO_Register_SERDESPRBSControlLink4);
	form.description = "SERDES PRBS Control Link 4";
	form.vals.push_back("");  // translation
	std::ostringstream o;
	o << "RX PRBS Error:              " << std::boolalpha << ReadSERDESPRBSErrorFlag(CFO_Link_4, form.value);
	form.vals.push_back(o.str());
	o.flush();
	o.str("");
	o << "TX PRBS Select:             " << std::to_string(ReadSERDESTXPRBSSEL(CFO_Link_4, form.value));
	form.vals.push_back(o.str());
	o.flush();
	o.str("");
	o << "RX PRBS Select:             " << std::to_string(ReadSERDESRXPRBSSEL(CFO_Link_4, form.value));
	form.vals.push_back(o.str());
	o.flush();
	o.str("");
	o << "TX PRBS Enable Force Error: " << std::boolalpha << ReadSERDESTXPRBSForceError(CFO_Link_4, form.value);
	form.vals.push_back(o.str());
	o.flush();
	o.str("");
	o << "RX PRBS Count Reset:        " << std::boolalpha << ReadSERDESRXPRBSCountReset(CFO_Link_4, form.value);
	form.vals.push_back(o.str());
	o.flush();

	return form;
}

DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatSERDESPRBSControlLink5()
{
	auto form = CreateFormatter(CFO_Register_SERDESPRBSControlLink5);
	form.description = "SERDES PRBS Control Link 5";
	form.vals.push_back("");  // translation
	std::ostringstream o;
	o << "RX PRBS Error:              " << std::boolalpha << ReadSERDESPRBSErrorFlag(CFO_Link_5, form.value);
	form.vals.push_back(o.str());
	o.flush();
	o.str("");
	o << "TX PRBS Select:             " << std::to_string(ReadSERDESTXPRBSSEL(CFO_Link_5, form.value));
	form.vals.push_back(o.str());
	o.flush();
	o.str("");
	o << "RX PRBS Select:             " << std::to_string(ReadSERDESRXPRBSSEL(CFO_Link_5, form.value));
	form.vals.push_back(o.str());
	o.flush();
	o.str("");
	o << "TX PRBS Enable Force Error: " << std::boolalpha << ReadSERDESTXPRBSForceError(CFO_Link_5, form.value);
	form.vals.push_back(o.str());
	o.flush();
	o.str("");
	o << "RX PRBS Count Reset:        " << std::boolalpha << ReadSERDESRXPRBSCountReset(CFO_Link_5, form.value);
	form.vals.push_back(o.str());
	o.flush();

	return form;
}

DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatSERDESPRBSControlLink6()
{
	auto form = CreateFormatter(CFO_Register_SERDESPRBSControlLink6);
	form.description = "SERDES PRBS Control Link 6";
	form.vals.push_back("");  // translation
	std::ostringstream o;
	o << "RX PRBS Error:              " << std::boolalpha << ReadSERDESPRBSErrorFlag(CFO_Link_6, form.value);
	form.vals.push_back(o.str());
	o.flush();
	o.str("");
	o << "TX PRBS Select:             " << std::to_string(ReadSERDESTXPRBSSEL(CFO_Link_6, form.value));
	form.vals.push_back(o.str());
	o.flush();
	o.str("");
	o << "RX PRBS Select:             " << std::to_string(ReadSERDESRXPRBSSEL(CFO_Link_6, form.value));
	form.vals.push_back(o.str());
	o.flush();
	o.str("");
	o << "TX PRBS Enable Force Error: " << std::boolalpha << ReadSERDESTXPRBSForceError(CFO_Link_6, form.value);
	form.vals.push_back(o.str());
	o.flush();
	o.str("");
	o << "RX PRBS Count Reset:        " << std::boolalpha << ReadSERDESRXPRBSCountReset(CFO_Link_6, form.value);
	form.vals.push_back(o.str());
	o.flush();

	return form;
}

DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatSERDESPRBSControlLink7()
{
	auto form = CreateFormatter(CFO_Register_SERDESPRBSControlLink7);
	form.description = "SERDES PRBS Control Link 7";
	form.vals.push_back("");  // translation
	std::ostringstream o;
	o << "RX PRBS Error:              " << std::boolalpha << ReadSERDESPRBSErrorFlag(CFO_Link_7, form.value);
	form.vals.push_back(o.str());
	o.flush();
	o.str("");
	o << "TX PRBS Select:             " << std::to_string(ReadSERDESTXPRBSSEL(CFO_Link_7, form.value));
	form.vals.push_back(o.str());
	o.flush();
	o.str("");
	o << "RX PRBS Select:             " << std::to_string(ReadSERDESRXPRBSSEL(CFO_Link_7, form.value));
	form.vals.push_back(o.str());
	o.flush();
	o.str("");
	o << "TX PRBS Enable Force Error: " << std::boolalpha << ReadSERDESTXPRBSForceError(CFO_Link_7, form.value);
	form.vals.push_back(o.str());
	o.flush();
	o.str("");
	o << "RX PRBS Count Reset:        " << std::boolalpha << ReadSERDESRXPRBSCountReset(CFO_Link_7, form.value);
	form.vals.push_back(o.str());
	o.flush();

	return form;
}

uint32_t CFOLib::CFO_Registers::ReadCableDelayMeasureExponentialCount(std::optional<uint32_t> val)
{
	uint32_t readVal = val.has_value() ? *val : ReadRegister_(CFO_Register_CableDelayControlStatus);
	return (readVal >> 16) & 0xF;  // return 4-bit nibble at bit-16
}  // end ReadCableDelayMeasureExponentialCount()

void CFOLib::CFO_Registers::SetCableDelayMeasureExponentialCount(const uint32_t exponent)
{
	uint32_t val = ReadRegister_(CFO_Register_CableDelayControlStatus);
	val &= ~(0x0F << 16);           // mask off
	val |= (exponent & 0xF) << 16;  // insert 4-bit nibble at bit-16
	WriteRegister_(val, CFO_Register_CableDelayControlStatus);
}  // end SetCableDelayMeasureExponentialCount()

uint32_t CFOLib::CFO_Registers::ReadCableDelayMeasurement(const CFO_Link_ID link, const uint8_t roc, bool& done)
{
	CFOandDTC_Register calcReg = CFO_Register_CableDelayValue_offset;
	calcReg += 0x100 * roc;
	calcReg += 0x4 * link;
	__COUTT__ << "Delay Register 0x" << std::hex << calcReg << __E__;
	uint32_t readVal = ReadRegister_(calcReg);
	done = (readVal >> 31) & 1;
	return readVal & (~(1 << 31));  // return 4-bit nibble at bit-16
}  // end ReadCableDelayMeasureExponentialCount()

// void CFOLib::CFO_Registers::SetCableDelayValue(const CFO_Link_ID& link, const uint32_t delay)
// {
// 	CFO_Register reg;
// 	switch (link)
// 	{
// 		case CFO_Link_0:
// 			reg = CFO_Register_CableDelayValueLink0;
// 			break;
// 		case CFO_Link_1:
// 			reg = CFO_Register_CableDelayValueLink1;
// 			break;
// 		case CFO_Link_2:
// 			reg = CFO_Register_CableDelayValueLink2;
// 			break;
// 		case CFO_Link_3:
// 			reg = CFO_Register_CableDelayValueLink3;
// 			break;
// 		case CFO_Link_4:
// 			reg = CFO_Register_CableDelayValueLink4;
// 			break;
// 		case CFO_Link_5:
// 			reg = CFO_Register_CableDelayValueLink5;
// 			break;
// 		case CFO_Link_6:
// 			reg = CFO_Register_CableDelayValueLink6;
// 			break;
// 		case CFO_Link_7:
// 			reg = CFO_Register_CableDelayValueLink7;
// 			break;
// 		default:
// 			return;
// 	}
// 	WriteRegister_(delay, reg);
// }

// uint32_t CFOLib::CFO_Registers::ReadCableDelayValue(const CFO_Link_ID& link, std::optional<uint32_t> val)
// {
// 	switch (link)
// 	{
// 		case CFO_Link_0:
// 			return val.has_value()?*val:ReadRegister_(CFO_Register_CableDelayValueLink0);
// 		case CFO_Link_1:
// 			return ReadRegister_(CFO_Register_CableDelayValueLink1);
// 		case CFO_Link_2:
// 			return ReadRegister_(CFO_Register_CableDelayValueLink2);
// 		case CFO_Link_3:
// 			return ReadRegister_(CFO_Register_CableDelayValueLink3);
// 		case CFO_Link_4:
// 			return ReadRegister_(CFO_Register_CableDelayValueLink4);
// 		case CFO_Link_5:
// 			return ReadRegister_(CFO_Register_CableDelayValueLink5);
// 		case CFO_Link_6:
// 			return ReadRegister_(CFO_Register_CableDelayValueLink6);
// 		case CFO_Link_7:
// 			return ReadRegister_(CFO_Register_CableDelayValueLink7);
// 		default:
// 			return -1;
// 	}
// }

// DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatCableDelayValueLink0()
// {
// 	auto form = CreateFormatter(CFO_Register_CableDelayValueLink0);
// 	form.description = "Cable Delay Value Link 0";
// 	form.vals.push_back(std::to_string(ReadCableDelayValue(CFO_Link_0)));
// 	return form;
// }

// DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatCableDelayValueLink1()
// {
// 	auto form = CreateFormatter(CFO_Register_CableDelayValueLink1);
// 	form.description = "Cable Delay Value Link 1";
// 	form.vals.push_back(std::to_string(ReadCableDelayValue(CFO_Link_1)));
// 	return form;
// }

// DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatCableDelayValueLink2()
// {
// 	auto form = CreateFormatter(CFO_Register_CableDelayValueLink2);
// 	form.description = "Cable Delay Value Link 2";
// 	form.vals.push_back(std::to_string(ReadCableDelayValue(CFO_Link_2)));
// 	return form;
// }

// DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatCableDelayValueLink3()
// {
// 	auto form = CreateFormatter(CFO_Register_CableDelayValueLink3);
// 	form.description = "Cable Delay Value Link 3";
// 	form.vals.push_back(std::to_string(ReadCableDelayValue(CFO_Link_3)));
// 	return form;
// }

// DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatCableDelayValueLink4()
// {
// 	auto form = CreateFormatter(CFO_Register_CableDelayValueLink4);
// 	form.description = "Cable Delay Value Link 4";
// 	form.vals.push_back(std::to_string(ReadCableDelayValue(CFO_Link_4)));
// 	return form;
// }

// DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatCableDelayValueLink5()
// {
// 	auto form = CreateFormatter(CFO_Register_CableDelayValueLink5);
// 	form.description = "Cable Delay Value Link 5";
// 	form.vals.push_back(std::to_string(ReadCableDelayValue(CFO_Link_5)));
// 	return form;
// }

// DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatCableDelayValueLink6()
// {
// 	auto form = CreateFormatter(CFO_Register_CableDelayValueLink6);
// 	form.description = "Cable Delay Value Link 6";
// 	form.vals.push_back(std::to_string(ReadCableDelayValue(CFO_Link_6)));
// 	return form;
// }

// DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatCableDelayValueLink7()
// {
// 	auto form = CreateFormatter(CFO_Register_CableDelayValueLink7);
// 	form.description = "Cable Delay Value Link 7";
// 	form.vals.push_back(std::to_string(ReadCableDelayValue(CFO_Link_7)));
// 	return form;
// }

// void CFOLib::CFO_Registers::ResetDelayRegister()
// {
// 	WriteRegister_(0, CFO_Register_CableDelayControlStatus);
// }

// bool CFOLib::CFO_Registers::ReadDelayMeasureError(const CFO_Link_ID& link, std::optional<uint32_t> val)
// {
// 	std::bitset<32> dataSet = val.has_value()?*val:ReadRegister_(CFO_Register_CableDelayControlStatus);
// 	return dataSet[24 + link];
// }

// bool CFOLib::CFO_Registers::ReadDelayExternalLoopbackEnable(std::optional<uint32_t> val)
// {
// 	std::bitset<32> dataSet = val.has_value()?*val:ReadRegister_(CFO_Register_CableDelayControlStatus);
// 	return dataSet[16];
// }

// void CFOLib::CFO_Registers::SetDelayExternalLoopbackEnable(bool value)
// {
// 	std::bitset<32> dataSet = ReadRegister_(CFO_Register_CableDelayControlStatus);
// 	dataSet[16] = value;
// 	WriteRegister_(dataSet.to_ulong(), CFO_Register_CableDelayControlStatus);
// }

// void CFOLib::CFO_Registers::EnableDelayMeasureMode(const CFO_Link_ID& link)
// {
// 	std::bitset<32> dataSet = ReadRegister_(CFO_Register_CableDelayControlStatus);
// 	dataSet[8 + link] = 1;
// 	WriteRegister_(dataSet.to_ulong(), CFO_Register_CableDelayControlStatus);
// }

// void CFOLib::CFO_Registers::DisableDelayMeasureMode(const CFO_Link_ID& link)
// {
// 	std::bitset<32> dataSet = ReadRegister_(CFO_Register_CableDelayControlStatus);
// 	dataSet[8 + link] = 0;
// 	WriteRegister_(dataSet.to_ulong(), CFO_Register_CableDelayControlStatus);
// }

// bool CFOLib::CFO_Registers::ReadDelayMeasureMode(const CFO_Link_ID& link, std::optional<uint32_t> val)
// {
// 	std::bitset<32> dataSet = val.has_value()?*val:ReadRegister_(CFO_Register_CableDelayControlStatus);
// 	return dataSet[8 + link];
// }

// void CFOLib::CFO_Registers::EnableDelayMeasureNow(const CFO_Link_ID& link)
// {
// 	std::bitset<32> dataSet = ReadRegister_(CFO_Register_CableDelayControlStatus);
// 	dataSet[link] = 1;
// 	WriteRegister_(dataSet.to_ulong(), CFO_Register_CableDelayControlStatus);
// }

// void CFOLib::CFO_Registers::DisableDelayMeasureNow(const CFO_Link_ID& link)
// {
// 	std::bitset<32> dataSet = ReadRegister_(CFO_Register_CableDelayControlStatus);
// 	dataSet[link] = 0;
// 	WriteRegister_(dataSet.to_ulong(), CFO_Register_CableDelayControlStatus);
// }

// bool CFOLib::CFO_Registers::ReadDelayMeasureNow(const CFO_Link_ID& link, std::optional<uint32_t> val)
// {
// 	std::bitset<32> dataSet = val.has_value()?*val:ReadRegister_(CFO_Register_CableDelayControlStatus);
// 	return dataSet[link];
// }

// DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatCableDelayControl()
// {
// 	auto form = CreateFormatter(CFO_Register_CableDelayControlStatus);
// 	form.description = "Cabel Delay Control and Status";
// 	form.vals.push_back("[ x = 1 (hi) ]"); //translation
// 	form.vals.push_back(std::string("Delay External Loopback Enable: [") +
// 						(ReadDelayExternalLoopbackEnable(form.value) ? "x" : " ") + std::string("]"));
// 	form.vals.push_back(std::string("Delay Measure Flags:           ([Error, Enabled, Now])"));
// 	for (auto r : CFO_Links)
// 		form.vals.push_back(std::string("Link ") + std::to_string(r) + ":                         [" +
// 							(ReadDelayMeasureError(r, form.value) ? "x" : " ") + "," +
// 							(ReadDelayMeasureMode(r, form.value) ? "x" : " ") + "," +
// 							(ReadDelayMeasureNow(r, form.value) ? "x" : " ") + "]");
// 	return form;
// }

// FPGA PROM Program Status Register
bool CFOLib::CFO_Registers::ReadFPGAPROMProgramFIFOFull(std::optional<uint32_t> val)
{
	std::bitset<32> dataSet = val.has_value() ? *val : ReadRegister_(CFO_Register_FPGAPROMProgramStatus);
	return dataSet[1];
}

bool CFOLib::CFO_Registers::ReadFPGAPROMReady(std::optional<uint32_t> val)
{
	std::bitset<32> dataSet = val.has_value() ? *val : ReadRegister_(CFO_Register_FPGAPROMProgramStatus);
	return dataSet[0];
}

DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatFPGAPROMProgramStatus()
{
	auto form = CreateFormatter(CFO_Register_FPGAPROMProgramStatus);
	form.description = "FPGA PROM Program Status";
	form.vals.push_back("[ x = 1 (hi) ]");  // translation
	form.vals.push_back(std::string("FPGA PROM Program FIFO Full: [") + (ReadFPGAPROMProgramFIFOFull(form.value) ? "x" : " ") +
						"]");
	form.vals.push_back(std::string("FPGA PROM Ready:             [") + (ReadFPGAPROMReady(form.value) ? "x" : " ") + "]");
	return form;
}

// FPGA Core Access Register
void CFOLib::CFO_Registers::ReloadFPGAFirmware()
{
	WriteRegister_(0xFFFFFFFF, CFO_Register_FPGACoreAccess);
	while (ReadFPGACoreAccessFIFOFull())
	{
		usleep(10);
	}
	WriteRegister_(0xAA995566, CFO_Register_FPGACoreAccess);
	while (ReadFPGACoreAccessFIFOFull())
	{
		usleep(10);
	}
	WriteRegister_(0x20000000, CFO_Register_FPGACoreAccess);
	while (ReadFPGACoreAccessFIFOFull())
	{
		usleep(10);
	}
	WriteRegister_(0x30020001, CFO_Register_FPGACoreAccess);
	while (ReadFPGACoreAccessFIFOFull())
	{
		usleep(10);
	}
	WriteRegister_(0x00000000, CFO_Register_FPGACoreAccess);
	while (ReadFPGACoreAccessFIFOFull())
	{
		usleep(10);
	}
	WriteRegister_(0x30008001, CFO_Register_FPGACoreAccess);
	while (ReadFPGACoreAccessFIFOFull())
	{
		usleep(10);
	}
	WriteRegister_(0x0000000F, CFO_Register_FPGACoreAccess);
	while (ReadFPGACoreAccessFIFOFull())
	{
		usleep(10);
	}
	WriteRegister_(0x20000000, CFO_Register_FPGACoreAccess);
}

bool CFOLib::CFO_Registers::ReadFPGACoreAccessFIFOFull(std::optional<uint32_t> val)
{
	std::bitset<32> dataSet = val.has_value() ? *val : ReadRegister_(CFO_Register_FPGACoreAccess);
	return dataSet[1];
}

bool CFOLib::CFO_Registers::ReadFPGACoreAccessFIFOEmpty(std::optional<uint32_t> val)
{
	std::bitset<32> dataSet = val.has_value() ? *val : ReadRegister_(CFO_Register_FPGACoreAccess);
	return dataSet[0];
}

DTCLib::RegisterFormatter CFOLib::CFO_Registers::FormatFPGACoreAccess()
{
	auto form = CreateFormatter(CFO_Register_FPGACoreAccess);
	form.description = "FPGA Core Access";
	form.vals.push_back("[ x = 1 (hi) ]");  // translation
	form.vals.push_back(std::string("FPGA Core Access FIFO Full:  [") + (ReadFPGACoreAccessFIFOFull(form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("FPGA Core Access FIFO Empty: [") + (ReadFPGACoreAccessFIFOEmpty(form.value) ? "x" : " ") +
						"]");

	return form;
}

// Oscillator Programming (DDR and SERDES)
bool CFOLib::CFO_Registers::SetNewOscillatorFrequency(double targetFrequency)
{
	auto currentFrequency = ReadSERDESOscillatorFrequency();
	auto currentProgram = ReadSERDESOscillatorParameters();
	__COUT__ << "Target Frequency: " << targetFrequency << ", Current Frequency: " << currentFrequency
			 << ", Current Program: " << std::showbase << std::hex << currentProgram;

	// Check if targetFrequency is essentially the same as the current frequency...
	if (fabs(currentFrequency - targetFrequency) < targetFrequency * 30 / 1000000)
	{
		__COUT_INFO__ << "New frequency and old frequency are within 30 ppm of each other, not reprogramming!";
		return false;
	}

	auto newParameters = CalculateFrequencyForProgramming_(targetFrequency, currentFrequency, currentProgram);
	if (newParameters == 0)
	{
		__COUT_WARN__ << "New program calculated as 0! Check parameters!";
		return false;
	}
	SetSERDESOscillatorParameters(newParameters);
	SetSERDESOscillatorFrequency(targetFrequency);
	return true;
}

void CFOLib::CFO_Registers::DisableLinks()
{
	__COUT_INFO__ << "CFO disable serdes transmit and receive";
	WriteRegister_(0, CFOandDTC_Register_LinkEnable);
}

void CFOLib::CFO_Registers::DisableAllOutputs()
{
	__COUT_INFO__ << "CFO disable Event Start character output";
	WriteRegister_(0, CFOandDTC_Register_Control);

	__COUT_INFO__ << "CFO disable serdes transmit and receive";
	WriteRegister_(0, CFOandDTC_Register_LinkEnable);

	__COUT_INFO__ << "CFO turn off Event Windows";
	// WriteRegister_(0,CFO_Register_EventWindowEmulatorIntervalTime);
	DisableBeamOnMode(CFOLib::CFO_Link_ID::CFO_Link_ALL);
	DisableBeamOffMode(CFOLib::CFO_Link_ID::CFO_Link_ALL);

	// LEGACY: register 0x9154 repurposed as RunPlanSubrunEvtLimit; no longer writing ClockMarkerIntervalCount
	// __COUT_INFO__ << "CFO turn off 40MHz marker interval";
	// WriteRegister_(0, CFO_Register_ClockMarkerIntervalCount);
}

// Private Functions
bool CFOLib::CFO_Registers::NeedToVerifyRegisterWrite_(const CFOandDTC_Register& address)
{
	switch (address)
	{
		// list all registers that do no need to be verified
		case CFO_Register_RunPlan_Data:
			return false;
		default:;
	}
	return true;
}  // end NeedToVerifyRegisterWrite_()

void CFOLib::CFO_Registers::VerifyRegisterWrite_(const CFOandDTC_Register& address, uint32_t readbackValue, uint32_t dataToWrite)
{
	// verify register readback
	if (1)
	{
		switch (address)  // handle special register checks by masking of DONT-CARE bits, or else check full 32 bits
		{
			//---------- CFO only registers
			case CFO_Register_JitterAttenuatorCSR:  // 0x9500 bit-0 is reset, input select bit-5:4, bit-8 is LOL, bit-11:9
													// (input LOS).. only check input select bits
				dataToWrite &= (3 << 4);
				readbackValue &= (3 << 4);
				break;

			default:;  // do direct comparison of each bit
		}              // end readback verification address case handling

		if (readbackValue != dataToWrite)
		{
			try
			{
				__SS__ << "Write check mismatch - "
					   << "write value 0x" << std::setw(8) << std::setfill('0') << std::setprecision(8) << std::hex << static_cast<uint32_t>(dataToWrite)
					   << " to register 0x" << std::setw(4) << std::setfill('0') << std::setprecision(4) << std::hex << static_cast<uint32_t>(address) << "... read back 0x" << std::setw(8) << std::setfill('0') << std::setprecision(8) << std::hex << static_cast<uint32_t>(readbackValue) << std::endl
					   << std::endl
					   << "If you do not understand this error, try checking the CFO firmware version: " << ReadDesignDate() << std::endl;
				__SS_ONLY_THROW__;
			}
			catch (const std::runtime_error& e)
			{
				std::stringstream ss;
				ss << e.what();
				ss << "\n\nThe stack trace is as follows:\n"
				   << otsStyleStackTrace() << __E__;  // artdaq::debug::getStackTraceCollector().print_stacktrace() << __E__;
				__SS_THROW__;
			}

			// __COUT_ERR__ << ss.str();
			// throw DTC_IOErrorException(ss.str());
			// __FE_COUT_ERR__ << ss.str();
		}

	}  // end verify register readback
}

int CFOLib::CFO_Registers::DecodeHighSpeedDivider_(int input)
{
	switch (input)
	{
		case 0:
			return 4;
		case 1:
			return 5;
		case 2:
			return 6;
		case 3:
			return 7;
		case 5:
			return 9;
		case 7:
			return 11;
		default:
			return -1;
	}
}

int CFOLib::CFO_Registers::EncodeHighSpeedDivider_(int input)
{
	switch (input)
	{
		case 4:
			return 0;
		case 5:
			return 1;
		case 6:
			return 2;
		case 7:
			return 3;
		case 9:
			return 5;
		case 11:
			return 7;
		default:
			return -1;
	}
}

int CFOLib::CFO_Registers::EncodeOutputDivider_(int input)
{
	if (input == 1) return 0;
	int temp = input / 2;
	return (temp * 2) - 1;
}

uint64_t CFOLib::CFO_Registers::CalculateFrequencyForProgramming_(double targetFrequency, double currentFrequency,
																  uint64_t currentProgram)
{
	TLOG(TLVL_CalculateFreq) << __COUT_HDR__ << "CalculateFrequencyForProgramming: targetFrequency=" << targetFrequency << ", currentFrequency=" << currentFrequency
							 << ", currentProgram=" << std::showbase << std::hex << static_cast<unsigned long long>(currentProgram);
	auto currentHighSpeedDivider = DecodeHighSpeedDivider_((currentProgram >> 45) & 0x7);
	auto currentOutputDivider = DecodeOutputDivider_((currentProgram >> 38) & 0x7F);
	auto currentRFREQ = DecodeRFREQ_(currentProgram & 0x3FFFFFFFFF);
	TLOG(TLVL_CalculateFreq) << __COUT_HDR__ << "CalculateFrequencyForProgramming: Current HSDIV=" << currentHighSpeedDivider << ", N1=" << currentOutputDivider << ", RFREQ=" << currentRFREQ;
	const auto minFreq = 4850000000;  // Hz
	const auto maxFreq = 5670000000;  // Hz

	auto fXTAL = currentFrequency * currentHighSpeedDivider * currentOutputDivider / currentRFREQ;
	TLOG(TLVL_CalculateFreq) << __COUT_HDR__ << "CalculateFrequencyForProgramming: fXTAL=" << fXTAL;

	std::vector<int> hsdiv_values = {11, 9, 7, 6, 5, 4};
	std::vector<std::pair<int, double>> parameter_values;
	for (auto hsdiv : hsdiv_values)
	{
		auto minN = minFreq / (targetFrequency * hsdiv);
		if (minN > 128) break;

		auto thisN = 2;
		if (minN < 1) thisN = 1;
		while (thisN < minN)
		{
			thisN += 2;
		}
		auto fdco_new = hsdiv * thisN * targetFrequency;
		TLOG(TLVL_CalculateFreq) << __COUT_HDR__ << "CalculateFrequencyForProgramming: Adding solution: HSDIV=" << hsdiv << ", N1=" << thisN << ", fdco_new=" << fdco_new;
		parameter_values.push_back(std::make_pair(thisN, fdco_new));
	}

	auto counter = -1;
	auto newHighSpeedDivider = 0;
	auto newOutputDivider = 0;
	auto newRFREQ = 0.0;

	for (auto values : parameter_values)
	{
		++counter;
		if (values.second > maxFreq) continue;

		newHighSpeedDivider = hsdiv_values[counter];
		newOutputDivider = values.first;
		newRFREQ = values.second / fXTAL;
		break;
	}
	TLOG(TLVL_CalculateFreq) << __COUT_HDR__ << "CalculateFrequencyForProgramming: New Program: HSDIV=" << newHighSpeedDivider << ", N1=" << newOutputDivider << ", RFREQ=" << newRFREQ;

	if (EncodeHighSpeedDivider_(newHighSpeedDivider) == -1)
	{
		__COUT_ERR__ << "ERROR: CalculateFrequencyForProgramming: Invalid HSDIV " << newHighSpeedDivider << "!";
		return 0;
	}
	if (newOutputDivider > 128 || newOutputDivider < 0)
	{
		__COUT_ERR__ << "ERROR: CalculateFrequencyForProgramming: Invalid N1 " << newOutputDivider << "!";
		return 0;
	}
	if (newRFREQ <= 0)
	{
		__COUT_ERR__ << "ERROR: CalculateFrequencyForProgramming: Invalid RFREQ " << newRFREQ << "!";
		return 0;
	}

	auto output = (static_cast<uint64_t>(EncodeHighSpeedDivider_(newHighSpeedDivider)) << 45) +
				  (static_cast<uint64_t>(EncodeOutputDivider_(newOutputDivider)) << 38) + EncodeRFREQ_(newRFREQ);
	TLOG(TLVL_CalculateFreq) << __COUT_HDR__ << "CalculateFrequencyForProgramming: New Program: " << std::showbase << std::hex << static_cast<unsigned long long>(output);
	return output;
}
