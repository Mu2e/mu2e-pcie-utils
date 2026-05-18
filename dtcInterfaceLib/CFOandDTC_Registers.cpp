#include "CFOandDTC_Registers.h"

#include <assert.h>
#include <unistd.h>
#include <chrono>
#include <cmath>
#include <iomanip>  // std::setw, std::setfill
#include <sstream>  // Convert uint to hex string

#include "TRACE/tracemf.h"

#include "dtcInterfaceLib/otsStyleCoutMacros.h"

#undef __COUT_HDR__
#define __COUT_HDR__ "core-CFOandDTC " << device_.getDeviceUID() << ": "
#define TLVL_ResetDTC TLVL_DEBUG + 5
#define TLVL_AutogenDRP TLVL_DEBUG + 6
#define TLVL_SERDESReset TLVL_DEBUG + 7
#define TLVL_CalculateFreq TLVL_DEBUG + 8
#define TLVL_ReadRegister TLVL_DEBUG + 20

using namespace DTCLib;

// struct CFOandDTC_Register {
enum CFOandDTC_Register : uint16_t
{
	DTCLIB_COMMON_REGISTERS
};

// }; //end CFOandDTC_Register enum

/// <summary>
/// Construct an instance of the core CFO-and-DTC register map
/// </summary>
DTCLib::CFOandDTC_Registers::CFOandDTC_Registers()
	: device_()
{
}  // end constructor()

/// <summary>
/// CFOandDTC_Registers destructor
/// </summary>
DTCLib::CFOandDTC_Registers::~CFOandDTC_Registers()
{
	TLOG(TLVL_TRACE) << "DESTRUCTOR";
}  // end destructor()

/// <summary>
/// Perform a register dump
/// </summary>
/// <param name="width">Printable width of description fields</param>
/// <returns>String containing all registers, with their human-readable representations</returns>
std::string DTCLib::CFOandDTC_Registers::FormattedRegDump(int width,
														  const std::vector<std::function<RegisterFormatter()>>& regVec)
{
	formatterWidth_ = width - 27 - 65;
	if (formatterWidth_ < 28)
		formatterWidth_ = 28;

	std::string divider(formatterWidth_ + 27, '=');

	std::string spaces(formatterWidth_ - 4 - 9, ' ');
	std::ostringstream o;
	o << "Register Dump: " << std::endl;
	o << divider << std::endl;
	{  // move address to right-align with values
		std::string placeholder = "";
		placeholder.resize(formatterWidth_ - 7, ' ');
		o << placeholder;
	}
	o << "Address | Hex Value  |\nRegister Name " << spaces << "| (Translation)" << std::endl;
	{  // move RegisterFormatter to right-align with values
		std::string placeholder = "";
		placeholder.resize(formatterWidth_, ' ');
		o << placeholder << " | Decorated Values" << std::endl;
	}
	for (auto i : regVec)
	{
		o << divider << std::endl;
		o << i();
	}
	return o.str();
}  // end FormattedRegDump()

// Desgin Version/Date Registers
/// <summary>
/// Read the design version
/// </summary>
/// <returns>Design version, in VersionNumber_Date format</returns>
std::string DTCLib::CFOandDTC_Registers::ReadDesignVersion() { return  // ReadDesignVersionNumber() + "_" +
															   ReadDesignDate() + ", Vivado Version: " + ReadVivadoVersionNumber() +
															   ", Link Speed: " + ReadDesignLinkSpeed() + "_" + ReadDesignType(); }

/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::CFOandDTC_Registers::FormatDesignVersion()
{
	auto form = CreateFormatter(CFOandDTC_Register_DesignVersion);
	form.description = "DTC Firmware Design Version";
	form.vals.push_back(std::string("Version Number:                [") + ReadDesignVersionNumber(form.value) + "]");
	form.vals.push_back(std::string("Link Speed CFO link/ROC links: [") + ReadDesignLinkSpeed(form.value) + "]");
	form.vals.push_back(std::string("Design Type (C=CFO, D=DTC):    [") + ReadDesignType(form.value) + "]");
	return form;
}

/// <summary>
/// Read the modification date of the DTC firmware
/// </summary>
/// <returns>Design date in MON/DD/20YY HH:00 format</returns>
std::string DTCLib::CFOandDTC_Registers::ReadDesignDate(std::optional<uint32_t> val)
{
	auto readData = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_DesignDate);
	std::ostringstream o;
	bool isCFO = false;
	std::vector<std::string> months({"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"});
	size_t mon = ((readData >> 20) & 0xF) * 10 + ((readData >> 16) & 0xF);
	if (mon - 1 >= months.size())
	{
		__SS__ << "Invalid register read for firmware design date month index: " + std::to_string(mon) << " in hex read-data 0x" << std::hex << readData << ". If the value is 0 or 165, this likely means the PCIe is not initialized and perhaps a PCIe reset of the Linux system would fix the issue.";
		__SS_THROW__;
	}
	if (((readData >> 28) & 0xF) == 0xA)
		o << "SIM-";
	else if (((readData >> 28) & 0xF) == 0xC)
	{
		o << "CFO-";
		isCFO = true;
	}
	else if (((readData >> 28) & 0xF) == 0xD)
		o << "DTC-";
	else if (((readData >> 28) & 0xF) == 0xE)
		o << "CRVDTC-";
	else if (((readData >> 28) & 0xF) == 0xF)
		o << "TEMP-";
	o << months[mon - 1] << "/" << ((readData >> 12) & 0xF) << ((readData >> 8) & 0xF) << "/20" <<
		// ((readData>>28)&0xF) <<
		(((readData >> 28) & 0xF) == 0x1 ?  // if 1, then old!!! firmware
			 (10 + ((readData >> 24) & 0xF))
										 : (20 + ((readData >> 24) & 0xF)))
	  << " " <<  // year 2020 + hex nibble at bit-24
		((readData >> 4) & 0x7) << ((readData >> 0) & 0xF) << ":00   ";
	if (isCFO)
		o << "8-Links";
	else
		o << (((readData >> 7) & 0x1) ? "6-ROC" : "3-ROC");
	o << "  raw-data: 0x" << std::hex << readData;
	return o.str();
}

/// <summary>
/// Checks if is CRV DTC version
/// </summary>
/// <returns>returns true if the firmware is CRV DTC version, false otherwise</returns>
bool DTCLib::CFOandDTC_Registers::isCRVDTCDesignFlavour(std::optional<uint32_t> val)
{
	auto readData = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_DesignDate);
	return ((readData >> 28) & 0xF) == 0xe;
}

/// <summary>
/// Checks if is nonCRV DTC version
/// </summary>
/// <returns>returns true if the firmware is nonCRV DTC version, false otherwise</returns>
bool DTCLib::CFOandDTC_Registers::isNonCRVDTCDesignFlavour(std::optional<uint32_t> val)
{
	auto readData = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_DesignDate);
	return ((readData >> 28) & 0xF) == 0xd;
}

/// <summary>
/// Checks if is CFO version
/// </summary>
/// <returns>returns true if the firmware is CFO version, false otherwise</returns>
bool DTCLib::CFOandDTC_Registers::isCFODesignFlavour(std::optional<uint32_t> val)
{
	auto readData = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_DesignDate);
	return ((readData >> 28) & 0xF) == 0xc;
}

/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::CFOandDTC_Registers::FormatDesignDate()
{
	auto form = CreateFormatter(CFOandDTC_Register_DesignDate);
	form.description = "DTC Firmware Design Date";
	form.vals.push_back(ReadDesignDate(form.value));
	return form;
}

/// <summary>
/// Read the design version number
/// </summary>
/// <returns>The design version number, in vMM.mm format</returns>
std::string DTCLib::CFOandDTC_Registers::ReadDesignVersionNumber(std::optional<uint32_t> val)
{
	auto data = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_DesignVersion);
	int minor = data & 0xFF;
	int major = (data & 0xFF00) >> 8;

	__COUT_INFO__ << "Kernel Driver Version: " << GetDevice()->get_driver_version() << "Kernel Driver Version: " << GetDevice()->get_driver_version();
	return "v" + std::to_string(major) + "." + std::to_string(minor);
}  // end ReadDesignVersionNumber()

/// <summary>
/// Read the design link speed
/// </summary>
/// <returns>The design link speed, in hex character format</returns>
std::string DTCLib::CFOandDTC_Registers::ReadDesignLinkSpeed(std::optional<uint32_t> val)
{
	auto data = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_DesignVersion);
	int cfoLinkSpeed = (data >> 28) & 0xF;  // bit28: 3, 4, or 5 Indicates Firmware configured for 3.125, 4.0, or 4.8 Gbps on CFO Link SERDES
	int rocLinkSpeed = (data >> 24) & 0xF;  // bit24: 3, 4, or 5 Indicates Firmware configured for 3.125, 4.0, or 4.8 Gbps on ROC Link SERDES

	std::string cfoLinkSpeedStr = "?Gbps";
	if (cfoLinkSpeed == 3)
		cfoLinkSpeedStr = "3.125Gbps";
	else if (cfoLinkSpeed == 4)
		cfoLinkSpeedStr = "4.0Gbps";
	else if (cfoLinkSpeed == 5)
		cfoLinkSpeedStr = "4.8Gbps";

	std::string rocLinkSpeedStr = "?Gbps";
	if (rocLinkSpeed == 3)
		rocLinkSpeedStr = "3.125Gbps";
	else if (rocLinkSpeed == 4)
		rocLinkSpeedStr = "4.0Gbps";
	else if (rocLinkSpeed == 5)
		rocLinkSpeedStr = "4.8Gbps";

	return cfoLinkSpeedStr + "/" + rocLinkSpeedStr;
}  // end ReadDesignLinkSpeed()

/// <summary>
/// Read the design type
/// </summary>
/// <returns>The design link type, 0xC for CFO and 0xD for DTC </returns>
std::string DTCLib::CFOandDTC_Registers::ReadDesignType(std::optional<uint32_t> val)
{
	auto data = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_DesignVersion);
	int type = (data >> 16) & 0xF;                           // 0xC for CFO and 0xD for DTC
	return (type == 0xC ? "C" : (type == 0xD ? "D" : "U"));  // U for unknown!
}  // end ReadDesignLinkSpeed()

/// <summary>
/// Read the Vivado Version Number
/// </summary>
/// <returns>The Vivado Version number</returns>
std::string DTCLib::CFOandDTC_Registers::ReadVivadoVersionNumber(std::optional<uint32_t> val)
{
	auto data = val.has_value() ? (*val) : ReadRegister_(CFOandDTC_Register_VivadoVersion);
	std::ostringstream o;
	int yearHex = (data & 0xFFFF0000) >> 16;
	auto year = ((yearHex & 0xF000) >> 12) * 1000 + ((yearHex & 0xF00) >> 8) * 100 + ((yearHex & 0xF0) >> 4) * 10 +
				(yearHex & 0xF);
	int versionHex = (data & 0xFFFF);
	auto version = ((versionHex & 0xF000) >> 12) * 1000 + ((versionHex & 0xF00) >> 8) * 100 +
				   ((versionHex & 0xF0) >> 4) * 10 + (versionHex & 0xF);
	o << std::setfill('0') << std::setw(4) << year << "-" << version;
	// std::cout << o.str() << std::endl;
	return o.str();
}

/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::CFOandDTC_Registers::FormatVivadoVersion()
{
	auto form = CreateFormatter(CFOandDTC_Register_VivadoVersion);
	form.description = "Firmware Project Vivado Version";
	form.vals.push_back(ReadVivadoVersionNumber(form.value));
	return form;
}

/// <summary>
/// Perform a Soft Reset
/// </summary>
void DTCLib::CFOandDTC_Registers::SoftReset()
{
	TLOG(TLVL_ResetDTC) << __COUT_HDR__ << "Soft Reset start";
	device_.resetSpyHasOccurred();  // allow spy() to fire again after a reset
	std::bitset<32> data = ReadRegister_(CFOandDTC_Register_Control);
	data[31] = 1;  // set Soft Reset bit
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_Control);
	// NOTE: newer DTC versions (roughly > August 2023), self-clear the reset bit
	// NOTE: newer CFO versions (roughly > December 2023), do not self-clear the reset bit
	data = ReadRegister_(CFOandDTC_Register_Control);  // re-read in case there are defaults applied at Soft Reset moment
	data[31] = 0;                                      // clear Soft Reset bit
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_Control);
}

/// <summary>
/// Read the Soft Reset control Bit
/// </summary>
/// <returns>True if the Soft Reset is currently resetting, false otherwise</returns>
bool DTCLib::CFOandDTC_Registers::ReadSoftReset(std::optional<uint32_t> val)
{
	std::bitset<32> dataSet = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_Control);
	return dataSet[31];
}

/// <summary>
/// Set the SERDES Global Reset bit to true, and wait for the reset to complete
/// </summary>
void DTCLib::CFOandDTC_Registers::ResetSERDES()
{
	std::bitset<32> data = ReadRegister_(CFOandDTC_Register_Control);
	data[8] = 1;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_Control);
	usleep(1000);
	// does not self clear!
	//  while (ReadResetSERDES())
	//  {
	//  	usleep(1000);
	//  }
	data[8] = 0;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_Control);
}

/// <summary>
/// Read the SERDES Global Reset bit
/// </summary>
/// <returns>Whether a SERDES global reset is in progress</returns>
bool DTCLib::CFOandDTC_Registers::ReadResetSERDES(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_Control);
	return data[8];
}

/// <summary>
/// Read the Punched Clock Enable bit
/// </summary>
/// <returns>Whether punched clocks are enabled</returns>
bool DTCLib::CFOandDTC_Registers::ReadPunchEnable(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_Control);
	return data[9];
}

void DTCLib::CFOandDTC_Registers::SetPunchEnable()
{
	std::bitset<32> data = ReadRegister_(CFOandDTC_Register_Control);
	data[9] = 1;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_Control);
}

void DTCLib::CFOandDTC_Registers::ClearPunchEnable()
{
	std::bitset<32> data = ReadRegister_(CFOandDTC_Register_Control);
	data[9] = 0;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_Control);
}

/// <summary>
/// Runs the Loopback test of the CFO Emulator, inside the DTC, and broadcasts loopback markers to all ROCs.
/// </summary>
void DTCLib::CFOandDTC_Registers::RunCableDelayLoopbackTest()
{
	std::bitset<32> data = ReadRegister_(CFOandDTC_Register_Control);
	data[3] = 0;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_Control);
	data[3] = 1;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_Control);
}  // end RunCableDelayLoopbackTest()

/// <summary>
/// Perform a Hard Reset
/// </summary>
void DTCLib::CFOandDTC_Registers::HardReset()
{
	TLOG(TLVL_ResetDTC) << __COUT_HDR__ << "Soft Reset start";
	std::bitset<32> data = ReadRegister_(CFOandDTC_Register_Control);
	data[0] = 1;  // set Hard Reset bit
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_Control);
	// NOTE: only implemented in newer CFO and DTC versions (roughly > December 2023), and is implemented as a self-clear the reset bit
	//  data = ReadRegister_(CFOandDTC_Register_Control); //re-read in case there are defaults applied at Soft Reset moment
	//  data[0] = 0;  // clear Hard Reset bit
	//  WriteRegister_(data.to_ulong(), CFOandDTC_Register_Control);
}

/// <summary>
/// Read the Hard Reset control Bit
/// </summary>
/// <returns>True if the Soft Reset is currently resetting, false otherwise</returns>
bool DTCLib::CFOandDTC_Registers::ReadHardReset(std::optional<uint32_t> val)
{
	std::bitset<32> dataSet = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_Control);
	return dataSet[0];
}

/// <summary>
/// Clear the Control Register
/// </summary>
void DTCLib::CFOandDTC_Registers::ClearControlRegister()
{
	WriteRegister_(0, CFOandDTC_Register_Control);
}

/// <summary>
/// Read the FPGA On-die Temperature sensor
/// </summary>
/// <returns>Temperature of the FGPA (deg. C)</returns>
double DTCLib::CFOandDTC_Registers::ReadFPGATemperature(std::optional<uint32_t> val)
{
	auto data = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_FPGA_Temperature);

	return ((data * 503.975) / 4096.0) - 273.15;
}

/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::CFOandDTC_Registers::FormatFPGATemperature()
{
	auto form = CreateFormatter(CFOandDTC_Register_FPGA_Temperature);
	form.description = "FPGA Temperature";
	form.vals.push_back(std::to_string(ReadFPGATemperature(form.value)) + " C");
	return form;
}

/// <summary>
/// Read the FPGA VCC INT Voltage (Nominal is 1.0 V)
/// </summary>
/// <returns>FPGA VCC INT Voltage (V)</returns>
double DTCLib::CFOandDTC_Registers::ReadFPGAVCCINTVoltage(std::optional<uint32_t> val)
{
	auto data = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_FPGA_VCCINT);
	return (data / 4095.0) * 3.0;
}

/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::CFOandDTC_Registers::FormatFPGAVCCINT()
{
	auto form = CreateFormatter(CFOandDTC_Register_FPGA_VCCINT);
	form.description = "FPGA VCC INT";
	form.vals.push_back(std::to_string(ReadFPGAVCCINTVoltage(form.value)) + " V");
	return form;
}

/// <summary>
/// Read the FPGA VCC AUX Voltage (Nominal is 1.8 V)
/// </summary>
/// <returns>FPGA VCC AUX Voltage (V)</returns>
double DTCLib::CFOandDTC_Registers::ReadFPGAVCCAUXVoltage(std::optional<uint32_t> val)
{
	auto data = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_FPGA_VCCAUX);
	return (data / 4095.0) * 3.0;
}

/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::CFOandDTC_Registers::FormatFPGAVCCAUX()
{
	auto form = CreateFormatter(CFOandDTC_Register_FPGA_VCCAUX);
	form.description = "FPGA VCC AUX";
	form.vals.push_back(std::to_string(ReadFPGAVCCAUXVoltage(form.value)) + " V");
	return form;
}

/// <summary>
/// Read the FPGA VCC BRAM Voltage (Nominal 1.0 V)
/// </summary>
/// <returns>FPGA VCC BRAM Voltage (V)</returns>
double DTCLib::CFOandDTC_Registers::ReadFPGAVCCBRAMVoltage(std::optional<uint32_t> val)
{
	auto data = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_FPGA_VCCBRAM);
	return (data / 4095.0) * 3.0;
}

/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::CFOandDTC_Registers::FormatFPGAVCCBRAM()
{
	auto form = CreateFormatter(CFOandDTC_Register_FPGA_VCCBRAM);
	form.description = "FPGA VCC BRAM";
	form.vals.push_back(std::to_string(ReadFPGAVCCBRAMVoltage(form.value)) + " V");
	return form;
}

/// <summary>
/// Read the value of the FPGA Die Temperature Alarm bit
/// </summary>
/// <returns>Value of the bit</returns>
bool DTCLib::CFOandDTC_Registers::ReadFPGADieTemperatureAlarm(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_FPGA_MonitorAlarm);
	return data[8];
}

/// <summary>
/// Reset the FPGA Die Temperature Alarm bit
/// </summary>
void DTCLib::CFOandDTC_Registers::ResetFPGADieTemperatureAlarm(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_FPGA_MonitorAlarm);
	data[8] = 1;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_FPGA_MonitorAlarm);
}

/// <summary>
/// Read the FPGA Alarms bit (OR of VCC and User Temperature alarm bits)
/// </summary>
/// <returns>Value of the bit</returns>
bool DTCLib::CFOandDTC_Registers::ReadFPGAAlarms(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_FPGA_MonitorAlarm);
	return data[7];
}

/// <summary>
/// Reset the FPGA Alarms bit
/// </summary>
void DTCLib::CFOandDTC_Registers::ResetFPGAAlarms(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_FPGA_MonitorAlarm);
	data[7] = 1;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_FPGA_MonitorAlarm);
}

/// <summary>
/// Read the VCC BRAM Alarm bit
/// </summary>
/// <returns>Value of the bit</returns>
bool DTCLib::CFOandDTC_Registers::ReadVCCBRAMAlarm(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_FPGA_MonitorAlarm);
	return data[3];
}

/// <summary>
/// Reset the VCC BRAM Alarm bit
/// </summary>
void DTCLib::CFOandDTC_Registers::ResetVCCBRAMAlarm(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_FPGA_MonitorAlarm);
	data[3] = 1;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_FPGA_MonitorAlarm);
}

/// <summary>
/// Read the VCC AUX Alarm bit
/// </summary>
/// <returns>Value of the bit</returns>
bool DTCLib::CFOandDTC_Registers::ReadVCCAUXAlarm(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_FPGA_MonitorAlarm);
	return data[2];
}

/// <summary>
/// Reset the VCC AUX Alarm bit
/// </summary>
void DTCLib::CFOandDTC_Registers::ResetVCCAUXAlarm(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_FPGA_MonitorAlarm);
	data[2] = 1;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_FPGA_MonitorAlarm);
}

/// <summary>
/// Read the VCC INT Alarm bit
/// </summary>
/// <returns>Value of the bit</returns>
bool DTCLib::CFOandDTC_Registers::ReadVCCINTAlarm(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_FPGA_MonitorAlarm);
	return data[1];
}

/// <summary>
/// Reset the VCC INT Alarm bit
/// </summary>
void DTCLib::CFOandDTC_Registers::ResetVCCINTAlarm(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_FPGA_MonitorAlarm);
	data[1] = 1;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_FPGA_MonitorAlarm);
}

/// <summary>
/// Read the FPGA User Temperature Alarm bit
/// </summary>
/// <returns>Value of the bit</returns>
bool DTCLib::CFOandDTC_Registers::ReadFPGAUserTemperatureAlarm(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_FPGA_MonitorAlarm);
	return data[0];
}

/// <summary>
/// Reset the FPGA User Temperature Alarm bit
/// </summary>
void DTCLib::CFOandDTC_Registers::ResetFPGAUserTemperatureAlarm(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_FPGA_MonitorAlarm);
	data[0] = 1;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_FPGA_MonitorAlarm);
}

/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::CFOandDTC_Registers::FormatFPGAAlarms()
{
	auto form = CreateFormatter(CFOandDTC_Register_FPGA_MonitorAlarm);
	form.description = "FPGA Monitor Alarm";
	form.vals.push_back("[ x = 1 (hi) ]");  // translation
	form.vals.push_back(std::string("FPGA Die Temperature Alarm:  [") + (ReadFPGADieTemperatureAlarm(form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("FPGA Alarms OR:              [") + (ReadFPGAAlarms(form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("VCC BRAM Alarm:              [") + (ReadVCCBRAMAlarm(form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("VCC AUX Alarm:               [") + (ReadVCCAUXAlarm(form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("VCC INT Alarm:               [") + (ReadVCCINTAlarm(form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("FPGA User Temperature Alarm: [") + (ReadFPGAUserTemperatureAlarm(form.value) ? "x" : " ") + "]");

	return form;
}

/// <summary>
/// Formats the device time alive register value as human-readable seconds
/// </summary>
/// <returns>RegisterFormatter object containing time alive information</returns>
DTCLib::RegisterFormatter DTCLib::CFOandDTC_Registers::FormatDeviceTimeAlive()
{
	auto form = CreateFormatter(CFOandDTC_Register_TimeAlive);
	form.description = "Device Time Alive";
	std::stringstream oss;
	// Resolution is 1/250MHz * 2^18 per LSB = 2^18 / 250e6 seconds per count
	double seconds = static_cast<double>(form.value) * (static_cast<double>(1 << 18) / 250e6);
	int totalSeconds = static_cast<int>(seconds);
	int days = totalSeconds / 86400;
	int hours = (totalSeconds / 3600) % 24;
	int minutes = (totalSeconds / 60) % 60;
	double secs = seconds - days * 86400 - hours * 3600 - minutes * 60;
	oss << std::setfill('0') << days << " days " << std::setw(2) << hours << ":"
		<< std::setw(2) << minutes << ":" << std::setw(6) << std::fixed << std::setprecision(3)
		<< secs << " (Firmware Version: " << ReadDesignDate() << ")";
	form.vals.push_back(oss.str());
	return form;
}

/// <summary>
/// Formats the device hash of node + pcie index
/// </summary>
/// <returns>RegisterFormatter object containing hash information</returns>
DTCLib::RegisterFormatter DTCLib::CFOandDTC_Registers::FormatDeviceHash()
{
	auto form = CreateFormatter(CFOandDTC_Register_Scratch);
	form.description = "Device Hash";
	std::stringstream oss;
	oss << "Hash of Device Node and PCIe Index: 0x" << std::hex << mu2e_host_hash(device_.getDeviceIndex(), nullptr /* this host */) << std::dec;
	form.vals.push_back(oss.str());
	return form;
}

// Private Functions
uint32_t DTCLib::CFOandDTC_Registers::WriteRegister_(uint32_t dataToWrite, const CFOandDTC_Register& address)
{
	auto retry = 3;
	int errorCode;
	bool needToVerify = NeedToVerifyRegisterWrite_(address);
	uint32_t readbackValue = needToVerify ? -1 : dataToWrite;
	do
	{
		if (needToVerify)
			errorCode = device_.write_register_checked(address, 100, dataToWrite, &readbackValue);
		else
			errorCode = device_.write_register(address, 100, dataToWrite);

		--retry;
	} while (retry > 0 && errorCode != 0);
	if (errorCode != 0)
	{
		__SS__ << "Error writing register 0x" << std::hex << static_cast<uint32_t>(address) << " " << errorCode;
		// CFO_DTC_TLOG(TLVL_ERROR) << ss.str();
		__SS_THROW__;
		// throw DTC_IOErrorException(errorCode);
	}

	if (TTEST(1))
	{  // trace seems to ignore the std::setfill, so using stringstream
		std::stringstream o;
		o << "write value 0x" << std::setw(8) << std::setfill('0') << std::setprecision(8) << std::hex << static_cast<uint32_t>(dataToWrite)
		  << " to register 0x" << std::setw(4) << std::setfill('0') << std::setprecision(4) << std::hex << static_cast<uint32_t>(address) << " needToVerify=" << needToVerify << std::endl;
		if (needToVerify)
			o << " ... readback 0x" << std::setw(8) << std::setfill('0') << std::setprecision(8) << std::hex << static_cast<uint32_t>(readbackValue) << std::endl;
		__COUTT__ << o.str();
	}

	// verify register readback
	if (needToVerify)
	{
		// readbackValue = ReadRegister_(address); //already read above by write_register_checked!
		if (!CFOandDTCVerifyRegisterWrite_(address, readbackValue, dataToWrite))  // first check if it is a core register, i.e. CFOandDTC*
			VerifyRegisterWrite_(address, readbackValue, dataToWrite);            // virtual function call
		return readbackValue;
	}  // end verify register readback

	return readbackValue;
}  // end WriteRegister_()

// return false if not a core register, i.e. CFOandDTC*
bool DTCLib::CFOandDTC_Registers::CFOandDTCVerifyRegisterWrite_(const CFOandDTC_Register& address, uint32_t readbackValue, uint32_t dataToWrite)
{
	// verify register readback
	if (1)
	{
		bool isCoreRegister = false;

		switch (address)  // handle special register checks by masking of DONT-CARE bits, or else check full 32 bits
		{
			//---------- CFO and DTC registers
			case CFOandDTC_Register_SERDESClock_IICBusLow:      // lowest 16-bits are the I2C read value. So ignore in write validation
			case CFOandDTC_Register_FireflyRX_IICBusConfigLow:  // lowest 16-bits are the I2C read value. So ignore in write validation
				dataToWrite &= 0xffff0000;
				readbackValue &= 0xffff0000;
				isCoreRegister = true;
				break;
			case CFOandDTC_Register_SERDESClock_IICBusHigh:  // this is an I2C register, it clears bit-0 when transaction finishes
			{
				int i = -1;
				while ((dataToWrite & 0x1) && (readbackValue & 0x1))  // wait for I2C to clear...
				{
					readbackValue = ReadRegister_(address);
					usleep(100);
					if ((++i % 10) == 9)
						__COUT__ << "I2C waited " << i + 1 << " times..." << std::endl;
				}
			}
				dataToWrite &= ~1;
				readbackValue &= ~1;
				isCoreRegister = true;
				break;
			case CFOandDTC_Register_FireFlyControlStatus:  // only 10:8 and 2:0 defined in Firefly control reg
				dataToWrite &= (0x7 << 8) | (0x7 << 0);
				readbackValue &= (0x7 << 8) | (0x7 << 0);
				isCoreRegister = true;
				break;
			// case : // upper 16-bits are part of I2C operation. So ignore in write validation
			// 	dataToWrite		&= 0x0000ffff;
			// 	readbackValue 	&= 0x0000ffff;
			// 	isCoreRegister = true;
			// 	break;
			case CFOandDTC_Register_Control:  // bit 0 and 31 are reset bits, and self-clear (effectively, write only), also bit-25 self-resets (mig reset)
				if ((dataToWrite >> 0) & 1)
					return true;  // ignore check if hard reset bit-0 high, because factory defaults are not maintained here
				dataToWrite &= 0x7dffffff;
				readbackValue &= 0x7dffffff;
				isCoreRegister = true;
				break;

			default:;  // do direct comparison of each bit
		}              // end readback verification address case handling

		if (isCoreRegister && readbackValue != dataToWrite)
		{
			try
			{
				__SS__ << "Write check mismatch - "
					   << "write value 0x" << std::setw(8) << std::setfill('0') << std::setprecision(8) << std::hex << static_cast<uint32_t>(dataToWrite)
					   << " to register 0x" << std::setw(4) << std::setfill('0') << std::setprecision(4) << std::hex << static_cast<uint32_t>(address) << "... read back 0x" << std::setw(8) << std::setfill('0') << std::setprecision(8) << std::hex << static_cast<uint32_t>(readbackValue) << std::endl
					   << std::endl
					   << "If you do not understand this error, try checking the DTC firmware version: " << ReadDesignDate() << std::endl;
				__SS_ONLY_THROW__;
			}
			catch (const std::runtime_error& e)
			{
				std::stringstream ss;
				ss << e.what();
				ss << "\n\nThe stack trace is as follows:\n"
				   << otsStyleStackTrace() << __E__;
				__SS_THROW__;
			}
		}
		return isCoreRegister;
	}  // end verify register readback
	return false;

}  // end VerifyRegisterWrite_()

uint32_t DTCLib::CFOandDTC_Registers::ReadRegister_(const CFOandDTC_Register& address)
{
	auto retry = 3;
	int errorCode;
	uint32_t data;
	do
	{
		errorCode = device_.read_register(address, 100, &data);
		--retry;
	} while (retry > 0 && errorCode != 0);
	if (errorCode != 0)
	{
		__SS__ << "Error reading register 0x" << std::setw(4) << std::setfill('0') << std::setprecision(4) << std::hex << static_cast<uint32_t>(address) << " " << errorCode;
		__SS_THROW__;
		// throw DTC_IOErrorException(errorCode);
	}

	if (address == CFOandDTC_Register_Control && data == uint32_t(-1))
	{
		__SS__ << "Invalid register read for the Control Register: " << data << ". If the value is all 1s (4294967295), this likely means the FPGA-PCIe interface in not initialized and perhaps a PCIe reset of the linux system would fix the issue.";
		__SS_THROW__;
	}

	if (TTEST(1))
	{  // trace seems to ignore the std::setfill, so using stringstream
		std::stringstream o;
		o << "read value 0x" << std::setw(8) << std::setfill('0') << std::setprecision(8) << std::hex << static_cast<uint32_t>(data)
		  << " from register 0x" << std::setw(4) << std::setfill('0') << std::setprecision(4) << std::hex << static_cast<uint32_t>(address) << std::endl;
		__COUTT__ << o.str();
	}

	return data;
}  // end ReadRegister_()

//========================================================================
bool DTCLib::CFOandDTC_Registers::GetBit_(const CFOandDTC_Register& address, size_t bit)
{
	if (bit > 31)
	{
		__SS__ << "Cannot read bit " << bit << ", as it is out of range";
		__SS_THROW__;
		// throw std::out_of_range("Cannot read bit " + std::to_string(bit) + ", as it is out of range");
	}
	return std::bitset<32>(ReadRegister_(address))[bit];
}

//========================================================================
void DTCLib::CFOandDTC_Registers::SetBit_(const CFOandDTC_Register& address, size_t bit, bool value)
{
	if (bit > 31)
	{
		__SS__ << "Cannot set bit " << bit << ", as it is out of range";
		__SS_THROW__;
		// throw std::out_of_range("Cannot set bit " + std::to_string(bit) + ", as it is out of range");
	}
	auto regVal = std::bitset<32>(ReadRegister_(address));
	regVal[bit] = value;
	WriteRegister_(regVal.to_ulong(), address);
}

//========================================================================
// Jitter Attenuator CSR Register
/// <summary>
/// Read the value of the Jitter Attenuator Select
/// </summary>
/// <returns>Jitter Attenuator Select value</returns>
std::bitset<2> DTCLib::CFOandDTC_Registers::ReadJitterAttenuatorSelect(CFOandDTC_Register JAreg, std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(JAreg);
	std::bitset<2> output;
	output[0] = data[4];
	output[1] = data[5];
	return output;
}

//========================================================================
/// <summary>
/// Set the Jitter Attenuator Select bits. JA reset only needed after a power cycle
/// </summary>
/// <param name="data">Value to set</param>
void DTCLib::CFOandDTC_Registers::SetJitterAttenuatorSelect(CFOandDTC_Register JAreg, std::bitset<2> data, bool alsoResetJA /* = false */)
{
	__COUT__ << "JA select " << data << " = " << (data == 0 ? "CFO control link" : (data == 1 ? "RTF copper clock" : (data == 2 ? "FPGA FMC" : "undefined source!")));
	;
	std::bitset<32> regdata = ReadRegister_(JAreg);

	// attempt detection if already locked with same input mux select, early exit
	// form.vals.push_back(std::string("JA in Reset:   [") + (data[0] ? "YES" : "No") + "]");
	// form.vals.push_back(std::string("JA Loss-of-Lock:   [") + (data[8] ? "Not Locked" : "LOCKED") + "]");
	// form.vals.push_back(std::string("JA Input-0 Upstream Control Link Rx Recovered Clock:   [") + (data[9] ? "Missing" : "OK") + "]");
	// form.vals.push_back(std::string("JA Input-1 RJ45 Upstream Rx Clock:   [") + (data[10] ? "Missing" : "OK") + "]");
	// form.vals.push_back(std::string("JA Input-2 Timing Card Selectable, SFP+ or FPGA, Input Clock:   [") + (data[11] ? "Missing" : "OK") + "]");
	if (regdata[0] == 0 && regdata[8] == 0 && regdata[4] == data[0] && regdata[5] == data[1])
	{
		__COUT__ << "JA already locked with selected input " << data;
		return;
	}
	regdata[4] = data[0];
	regdata[5] = data[1];
	regdata = WriteRegister_(regdata.to_ulong(), JAreg);

	if (!alsoResetJA || regdata[8] == 0)  // if locked, then do not reconfigure JA (JA only needs a reset after a cold start, usually indicated by lock)
	{
		__COUT__ << "JA select done with no reset for input " << data;
		return;
	}

	__COUT__ << "JA reset...";

	ResetJitterAttenuator(JAreg, regdata.to_ulong());
	sleep(1);

	ConfigureJitterAttenuator();
	__COUT__ << "JA select done for input " << data;
}  // end SetJitterAttenuatorSelect()

//========================================================================
/// <summary>
/// Read the Jitter Attenuator Reset bit
/// </summary>
/// <returns>Value of the Jitter Attenuator Reset bit</returns>
bool DTCLib::CFOandDTC_Registers::ReadJitterAttenuatorReset(CFOandDTC_Register JAreg, std::optional<uint32_t> val)
{
	std::bitset<32> regdata = val.has_value() ? *val : ReadRegister_(JAreg);
	return regdata[0];
}  // end ReadJitterAttenuatorReset()

//========================================================================
/// <summary>
/// Read the Jitter Attenuator Locked bit
/// </summary>
/// <returns>Inverted Value of the Jitter Attenuator Loss-of-Lock bit</returns>
bool DTCLib::CFOandDTC_Registers::ReadJitterAttenuatorLocked(CFOandDTC_Register JAreg, std::optional<uint32_t> val)
{
	std::bitset<32> regdata = val.has_value() ? *val : ReadRegister_(JAreg);
	return !regdata[8];
}  // end ReadJitterAttenuatorLocked()

//========================================================================
/// <summary>
/// Reset the Jitter Attenuator
/// </summary>
void DTCLib::CFOandDTC_Registers::ResetJitterAttenuator(CFOandDTC_Register JAreg, std::optional<uint32_t> val)
{
	std::bitset<32> regdata = val.has_value() ? *val : ReadRegister_(JAreg);
	regdata[0] = 1;
	WriteRegister_(regdata.to_ulong(), JAreg);
	usleep(1000);
	regdata[0] = 0;
	WriteRegister_(regdata.to_ulong(), JAreg);
}  // end ResetJitterAttenuator()

//========================================================================
/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::CFOandDTC_Registers::FormatJitterAttenuatorCSR(CFOandDTC_Register JAreg)
{
	auto form = CreateFormatter(JAreg);
	std::bitset<32> data = form.value;
	std::bitset<2> JAinputSelect;
	JAinputSelect[0] = data[4];
	JAinputSelect[1] = data[5];
	form.description = "Jitter Attenuator CSR";
	form.vals.push_back("<field> : [<value>]");  // first value describes format
	form.vals.push_back(std::string("JA Source Clock Select: [") +
						(JAinputSelect.to_ulong() == 0 ? "from internal CFO"
													   : (JAinputSelect.to_ulong() == 1 ? "from RJ45"
																						: "Timing Card Selectable (SFP+ or FPGA) Input Clock")) +
						"]");
	form.vals.push_back(std::string("JA in Reset:   [") + (data[0] ? "YES" : "No") + "]");
	form.vals.push_back(std::string("JA Loss-of-Lock:   [") + (data[8] ? "Not Locked" : "LOCKED") + "]");
	form.vals.push_back(std::string("JA Input-0 CFO (or emulated CFO) Clock Source:   [") + (data[9] ? "Missing" : "OK") + "]");
	form.vals.push_back(std::string("JA Input-1 RJ45 Clock Source:   [") + (data[10] ? "Missing" : "OK") + "]");
	form.vals.push_back(std::string("JA Input-2 Timing Card Selectable, SFP+ or FPGA, Input Clock:   [") + (data[11] ? "Missing" : "OK") + "]");
	return form;
}  // end FormatJitterAttenuatorCSR()

/// <summary>
/// Write a value to the SERDES IIC Bus
/// </summary>
/// <param name="device">Device address</param>
/// <param name="address">Register address</param>
/// <param name="data">Data to write</param>
void DTCLib::CFOandDTC_Registers::WriteSERDESIICInterface(DTC_IICSERDESBusAddress device, uint8_t address, uint8_t data)
{
	uint32_t reg_data = (static_cast<uint8_t>(device) << 24) + (address << 16) + (data << 8);
	WriteRegister_(reg_data, CFOandDTC_Register_SERDESClock_IICBusLow);
	WriteRegister_(0x1, CFOandDTC_Register_SERDESClock_IICBusHigh);
	u_int16_t retries = 1000;
	while (ReadRegister_(CFOandDTC_Register_SERDESClock_IICBusHigh) == 0x1)
	{
		if (--retries == 0)
		{
			__SS__ << "Timeout waiting for I2C interface to write!" << __E__;
			__SS_THROW__;
		}
		usleep(1000);
	}
}

/// <summary>
/// Read a value from the SERDES IIC Bus
/// </summary>
/// <param name="device">Device address</param>
/// <param name="address">Register address</param>
/// <returns>Value of register</returns>
uint8_t DTCLib::CFOandDTC_Registers::ReadSERDESIICInterface(DTC_IICSERDESBusAddress device, uint8_t address)
{
	uint32_t reg_data = (static_cast<uint8_t>(device) << 24) + (address << 16);
	WriteRegister_(reg_data, CFOandDTC_Register_SERDESClock_IICBusLow);
	WriteRegister_(0x2, CFOandDTC_Register_SERDESClock_IICBusHigh);
	u_int16_t retries = 1000;
	while (ReadRegister_(CFOandDTC_Register_SERDESClock_IICBusHigh) == 0x2)
	{
		if (--retries == 0)
		{
			__SS__ << "Timeout waiting for I2C interface to read!" << __E__;
			__SS_THROW__;
		}
		usleep(1000);
	}
	auto data = ReadRegister_(CFOandDTC_Register_SERDESClock_IICBusLow);
	return static_cast<uint8_t>(data);
}

/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::CFOandDTC_Registers::FormatSERDESOscillatorParameterLow()
{
	auto form = CreateFormatter(CFOandDTC_Register_SERDESClock_IICBusLow);
	form.description = "SERDES Oscillator IIC Bus Low";
	form.vals.push_back("");  // translation
	auto data = form.value;
	std::ostringstream s1, s2, s3, s4;
	s1 << "Device:     " << std::showbase << std::hex << ((data & 0xFF000000) >> 24);
	form.vals.push_back(s1.str());
	s2 << "Address:    " << std::showbase << std::hex << ((data & 0xFF0000) >> 16);
	form.vals.push_back(s2.str());
	s3 << "Write Data: " << std::showbase << std::hex << ((data & 0xFF00) >> 8);
	form.vals.push_back(s3.str());
	s4 << "Read Data:  " << std::showbase << std::hex << (data & 0xFF);
	form.vals.push_back(s4.str());
	return form;
}

/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::CFOandDTC_Registers::FormatSERDESOscillatorParameterHigh()
{
	auto form = CreateFormatter(CFOandDTC_Register_SERDESClock_IICBusHigh);
	auto data = form.value;
	form.description = "SERDES Oscillator IIC Bus High";
	form.vals.push_back("([ x = 1 (hi) ])");  // translation
	form.vals.push_back(std::string("Write:  [") + (data & 0x1 ? "x" : " ") + "]");
	form.vals.push_back(std::string("Read:   [") + (data & 0x2 ? "x" : " ") + "]");
	return form;
}

// Firefly TX IIC Registers
/// <summary>
/// Read the Reset bit of the Firefly TX IIC Bus
/// </summary>
/// <returns>Reset bit value</returns>
bool DTCLib::CFOandDTC_Registers::ReadFireflyTXIICInterfaceReset(std::optional<uint32_t> val)
{
	auto dataSet = std::bitset<32>(val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_FireflyTX_IICBusControl));
	return dataSet[31];
}
/// <summary>
/// Reset the Firefly TX IIC Bus
/// </summary>
void DTCLib::CFOandDTC_Registers::ResetFireflyTXIICInterface()
{
	auto bs = std::bitset<32>();
	bs[31] = 1;
	WriteRegister_(bs.to_ulong(), CFOandDTC_Register_FireflyTX_IICBusControl);
	u_int16_t retries = 1000;
	while (ReadFireflyTXIICInterfaceReset())
	{
		if (--retries == 0)
		{
			__SS__ << "Timeout waiting for I2C interface to reset!" << __E__;
			__SS_THROW__;
		}
		usleep(1000);
	}
}
/// <summary>
/// Write a value to the Firefly TX IIC Bus
/// </summary>
/// <param name="device">Device address</param>
/// <param name="address">Register address</param>
/// <param name="data">Data to write</param>
void DTCLib::CFOandDTC_Registers::WriteFireflyTXIICInterface(uint8_t device, uint8_t address, uint8_t data)
{
	uint32_t reg_data = (static_cast<uint8_t>(device) << 24) + (address << 16) + (data << 8);
	WriteRegister_(reg_data, CFOandDTC_Register_FireflyTX_IICBusConfigLow);
	WriteRegister_(0x1, CFOandDTC_Register_FireflyTX_IICBusConfigHigh);
	u_int16_t retries = 1000;
	while (ReadRegister_(CFOandDTC_Register_FireflyTX_IICBusConfigHigh) == 0x1)
	{
		if (--retries == 0)
		{
			__SS__ << "Timeout waiting for I2C interface to write!" << __E__;
			__SS_THROW__;
		}
		usleep(1000);
	}
}
/// <summary>
/// Read a value from the Firefly TX IIC Bus
/// </summary>
/// <param name="device">Device address</param>
/// <param name="address">Register address</param>
/// <returns>Value of register</returns>
uint8_t DTCLib::CFOandDTC_Registers::ReadFireflyTXIICInterface(uint8_t device, uint8_t address)
{
	uint32_t reg_data = (static_cast<uint8_t>(device) << 24) + (address << 16);
	WriteRegister_(reg_data, CFOandDTC_Register_FireflyTX_IICBusConfigLow);
	WriteRegister_(0x2, CFOandDTC_Register_FireflyTX_IICBusConfigHigh);
	u_int16_t retries = 1000;
	while (ReadRegister_(CFOandDTC_Register_FireflyTX_IICBusConfigHigh) == 0x2)
	{
		if (--retries == 0)
		{
			__SS__ << "Timeout waiting for I2C interface to read!" << __E__;
			__SS_THROW__;
		}
		usleep(1000);
	}
	auto data = ReadRegister_(CFOandDTC_Register_FireflyTX_IICBusConfigLow);
	return static_cast<uint8_t>(data);
}
/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::CFOandDTC_Registers::FormatFireflyTXIICControl()
{
	auto form = CreateFormatter(CFOandDTC_Register_FireflyTX_IICBusControl);
	form.description = "TX Firefly IIC Bus Control";
	form.vals.push_back(std::string("Reset:  [") + (ReadFireflyTXIICInterfaceReset(form.value) ? "x" : " ") + "]");
	return form;
}
/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::CFOandDTC_Registers::FormatFireflyTXIICParameterLow()
{
	auto form = CreateFormatter(CFOandDTC_Register_FireflyTX_IICBusConfigLow);
	form.description = "TX Firefly IIC Bus Low";
	form.vals.push_back("");  // translation
	auto data = form.value;
	std::ostringstream s1, s2, s3, s4;
	s1 << "Device:     " << std::showbase << std::hex << ((data & 0xFF000000) >> 24);
	form.vals.push_back(s1.str());
	s2 << "Address:    " << std::showbase << std::hex << ((data & 0xFF0000) >> 16);
	form.vals.push_back(s2.str());
	s3 << "Write Data: " << std::showbase << std::hex << ((data & 0xFF00) >> 8);
	form.vals.push_back(s3.str());
	s4 << "Read Data:  " << std::showbase << std::hex << (data & 0xFF);
	form.vals.push_back(s4.str());
	return form;
}
/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::CFOandDTC_Registers::FormatFireflyTXIICParameterHigh()
{
	auto form = CreateFormatter(CFOandDTC_Register_FireflyTX_IICBusConfigHigh);
	auto data = form.value;
	form.description = "TX Firefly IIC Bus High";
	form.vals.push_back("([ x = 1 (hi) ])");  // translation
	form.vals.push_back(std::string("Write:  [") + (data & 0x1 ? "x" : " ") + "]");
	form.vals.push_back(std::string("Read:   [") + (data & 0x2 ? "x" : " ") + "]");
	return form;
}

// Firefly RX IIC Registers
/// <summary>
/// Read the Reset bit of the Firefly RX IIC Bus
/// </summary>
/// <returns>Reset bit value</returns>
bool DTCLib::CFOandDTC_Registers::ReadFireflyRXIICInterfaceReset(std::optional<uint32_t> val)
{
	auto dataSet = std::bitset<32>(val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_FireflyRX_IICBusControl));
	return dataSet[31];
}
/// <summary>
/// Reset the Firefly RX IIC Bus
/// </summary>
void DTCLib::CFOandDTC_Registers::ResetFireflyRXIICInterface()
{
	auto bs = std::bitset<32>();
	bs[31] = 1;
	WriteRegister_(bs.to_ulong(), CFOandDTC_Register_FireflyRX_IICBusControl);
	u_int16_t retries = 1000;
	while (ReadFireflyRXIICInterfaceReset())
	{
		if (--retries == 0)
		{
			__SS__ << "Timeout waiting for I2C interface to reset!" << __E__;
			__SS_THROW__;
		}
		usleep(1000);
	}
}
/// <summary>
/// Write a value to the Firefly RX IIC Bus
/// </summary>
/// <param name="device">Device address</param>
/// <param name="address">Register address</param>
/// <param name="data">Data to write</param>
void DTCLib::CFOandDTC_Registers::WriteFireflyRXIICInterface(uint8_t device, uint8_t address, uint8_t data)
{
	uint32_t reg_data = (static_cast<uint8_t>(device) << 24) + (address << 16) + (data << 8);
	WriteRegister_(reg_data, CFOandDTC_Register_FireflyRX_IICBusConfigLow);
	WriteRegister_(0x1, CFOandDTC_Register_FireflyRX_IICBusConfigHigh);
	u_int16_t retries = 1000;
	while (ReadRegister_(CFOandDTC_Register_FireflyRX_IICBusConfigHigh) == 0x1)
	{
		if (--retries == 0)
		{
			__SS__ << "Timeout waiting for I2C interface to write!" << __E__;
			__SS_THROW__;
		}
		usleep(1000);
	}
}
/// <summary>
/// Read a value from the Firefly RX IIC Bus
/// </summary>
/// <param name="device">Device address</param>
/// <param name="address">Register address</param>
/// <returns>Value of register</returns>
uint8_t DTCLib::CFOandDTC_Registers::ReadFireflyRXIICInterface(uint8_t device, uint8_t address)
{
	uint32_t reg_data = (static_cast<uint8_t>(device) << 24) + (address << 16);
	WriteRegister_(reg_data, CFOandDTC_Register_FireflyRX_IICBusConfigLow);
	WriteRegister_(0x2, CFOandDTC_Register_FireflyRX_IICBusConfigHigh);
	u_int16_t retries = 1000;
	while (ReadRegister_(CFOandDTC_Register_FireflyRX_IICBusConfigHigh) == 0x2)
	{
		if (--retries == 0)
		{
			__SS__ << "Timeout waiting for I2C interface to read!" << __E__;
			__SS_THROW__;
		}
		usleep(1000);
	}
	auto data = ReadRegister_(CFOandDTC_Register_FireflyRX_IICBusConfigLow);
	return static_cast<uint8_t>(data);
}
/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::CFOandDTC_Registers::FormatFireflyRXIICControl()
{
	auto form = CreateFormatter(CFOandDTC_Register_FireflyRX_IICBusControl);
	form.description = "RX Firefly IIC Bus Control";
	form.vals.push_back(std::string("Reset:  [") + (ReadFireflyRXIICInterfaceReset(form.value) ? "x" : " ") + "]");
	return form;
}
/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::CFOandDTC_Registers::FormatFireflyRXIICParameterLow()
{
	auto form = CreateFormatter(CFOandDTC_Register_FireflyRX_IICBusConfigLow);
	form.description = "RX Firefly IIC Bus Low";
	form.vals.push_back("");  // translation
	auto data = form.value;
	std::ostringstream s1, s2, s3, s4;
	s1 << "Device:     " << std::showbase << std::hex << ((data & 0xFF000000) >> 24);
	form.vals.push_back(s1.str());
	s2 << "Address:    " << std::showbase << std::hex << ((data & 0xFF0000) >> 16);
	form.vals.push_back(s2.str());
	s3 << "Write Data: " << std::showbase << std::hex << ((data & 0xFF00) >> 8);
	form.vals.push_back(s3.str());
	s4 << "Read Data:  " << std::showbase << std::hex << (data & 0xFF);
	form.vals.push_back(s4.str());
	return form;
}
/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::CFOandDTC_Registers::FormatFireflyRXIICParameterHigh()
{
	auto form = CreateFormatter(CFOandDTC_Register_FireflyRX_IICBusConfigHigh);
	auto data = form.value;
	form.description = "RX Firefly IIC Bus High";
	form.vals.push_back("([ x = 1 (hi) ])");  // translation
	form.vals.push_back(std::string("Write:  [") + (data & 0x1 ? "x" : " ") + "]");
	form.vals.push_back(std::string("Read:   [") + (data & 0x2 ? "x" : " ") + "]");
	return form;
}

// Firefly TXRX IIC Registers
/// <summary>
/// Read the Reset bit of the Firefly TXRX IIC Bus
/// </summary>
/// <returns>Reset bit value</returns>
bool DTCLib::CFOandDTC_Registers::ReadFireflyTXRXIICInterfaceReset(std::optional<uint32_t> val)
{
	auto dataSet = std::bitset<32>(val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_FireflyTXRX_IICBusControl));
	return dataSet[31];
}
/// <summary>
/// Reset the Firefly TXRX IIC Bus
/// </summary>
void DTCLib::CFOandDTC_Registers::ResetFireflyTXRXIICInterface()
{
	auto bs = std::bitset<32>();
	bs[31] = 1;
	WriteRegister_(bs.to_ulong(), CFOandDTC_Register_FireflyTXRX_IICBusControl);
	u_int16_t retries = 1000;
	while (ReadFireflyTXRXIICInterfaceReset())
	{
		if (--retries == 0)
		{
			__SS__ << "Timeout waiting for I2C interface to reset!" << __E__;
			__SS_THROW__;
		}
		usleep(1000);
	}
}
/// <summary>
/// Write a value to the Firefly TXRX IIC Bus
/// </summary>
/// <param name="device">Device address</param>
/// <param name="address">Register address</param>
/// <param name="data">Data to write</param>
void DTCLib::CFOandDTC_Registers::WriteFireflyTXRXIICInterface(uint8_t device, uint8_t address, uint8_t data)
{
	uint32_t reg_data = (static_cast<uint8_t>(device) << 24) + (address << 16) + (data << 8);
	WriteRegister_(reg_data, CFOandDTC_Register_FireflyTXRX_IICBusConfigLow);
	WriteRegister_(0x1, CFOandDTC_Register_FireflyTXRX_IICBusConfigHigh);
	u_int16_t retries = 1000;
	while (ReadRegister_(CFOandDTC_Register_FireflyTXRX_IICBusConfigHigh) == 0x1)
	{
		if (--retries == 0)
		{
			__SS__ << "Timeout waiting for I2C interface to write!" << __E__;
			__SS_THROW__;
		}
		usleep(1000);
	}
}
/// <summary>
/// Read a value from the Firefly TXRX IIC Bus
/// </summary>
/// <param name="device">Device address</param>
/// <param name="address">Register address</param>
/// <returns>Value of register</returns>
uint8_t DTCLib::CFOandDTC_Registers::ReadFireflyTXRXIICInterface(uint8_t device, uint8_t address)
{
	uint32_t reg_data = (static_cast<uint8_t>(device) << 24) + (address << 16);
	WriteRegister_(reg_data, CFOandDTC_Register_FireflyTXRX_IICBusConfigLow);
	WriteRegister_(0x2, CFOandDTC_Register_FireflyTXRX_IICBusConfigHigh);
	u_int16_t retries = 1000;
	while (ReadRegister_(CFOandDTC_Register_FireflyTXRX_IICBusConfigHigh) == 0x2)
	{
		if (--retries == 0)
		{
			__SS__ << "Timeout waiting for I2C interface to read!" << __E__;
			__SS_THROW__;
		}
		usleep(1000);
	}
	auto data = ReadRegister_(CFOandDTC_Register_FireflyTXRX_IICBusConfigLow);
	return static_cast<uint8_t>(data);
}
/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::CFOandDTC_Registers::FormatFireflyTXRXIICControl()
{
	auto form = CreateFormatter(CFOandDTC_Register_FireflyTXRX_IICBusControl);
	form.description = "TXRX Firefly IIC Bus Control";
	form.vals.push_back(std::string("Reset:  [") + (ReadFireflyTXRXIICInterfaceReset(form.value) ? "x" : " ") + "]");
	return form;
}
/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::CFOandDTC_Registers::FormatFireflyTXRXIICParameterLow()
{
	auto form = CreateFormatter(CFOandDTC_Register_FireflyTXRX_IICBusConfigLow);
	form.description = "TXRX Firefly IIC Bus Low";
	form.vals.push_back("");  // translation
	auto data = form.value;
	std::ostringstream s1, s2, s3, s4;
	s1 << "Device:     " << std::showbase << std::hex << ((data & 0xFF000000) >> 24);
	form.vals.push_back(s1.str());
	s2 << "Address:    " << std::showbase << std::hex << ((data & 0xFF0000) >> 16);
	form.vals.push_back(s2.str());
	s3 << "Write Data: " << std::showbase << std::hex << ((data & 0xFF00) >> 8);
	form.vals.push_back(s3.str());
	s4 << "Read Data:  " << std::showbase << std::hex << (data & 0xFF);
	form.vals.push_back(s4.str());
	return form;
}
/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::CFOandDTC_Registers::FormatFireflyTXRXIICParameterHigh()
{
	auto form = CreateFormatter(CFOandDTC_Register_FireflyTXRX_IICBusConfigHigh);
	auto data = form.value;
	form.description = "TXRX Firefly IIC Bus High";
	form.vals.push_back("([ x = 1 (hi) ])");  // translation
	form.vals.push_back(std::string("Write:  [") + (data & 0x1 ? "x" : " ") + "]");
	form.vals.push_back(std::string("Read:   [") + (data & 0x2 ? "x" : " ") + "]");
	return form;
}

// Firefly CSR Register
/// <summary>
/// Read the TXRX Firefly Present bit
/// </summary>
/// <returns>Value of bit</returns>
bool DTCLib::CFOandDTC_Registers::ReadTXRXFireflyPresent(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_FireFlyControlStatus);
	return data[26];
}
/// <summary>
/// Read the RX Firefly Present bit
/// </summary>
/// <returns>Value of bit</returns>
bool DTCLib::CFOandDTC_Registers::ReadRXFireflyPresent(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_FireFlyControlStatus);
	return data[25];
}
/// <summary>
/// Read the TX Firefly Present bit
/// </summary>
/// <returns>Value of bit</returns>
bool DTCLib::CFOandDTC_Registers::ReadTXFireflyPresent(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_FireFlyControlStatus);
	return data[24];
}
/// <summary>
/// Read the TXRX Firefly Interrupt bit
/// </summary>
/// <returns>Value of bit</returns>
bool DTCLib::CFOandDTC_Registers::ReadTXRXFireflyInterrupt(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_FireFlyControlStatus);
	return data[18];
}
/// <summary>
/// Read the RX Firefly Interrupt bit
/// </summary>
/// <returns>Value of bit</returns>
bool DTCLib::CFOandDTC_Registers::ReadRXFireflyInterrupt(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_FireFlyControlStatus);
	return data[17];
}
/// <summary>
/// Read the TX Firefly Interrupt bit
/// </summary>
/// <returns>Value of bit</returns>
bool DTCLib::CFOandDTC_Registers::ReadTXFireflyInterrupt(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_FireFlyControlStatus);
	return data[16];
}
/// <summary>
/// Read the TXRX Firefly Select bit
/// </summary>
/// <returns>Value of bit</returns>
bool DTCLib::CFOandDTC_Registers::ReadTXRXFireflySelect(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_FireFlyControlStatus);
	return data[10];
}
/// <summary>
/// Set the TXRX Firefly Select bit
/// </summary>
/// <param name="select">Value to write</param>
void DTCLib::CFOandDTC_Registers::SetTXRXFireflySelect(bool select)
{
	std::bitset<32> data = select << 9;  // Do not read value -- ReadRegister_(CFOandDTC_Register_FireFlyControlStatus);
	// data[9] = select;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_FireFlyControlStatus);
}
/// <summary>
/// Read the TX Firefly Select bit
/// </summary>
/// <returns>Value of bit</returns>
bool DTCLib::CFOandDTC_Registers::ReadTXFireflySelect(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_FireFlyControlStatus);
	return data[9];
}
/// <summary>
/// Set the TX Firefly Select bit
/// </summary>
/// <param name="select">Value to write</param>
void DTCLib::CFOandDTC_Registers::SetTXFireflySelect(bool select)
{
	std::bitset<32> data = ReadRegister_(CFOandDTC_Register_FireFlyControlStatus);
	data[9] = select;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_FireFlyControlStatus);
}
/// <summary>
/// Read the RX Firefly Select bit
/// </summary>
/// <returns>Value of bit</returns>
bool DTCLib::CFOandDTC_Registers::ReadRXFireflySelect(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_FireFlyControlStatus);
	return data[8];
}
/// <summary>
/// Set the RX Firefly Select bit
/// </summary>
/// <param name="select">Value to write</param>
void DTCLib::CFOandDTC_Registers::SetRXFireflySelect(bool select)
{
	std::bitset<32> data = ReadRegister_(CFOandDTC_Register_FireFlyControlStatus);
	data[8] = select;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_FireFlyControlStatus);
}
/// <summary>
/// Read the TXRX Firefly Reset bit
/// </summary>
/// <returns>Value of the bit</returns>
bool DTCLib::CFOandDTC_Registers::ReadResetTXRXFirefly(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_FireFlyControlStatus);
	return data[2];
}
/// Reset the TXRX Firefly
void DTCLib::CFOandDTC_Registers::ResetTXRXFirefly()
{
	std::bitset<32> data = ReadRegister_(CFOandDTC_Register_FireFlyControlStatus);
	data[2] = 1;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_FireFlyControlStatus);
	usleep(1000);
	data[2] = 0;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_FireFlyControlStatus);
}
/// <summary>
/// Read the TX Firefly Reset bit
/// </summary>
/// <returns>Value of the bit</returns>
bool DTCLib::CFOandDTC_Registers::ReadResetTXFirefly(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_FireFlyControlStatus);
	return data[1];
}
/// Reset the TX Firefly
void DTCLib::CFOandDTC_Registers::ResetTXFirefly()
{
	std::bitset<32> data = ReadRegister_(CFOandDTC_Register_FireFlyControlStatus);
	data[1] = 1;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_FireFlyControlStatus);
	usleep(1000);
	data[1] = 0;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_FireFlyControlStatus);
}
/// <summary>
/// Read the RX Firefly Reset bit
/// </summary>
/// <returns>Value of the bit</returns>
bool DTCLib::CFOandDTC_Registers::ReadResetRXFirefly(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_FireFlyControlStatus);
	return data[0];
}
/// Reset the RX Firefly
void DTCLib::CFOandDTC_Registers::ResetRXFirefly()
{
	std::bitset<32> data = ReadRegister_(CFOandDTC_Register_FireFlyControlStatus);
	data[0] = 1;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_FireFlyControlStatus);
	usleep(1000);
	data[0] = 0;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_FireFlyControlStatus);
}
/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::CFOandDTC_Registers::FormatFireflyCSR()
{
	auto form = CreateFormatter(CFOandDTC_Register_FireFlyControlStatus);
	form.description = "FireFly Control and Status";
	form.vals.push_back("([ x = 1 (hi) ])");  // translation
	form.vals.push_back(std::string("TXRX FireFly Present:   [") + (ReadTXRXFireflyPresent(form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("TX FireFly Present:     [") + (ReadRXFireflyPresent(form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("RX FireFly Present:     [") + (ReadTXFireflyPresent(form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("TXRX FireFly Interrupt: [") + (ReadTXRXFireflyInterrupt(form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("TX FireFly Interrupt:   [") + (ReadRXFireflyInterrupt(form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("RX FireFly Interrupt:   [") + (ReadTXFireflyInterrupt(form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("TXRX FireFly Select:    [") + (ReadTXRXFireflySelect(form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("TX FireFly Select:      [") + (ReadTXFireflySelect(form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("RX FireFly Select:      [") + (ReadRXFireflySelect(form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("TXRX FireFly Reset:     [") + (ReadResetTXRXFirefly(form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("TX FireFly Reset:       [") + (ReadResetTXFirefly(form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("RX FireFly Reset:       [") + (ReadResetRXFirefly(form.value) ? "x" : " ") + "]");
	return form;
}

//========================================================================
/// <summary>
/// Configure the Jitter Attenuator
/// </summary>
void DTCLib::CFOandDTC_Registers::ConfigureJitterAttenuator()
{
	CFOandDTC_Register IICLowReg = CFOandDTC_Register_SERDESClock_IICBusLow;
	CFOandDTC_Register IICHighReg = CFOandDTC_Register_SERDESClock_IICBusHigh;

	// Start configuration preamble
	// set page B
	WriteRegister_(0x68010B00, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	// page B registers
	WriteRegister_(0x6824C000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68250000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	// set page 5
	WriteRegister_(0x68010500, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	// page 5 registers
	WriteRegister_(0x68400100, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	// End configuration preamble
	//
	// Delay 300 msec
	usleep(300000 /*300ms*/);

	// Delay is worst case time for device to complete any calibration
	// that is running due to device state change previous to this script
	// being processed.
	//
	// Start configuration registers
	// set page 0
	WriteRegister_(0x68010000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	// page 0 registers
	WriteRegister_(0x68060000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68070000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68080000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x680B6800, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68160200, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x6817DC00, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68180000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x6819DD00, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x681ADF00, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x682B0200, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x682C0F00, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x682D5500, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x682E3700, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x682F0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68303700, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68310000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68323700, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68330000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68343700, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68350000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68363700, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68370000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68383700, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68390000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x683A3700, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x683B0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x683C3700, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x683D0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x683FFF00, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68400400, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68410E00, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68420E00, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68430E00, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68440E00, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68450C00, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68463200, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68473200, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68483200, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68493200, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x684A3200, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x684B3200, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x684C3200, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x684D3200, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x684E5500, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x684F5500, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68500F00, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68510300, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68520300, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68530300, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68540300, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68550300, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68560300, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68570300, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68580300, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68595500, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x685AAA00, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x685BAA00, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x685C0A00, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x685D0100, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x685EAA00, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x685FAA00, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68600A00, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68610100, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x6862AA00, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x6863AA00, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68640A00, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68650100, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x6866AA00, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x6867AA00, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68680A00, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68690100, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68920200, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x6893A000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68950000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68968000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68986000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x689A0200, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x689B6000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x689D0800, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x689E4000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68A02000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68A20000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68A98A00, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68AA6100, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68AB0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68AC0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68E52100, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68EA0A00, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68EB6000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68EC0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68ED0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	// set page 1
	WriteRegister_(0x68010100, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	// page 1 registers
	WriteRegister_(0x68020100, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68120600, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68130900, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68143B00, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68152800, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68170600, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68180900, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68193B00, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x681A2800, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x683F1000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68400000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68414000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x6842FF00, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	// set page 2
	WriteRegister_(0x68010200, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	// page 2 registers
	WriteRegister_(0x68060000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68086400, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68090000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x680A0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x680B0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x680C0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x680D0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x680E0100, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x680F0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68100000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68110000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68126400, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68130000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68140000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68150000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68160000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68170000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68180100, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68190000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x681A0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x681B0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x681C6400, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x681D0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x681E0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x681F0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68200000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68210000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68220100, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68230000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68240000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68250000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68266400, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68270000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68280000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68290000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x682A0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x682B0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x682C0100, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x682D0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x682E0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x682F0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68310B00, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68320B00, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68330B00, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68340B00, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68350000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68360000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68370000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68388000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x6839D400, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x683A0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x683B0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x683C0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x683D0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x683EC000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68500000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68510000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68520000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68530000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68540000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68550000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x686B5200, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x686C6500, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x686D7600, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x686E3100, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x686F2000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68702000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68712000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68722000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x688A0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x688B0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x688C0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x688D0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x688E0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x688F0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68900000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68910000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x6894B000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68960200, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68970200, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68990200, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x689DFA00, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x689E0100, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x689F0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68A9CC00, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68AA0400, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68AB0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68B7FF00, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	// set page 3
	WriteRegister_(0x68010300, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	// page 3 registers
	WriteRegister_(0x68020000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68030000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68040000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68050000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68061100, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68070000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68080000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68090000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x680A0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x680B8000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x680C0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x680D0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x680E0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x680F0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68100000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68110000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68120000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68130000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68140000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68150000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68160000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68170000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68380000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68391F00, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x683B0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x683C0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x683D0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x683E0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x683F0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68400000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68410000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68420000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68430000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68440000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68450000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68460000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68590000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x685A0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x685B0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x685C0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	// set page 4
	WriteRegister_(0x68010400, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	// page 4 registers
	WriteRegister_(0x68870100, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	// set page 5
	WriteRegister_(0x68010500, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	// page 5 registers
	WriteRegister_(0x68081000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68091F00, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x680A0C00, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x680B0B00, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x680C3F00, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x680D3F00, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x680E1300, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x680F2700, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68100900, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68110800, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68123F00, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68133F00, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68150000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68160000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68170000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68180000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x6819A800, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x681A0200, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x681B0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x681C0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x681D0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x681E0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x681F8000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68212B00, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x682A0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x682B0100, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x682C8700, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x682D0300, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x682E1900, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x682F1900, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68310000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68324200, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68330300, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68340000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68350000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68360000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68370000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68380000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68390000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x683A0200, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x683B0300, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x683C0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x683D1100, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x683E0600, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68890D00, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x688A0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x689BFA00, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x689D1000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x689E2100, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x689F0C00, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68A00B00, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68A13F00, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68A23F00, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68A60300, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	// set page 8
	WriteRegister_(0x68010800, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	// page 8 registers
	WriteRegister_(0x68023500, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68030500, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68040000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68050000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68060000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68070000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68080000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68090000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x680A0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x680B0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x680C0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x680D0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x680E0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x680F0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68100000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68110000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68120000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68130000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68140000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68150000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68160000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68170000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68180000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68190000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x681A0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x681B0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x681C0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x681D0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x681E0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x681F0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68200000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68210000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68220000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68230000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68240000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68250000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68260000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68270000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68280000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68290000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x682A0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x682B0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x682C0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x682D0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x682E0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x682F0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68300000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68310000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68320000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68330000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68340000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68350000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68360000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68370000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68380000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68390000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x683A0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x683B0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x683C0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x683D0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x683E0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x683F0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68400000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68410000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68420000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68430000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68440000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68450000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68460000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68470000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68480000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68490000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x684A0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x684B0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x684C0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x684D0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x684E0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x684F0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68500000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68510000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68520000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68530000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68540000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68550000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68560000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68570000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68580000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68590000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x685A0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x685B0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x685C0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x685D0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x685E0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x685F0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68600000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68610000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	// set page 9
	WriteRegister_(0x68010900, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	// page 9 registers
	WriteRegister_(0x680E0200, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68430100, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68490F00, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x684A0F00, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x684E4900, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x684F0200, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x685E0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	// set page A
	WriteRegister_(0x68010A00, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	// page A registers
	WriteRegister_(0x68020000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68030100, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68040100, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68050100, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68140000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x681A0000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	// set page B
	WriteRegister_(0x68010B00, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	// page B registers
	WriteRegister_(0x68442F00, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68460000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68470000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68480000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x684A0200, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68570E00, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68580100, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	// End configuration registers
	//
	// Start configuration postamble
	// set page 5
	WriteRegister_(0x68010500, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	// page 5 registers
	WriteRegister_(0x68140100, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	// set page 0
	WriteRegister_(0x68010000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	// page 0 registers
	WriteRegister_(0x681C0100, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	// set page 5
	WriteRegister_(0x68010500, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	// page 5 registers
	WriteRegister_(0x68400000, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	// set page B
	WriteRegister_(0x68010B00, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	// page B registers
	WriteRegister_(0x6824C300, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);

	WriteRegister_(0x68250200, IICLowReg);
	WriteRegister_(0x00000001, IICHighReg);
}
