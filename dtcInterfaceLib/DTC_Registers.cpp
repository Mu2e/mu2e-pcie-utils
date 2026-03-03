#include "DTC_Registers.h"

#include <assert.h>
#include <unistd.h>
#include <chrono>
#include <cmath>
#include <iomanip>  // std::setw, std::setfill
#include <sstream>  // Convert uint to hex string

#include "TRACE/tracemf.h"

#define TLVL_ResetDTC TLVL_DEBUG + 5
#define TLVL_AutogenDRP TLVL_DEBUG + 6
#define TLVL_SERDESReset TLVL_DEBUG + 7
#define TLVL_CalculateFreq TLVL_DEBUG + 8
#define TLVL_Detail TLVL_DEBUG + 11
#define TLVL_ReadRegister TLVL_DEBUG + 20

#include "dtcInterfaceLib/otsStyleCoutMacros.h"

#undef __COUT_HDR__
#define __COUT_HDR__ "DTC " << this->getDeviceUID() << ": "

/// <summary>
/// Construct an instance of the DTC register map
/// </summary>
/// <param name="mode">Default: DTC_SimMode_Disabled; The simulation mode of the DTC</param>
/// <param name="dtc">DTC card index to use</param>
/// <param name="simFileName">Name to use for simulated DTC memory file, if needed</param>
/// <param name="rocMask">Nibble-mask for enabled ROCs. Ex. 0x1011 enables ROC0, 1, and 3</param>
/// <param name="expectedDesignVersion">Expected DTC Firmware Design Version. If set, will
/// throw an exception if the DTC firmware does not match (Default: "")</param>
/// <param name="skipInit">Whether to skip initialization phase for reading DTC out in current state</param>
DTCLib::DTC_Registers::DTC_Registers(DTC_SimMode mode, int dtc, std::string simFileName, unsigned rocMask, std::string expectedDesignVersion,
									 bool skipInit, const std::string& uid)
	: simMode_(mode), usingDetectorEmulator_(false), dmaSize_(64)
{
	TLOG(TLVL_TRACE) << "CONSTRUCTOR";

	auto sim = getenv("DTCLIB_SIM_ENABLE");
	if (sim != nullptr)
	{
		auto simstr = std::string(sim);
		simMode_ = DTC_SimModeConverter::ConvertToSimMode(simstr);
	}

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

	SetSimMode(expectedDesignVersion, simMode_, dtc, simFileName, rocMask, skipInit, (uid == "" ? ("DTC" + std::to_string(dtc)) : uid));
}  // end constructor()

/// <summary>
/// DTC_Registers destructor
/// </summary>
DTCLib::DTC_Registers::~DTC_Registers()
{
	TLOG(TLVL_TRACE) << "DESTRUCTOR";
	DisableDetectorEmulator();
	// DisableDetectorEmulatorMode();
	// DisableCFOEmulation();
	// SoftReset();
}  // end destructor()

/// <summary>
/// Initialize the DTC in the given SimMode.
/// </summary>
/// <param name="expectedDesignVersion">Expected DTC Firmware Design Version. If set, will throw an exception if the DTC firmware does not match</param>
/// <param name="mode">Mode to set</param>
/// <param name="dtc">DTC card index to use</param>
/// <param name="simMemoryFile">Name to use for simulated DTC memory file, if needed</param>
/// <param name="rocMask">Default 0x1; The initially-enabled links. Each digit corresponds to a link, so all links = 0x111111</param>
/// <param name="skipInit">Whether to skip initializing the DTC using the SimMode. Used to read state.</param>
/// <returns></returns>
DTCLib::DTC_SimMode DTCLib::DTC_Registers::SetSimMode(std::string expectedDesignVersion, DTC_SimMode mode, int dtc, std::string simMemoryFile,
													  unsigned rocMask, bool skipInit, const std::string& uid)
{
	simMode_ = mode;
	TLOG(TLVL_INFO) << "Initializing DTC device, sim mode is " << DTC_SimModeConverter(simMode_).toString() << " for uid = " << uid << ", deviceIndex = " << dtc;

	device_.init(simMode_, dtc, simMemoryFile, uid);
	if (expectedDesignVersion != "" &&
		static_cast<uint32_t>(std::stoul(expectedDesignVersion, nullptr, 16)) != ReadRegister_(CFOandDTC_Register_DesignDate))
	{
		__SS__ << "Version mismatch! Expected DTC version is '" << expectedDesignVersion << "' (0x" << std::hex << static_cast<uint32_t>(std::stoul(expectedDesignVersion, nullptr, 16)) << " != 0x" << ReadRegister_(CFOandDTC_Register_DesignDate) << ") while the readback version was '" << ReadDesignVersion() << ".'" << __E__;
		__SS_THROW__;
		// throw new DTC_WrongVersionException(expectedDesignVersion, ReadDesignVersion());
	}

	// if (skipInit || true)
	if (skipInit)
	{
		__COUT_INFO__ << "SKIPPING Initializing device";
		return simMode_;
	}

	__COUT__ << "Initialize requested, setting device registers acccording to sim mode " << DTC_SimModeConverter(simMode_).toString();

	TLOG(TLVL_Detail) << __COUT_HDR__ << "Setting up DTC links...";
	for (auto link : DTC_ROC_Links)
	{
		bool linkEnabled = ((rocMask >> (link * 4)) & 0x1) != 0;
		if (!linkEnabled)
		{
			TLOG(TLVL_Detail) << __COUT_HDR__ << "Disabling Link " << (int)link;
			DisableLink(link);

			DisableROCEmulator(link);
			SetSERDESLoopbackMode(link, DTC_SERDESLoopbackMode_Disabled);
		}
		else
		{
			TLOG(TLVL_Detail) << __COUT_HDR__ << "Enabling Link " << (int)link;
			EnableLink(link, DTC_LinkEnableMode(true, true));
		}
	}

	if (simMode_ != DTC_SimMode_Disabled)
	{
		TLOG(TLVL_Detail) << __COUT_HDR__ << "Setting up simulation modes in Hardware...";
		// Set up hardware simulation mode: Link 0 Tx/Rx Enabled, Loopback Enabled, ROC Emulator Enabled. All other links
		// disabled. for (auto link : DTC_ROC_Links)
		// 	{
		// 	  DisableLink(link);
		// 	}
		//	EnableLink(DTC_Link_0, DTC_LinkEnableMode(true, true, false), DTC_ROC_0);
		for (auto link : DTC_ROC_Links)
		{
			if (simMode_ == DTC_SimMode_Loopback)
			{
				TLOG(TLVL_Detail) << __COUT_HDR__ << "Setting up simulation of Loopback in Hardware... Link " << (int)link;
				SetSERDESLoopbackMode(link, DTC_SERDESLoopbackMode_NearPCS);
				//			SetMaxROCNumber(DTC_Link_0, DTC_ROC_0);
			}
			else if (simMode_ == DTC_SimMode_ROCEmulator)
			{
				TLOG(TLVL_Detail) << __COUT_HDR__ << "Setting up simulation of ROC in Hardware... Link " << (int)link;
				EnableROCEmulator(link);
				// SetMaxROCNumber(DTC_Link_0, DTC_ROC_0);
			}
			else
			{
				TLOG(TLVL_Detail) << __COUT_HDR__ << "Turning off simulation in Hardware... Link " << (int)link;
				SetSERDESLoopbackMode(link, DTC_SERDESLoopbackMode_Disabled);
				DisableROCEmulator(link);
			}
		}
		SetCFOEmulationMode();
	}
	else
	{
		ClearCFOEmulationMode();
	}
	ReadMinDMATransferLength();

	__COUT__ << "Done setting device registers";
	return simMode_;
}

/// <summary>
/// Read the DDR interface reset bit
/// </summary>
/// <returns>The current value of the DDR interface reset bit</returns>
bool DTCLib::DTC_Registers::ReadDDRInterfaceReset(std::optional<uint32_t> val)
{
	return !std::bitset<32>(val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_DesignStatus))[1];
}

/// <summary>
/// Set the Reset bit in the Design Status register
/// </summary>
/// <param name="reset"></param>
void DTCLib::DTC_Registers::SetDDRInterfaceReset(bool reset)
{
	std::bitset<32> data = ReadRegister_(CFOandDTC_Register_DesignStatus);
	data[1] = !reset;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_DesignStatus);
}

/// <summary>
/// Resets the DDR interface in the firmware
/// </summary>
void DTCLib::DTC_Registers::ResetDDRInterface()
{
	SetDDRInterfaceReset(true);
	usleep(1000);
	SetDDRInterfaceReset(false);
	while (ReadDDRInterfaceReset() && !ReadDDRAutoCalibrationDone())
	{
		usleep(1000);
	}
}

/// <summary>
/// Read the DDR Auto Calibration Done bit of the Design status register
/// </summary>
/// <returns>Whether the DDR Auto Calibration is done</returns>
bool DTCLib::DTC_Registers::ReadDDRAutoCalibrationDone(std::optional<uint32_t> val)
{
	return std::bitset<32>(val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_DesignStatus))[0];
}

/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatDesignStatus()
{
	auto form = CreateFormatter(CFOandDTC_Register_DesignStatus);
	form.description = "Control and Status";
	form.vals.push_back(std::string("Reset DDR Interface:       [") + (ReadDDRInterfaceReset(form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("DDR Auto-Calibration Done: [") + (ReadDDRAutoCalibrationDone(form.value) ? "x" : " ") + "]");
	return form;
}

/// <summary>
/// Enable the DTC CFO Emulator
/// Parameters for the CFO Emulator, such as count and starting timestamp, must be set before enabling.
/// </summary>
void DTCLib::DTC_Registers::EnableCFOEmulation()
{
	std::bitset<32> data = ReadRegister_(CFOandDTC_Register_Control);
	data[30] = 1;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_Control);
}

/// <summary>
/// Disable the DTC CFO Emulator
/// </summary>
void DTCLib::DTC_Registers::DisableCFOEmulation()
{
	std::bitset<32> data = ReadRegister_(CFOandDTC_Register_Control);
	data[30] = 0;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_Control);
}

/// <summary>
/// Reads the current state of the DTC CFO Emulator
/// </summary>
/// <returns>True if the emulator is enabled, false otherwise</returns>
bool DTCLib::DTC_Registers::ReadCFOEmulationEnabled(std::optional<uint32_t> val)
{
	std::bitset<32> dataSet = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_Control);
	return dataSet[30];
}

/// <summary>
/// Enable CFO Loopback. CFO packets will be returned to the CFO, for delay calculation.
/// </summary>
void DTCLib::DTC_Registers::EnableCFOLoopback()
{
	std::bitset<32> data = ReadRegister_(CFOandDTC_Register_Control);
	data[28] = 0;  // 0 is for Loopback to the CFO, 1 is for propagation downstream to the next DTC in the chain
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_Control);
}

/// <summary>
/// Disable CFO Loopback. CFO packets will be transmitted instead to the next DTC (Normal operation)
/// </summary>
void DTCLib::DTC_Registers::DisableCFOLoopback()
{
	std::bitset<32> data = ReadRegister_(CFOandDTC_Register_Control);
	data[28] = 1;  // 0 is for Loopback to the CFO, 1 is for propagation downstream to the next DTC in the chain
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_Control);
}

/// <summary>
/// Read the value of the CFO Link Loopback bit
/// </summary>
/// <returns>Value of the bit</returns>
bool DTCLib::DTC_Registers::ReadCFOLoopback(std::optional<uint32_t> val)
{
	std::bitset<32> dataSet = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_Control);
	return dataSet[28];
}

/// <summary>
/// Resets the DDR Write pointer to 0
/// </summary>
void DTCLib::DTC_Registers::ResetDDRWriteAddress()
{
	std::bitset<32> data = ReadRegister_(CFOandDTC_Register_Control);
	data[27] = 1;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_Control);
	usleep(1000);
	data = ReadRegister_(CFOandDTC_Register_Control);
	data[27] = 0;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_Control);
}

/// <summary>
/// Determine whether a DDR Write Pointer reset is in progress
/// </summary>
/// <returns>True if the DDR Write pointer is resetting, false otherwise</returns>
bool DTCLib::DTC_Registers::ReadResetDDRWriteAddress(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_Control);
	return data[27];
}

/// <summary>
/// Reset the DDR Read pointer to 0
/// </summary>
void DTCLib::DTC_Registers::ResetDDRReadAddress()
{
	std::bitset<32> data = ReadRegister_(CFOandDTC_Register_Control);
	data[26] = 1;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_Control);
	usleep(1000);
	data = ReadRegister_(CFOandDTC_Register_Control);
	data[26] = 0;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_Control);
}

/// <summary>
/// Determine whether the DDR Read pointer is currently being reset.
/// </summary>
/// <returns>True if the read pointer is currently being reset, false otherwise</returns>
bool DTCLib::DTC_Registers::ReadResetDDRReadAddress(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_Control);
	return data[26];
}

/// <summary>
/// Reset the DDR memory interface
/// </summary>
void DTCLib::DTC_Registers::ResetDDR()
{
	std::bitset<32> data = ReadRegister_(CFOandDTC_Register_Control);
	data[25] = 1;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_Control);
	usleep(1000);
	data = ReadRegister_(CFOandDTC_Register_Control);
	data[25] = 0;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_Control);
}

/// <summary>
/// Determine whether the DDR memory interface is currently being reset
/// </summary>
/// <returns>True if the DDR memory interface is currently being reset, false otherwise</returns>
bool DTCLib::DTC_Registers::ReadResetDDR(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_Control);
	return data[25];
}

/// <summary>
/// Enable sending Data Request packets from the DTC CFO Emulator with every
/// Readout Request
/// </summary>
void DTCLib::DTC_Registers::EnableCFOEmulatorDRP()
{
	std::bitset<32> data = ReadRegister_(CFOandDTC_Register_Control);
	data[24] = 1;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_Control);
}

/// <summary>
/// Disable sending Data Request packets from the DTC CFO Emulator with every
/// Readout Request
/// </summary>
void DTCLib::DTC_Registers::DisableCFOEmulatorDRP()
{
	std::bitset<32> data = ReadRegister_(CFOandDTC_Register_Control);
	data[24] = 0;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_Control);
}

/// <summary>
/// Read whether the DTC CFO Emulator is sending Data Request packets with every readout request
/// </summary>
/// <returns>True if the DTC CFO Emulator is sending Data Request packets with every readout request, false
/// otherwise</returns>
bool DTCLib::DTC_Registers::ReadCFOEmulatorDRP(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_Control);
	return data[24];
}

/// <summary>
/// Enable automatically generating Data Request packets when heartbeats are received
/// </summary>
void DTCLib::DTC_Registers::EnableAutogenDRP()
{
	TLOG(TLVL_AutogenDRP) << __COUT_HDR__ << "EnableAutogenDRP start";
	std::bitset<32> data = ReadRegister_(CFOandDTC_Register_Control);
	data[23] = 1;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_Control);
}

/// <summary>
/// Disable automatically generating Data Request packets when heartbeats are received
/// </summary>
void DTCLib::DTC_Registers::DisableAutogenDRP()
{
	std::bitset<32> data = ReadRegister_(CFOandDTC_Register_Control);
	data[23] = 0;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_Control);
}

/// <summary>
/// Read whether Data Request packets are generated by the DTC when heartbeats are received
/// </summary>
/// <returns>True if Data Request packets are generated by the DTC when heartbeats are received, false otherwise</returns>
bool DTCLib::DTC_Registers::ReadAutogenDRP(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_Control);
	return data[23];
}

/// <summary>
/// Enable receiving Data Request Packets from the DTCLib on DMA Channel 0
/// Possibly obsolete, ask Rick before using
/// </summary>
void DTCLib::DTC_Registers::EnableSoftwareDRP()
{
	DisableAutogenDRP();
}

/// <summary>
/// Disable receiving Data Request Packets from the DTCLib on DMA Channel 0
/// Possibly obsolete, ask Rick before using
/// </summary>
void DTCLib::DTC_Registers::DisableSoftwareDRP()
{
	EnableAutogenDRP();
}
//	std::bitset<32> data = ReadRegister_(CFOandDTC_Register_Control);
//	data[22] = 0;
//	WriteRegister_(data.to_ulong(), CFOandDTC_Register_Control);
// }

/// <summary>
/// Read whether receiving Data Request Packets from the DTCLib on DMA Channel 0 is enabled
/// </summary>
/// <returns>True if receiving Data Request Packets from the DTCLib on DMA Channel 0 is enabled, false
/// otherwise</returns>
bool DTCLib::DTC_Registers::ReadSoftwareDRP(std::optional<uint32_t> val)
{
	return !ReadAutogenDRP(val);
}

/// <summary>
/// Reset the PCIe interface
/// </summary>
void DTCLib::DTC_Registers::ResetPCIe()
{
	std::bitset<32> data = ReadRegister_(CFOandDTC_Register_Control);
	data[21] = 1;
	data[20] = 1;
	data[11] = 1;
	data[7] = 1;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_Control);
	usleep(1000);
	data = ReadRegister_(CFOandDTC_Register_Control);
	data[21] = 0;
	data[20] = 0;
	data[11] = 0;
	data[7] = 0;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_Control);

	// Note the DTC instance likely needs to be reinitialized on a firmware-DMA reset to realign pointers:
	// e.g. device_.initDMAEngine();
}

/// <summary>
/// Determine whether the PCIe interface is currently being reset
/// </summary>
/// <returns>True if the PCIe interface is currently being reset, false otherwise</returns>
bool DTCLib::DTC_Registers::ReadResetPCIe(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_Control);
	return data[21];
}

/// <summary>
/// Enable the All LED bits in Control register
/// </summary>
void DTCLib::DTC_Registers::EnableLEDs()
{
	std::bitset<32> data = ReadRegister_(CFOandDTC_Register_Control);
	data[16] = 1;
	data[17] = 1;
	data[18] = 1;
	data[19] = 1;
	data[20] = 1;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_Control);
}

/// <summary>
/// Disable the All LED bits in Control register
/// </summary>
void DTCLib::DTC_Registers::DisableLEDs()
{
	std::bitset<32> data = ReadRegister_(CFOandDTC_Register_Control);
	data[16] = 0;
	data[17] = 0;
	data[18] = 0;
	data[19] = 0;
	data[20] = 0;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_Control);
}

/// <summary>
/// Enable the All LED bits in Control register
/// </summary>
void DTCLib::DTC_Registers::FlashLEDs()
{
	DisableLEDs();
	sleep(1);
	EnableLEDs();
	sleep(1);
	DisableLEDs();
}

/// <summary>
/// Enable the Down LED0 Control register bit
/// </summary>
void DTCLib::DTC_Registers::EnableDownLED0()
{
	std::bitset<32> data = ReadRegister_(CFOandDTC_Register_Control);
	data[19] = 1;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_Control);
}

/// <summary>
/// Disable the Down LED0 Control register bit
/// </summary>
void DTCLib::DTC_Registers::DisableDownLED0()
{
	std::bitset<32> data = ReadRegister_(CFOandDTC_Register_Control);
	data[19] = 0;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_Control);
}

/// <summary>
/// Read the state of the Down LED0 Control register bit
/// </summary>
/// <returns>Whether the Down LED0 bit is set</returns>
bool DTCLib::DTC_Registers::ReadDownLED0State(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_Control);
	return data[19];
}
/// <summary>
/// Enable the Up LED1 Control register bit
/// </summary>
void DTCLib::DTC_Registers::EnableUpLED1()
{
	std::bitset<32> data = ReadRegister_(CFOandDTC_Register_Control);
	data[18] = 1;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_Control);
}

/// <summary>
/// Disable the Up LED1 Control register bit
/// </summary>
void DTCLib::DTC_Registers::DisableUpLED1()
{
	std::bitset<32> data = ReadRegister_(CFOandDTC_Register_Control);
	data[18] = 0;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_Control);
}

/// <summary>
/// Read the state of the Up LED1 Control register bit
/// </summary>
/// <returns>Whether the Up LED1 bit is set</returns>
bool DTCLib::DTC_Registers::ReadUpLED1State(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_Control);
	return data[18];
}
/// <summary>
/// Enable the Up LED0 Control register bit
/// </summary>
void DTCLib::DTC_Registers::EnableUpLED0()
{
	std::bitset<32> data = ReadRegister_(CFOandDTC_Register_Control);
	data[17] = 1;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_Control);
}

/// <summary>
/// Disable the Up LED0 Control register bit
/// </summary>
void DTCLib::DTC_Registers::DisableUpLED0()
{
	std::bitset<32> data = ReadRegister_(CFOandDTC_Register_Control);
	data[17] = 0;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_Control);
}

/// <summary>
/// Read the state of the Up LED0 Control register bit
/// </summary>
/// <returns>Whether the Up LED0 bit is set</returns>
bool DTCLib::DTC_Registers::ReadUpLED0State(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_Control);
	return data[17];
}

/// <summary>
/// Enable the LED6 Control register bit
/// </summary>
void DTCLib::DTC_Registers::EnableLED6()
{
	std::bitset<32> data = ReadRegister_(CFOandDTC_Register_Control);
	data[16] = 1;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_Control);
}

/// <summary>
/// Disable the LED6 Control register bit
/// </summary>
void DTCLib::DTC_Registers::DisableLED6()
{
	std::bitset<32> data = ReadRegister_(CFOandDTC_Register_Control);
	data[16] = 0;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_Control);
}

/// <summary>
/// Read the state of the LED6 Control register bit
/// </summary>
/// <returns>Whether the LED6 bit is set</returns>
bool DTCLib::DTC_Registers::ReadLED6State(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_Control);
	return data[16];
}

/// <summary>
/// Enable CFO Emulation mode.
/// </summary>
void DTCLib::DTC_Registers::SetCFOEmulationMode()
{
	std::bitset<32> data = ReadRegister_(CFOandDTC_Register_Control);
	data[15] = 1;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_Control);
}

/// <summary>
/// Disable CFO Emulation mode.
/// </summary>
void DTCLib::DTC_Registers::ClearCFOEmulationMode()
{
	std::bitset<32> data = ReadRegister_(CFOandDTC_Register_Control);
	data[15] = 0;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_Control);
}

/// <summary>
/// Read the state of the CFO Emulation Mode bit
/// </summary>
/// <returns>Whether CFO Emulation Mode is enabled</returns>
bool DTCLib::DTC_Registers::ReadCFOEmulationMode(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_Control);
	return data[15];
}

void DTCLib::DTC_Registers::SetDataFilterEnable()
{
	std::bitset<32> data = ReadRegister_(CFOandDTC_Register_Control);
	data[13] = 1;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_Control);
}

void DTCLib::DTC_Registers::ClearDataFilterEnable()
{
	std::bitset<32> data = ReadRegister_(CFOandDTC_Register_Control);
	data[13] = 0;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_Control);
}

bool DTCLib::DTC_Registers::ReadDataFilterEnable(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_Control);
	return data[13];
}

void DTCLib::DTC_Registers::SetDRPPrefetchEnable()
{
	std::bitset<32> data = ReadRegister_(CFOandDTC_Register_Control);
	data[12] = 1;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_Control);
}

void DTCLib::DTC_Registers::ClearDRPPrefetchEnable()
{
	std::bitset<32> data = ReadRegister_(CFOandDTC_Register_Control);
	data[12] = 0;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_Control);
}

bool DTCLib::DTC_Registers::ReadDRPPrefetchEnable(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_Control);
	return data[12];
}

void DTCLib::DTC_Registers::ROCInterfaceSoftReset()
{
	std::bitset<32> data = ReadRegister_(CFOandDTC_Register_Control);
	data[12] = 1;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_Control);
	usleep(100000);
	data[12] = 0;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_Control);
}

bool DTCLib::DTC_Registers::ReadROCInterfaceSoftReset(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_Control);
	return data[11];
}

void DTCLib::DTC_Registers::EnableDropDataToEmulateEventBuilding()
{
	std::bitset<32> data = ReadRegister_(CFOandDTC_Register_Control);
	data[10] = 1;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_Control);
}

void DTCLib::DTC_Registers::DisableDropDataToEmulateEventBuilding()
{
	std::bitset<32> data = ReadRegister_(CFOandDTC_Register_Control);
	data[10] = 0;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_Control);
}

bool DTCLib::DTC_Registers::ReadDropDataToEmulateEventBuilding(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_Control);
	return data[10];
}

void DTCLib::DTC_Registers::SetPunchEnable()
{
	std::bitset<32> data = ReadRegister_(CFOandDTC_Register_Control);
	data[9] = 1;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_Control);
}

void DTCLib::DTC_Registers::ClearPunchEnable()
{
	std::bitset<32> data = ReadRegister_(CFOandDTC_Register_Control);
	data[9] = 0;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_Control);
}

bool DTCLib::DTC_Registers::ReadPunchEnable(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_Control);
	return data[9];
}

/// This offset should be set 'permanently' for the DTC in response to
///		the sample measurement at bits [18:16] of DTC_Register_CFOLinkErrorFlags
void DTCLib::DTC_Registers::SetCFOSamplePermanentOffset(int permanentOffset)
{
	// do not write more bits because high bits, in older versions were the error clear latch... ReadRegister_(DTC_Register_CFOLinkErrorFlags);
	// set only 3-bits [2:0]
	WriteRegister_(permanentOffset & 7, DTC_Register_CFOLinkErrorFlags);
}  // end SetCFOSamplePermanentOffset()

int DTCLib::DTC_Registers::ReadCFOSamplePermanentOffset(std::optional<uint32_t> val)
{
	uint32_t setting = val.has_value() ? *val : ReadRegister_(DTC_Register_CFOLinkErrorFlags);
	setting &= 7;
	if (setting > 3)
		setting = -8 + setting;
	return setting;  // return 3-bit value handling of [-2,2]
}  // end ReadCFOSamplePermanentOffset()

/// @brief Set edge selection of RTF clock and external CFO rx -> tx data
/// @param forceCFOedge is a 2-bit value
void DTCLib::DTC_Registers::SetExternalCFOSampleEdgeMode(int forceCFOedge)
{
	std::bitset<32> data = ReadRegister_(CFOandDTC_Register_Control);

	data[6] = ((forceCFOedge >> 1) & 1) ? 0 : 1;  // invert bit[1], DTC control bit [6] := 1 for forced, 0 for auto
	data[5] = (forceCFOedge & 1);                 // DTC control bit [5] := 1 for falling-edge, 0 for rising-edge
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_Control);
}  // end SetExternalCFOSampleEdgeMode()

int DTCLib::DTC_Registers::ReadExternalCFOSampleEdgeMode(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_Control);
	return (data[6] << 1) | data[5];
}  // end ReadExternalCFOSampleEdgeMode()

void DTCLib::DTC_Registers::SetExternalFanoutClockInput()
{
	std::bitset<32> data = ReadRegister_(CFOandDTC_Register_Control);
	data[4] = 1;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_Control);
}

void DTCLib::DTC_Registers::SetInternalFanoutClockInput()
{
	std::bitset<32> data = ReadRegister_(CFOandDTC_Register_Control);
	data[4] = 0;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_Control);
}

bool DTCLib::DTC_Registers::ReadFanoutClockInput(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_Control);
	return data[4];
}

/// <summary>
/// Enable receiving DCS packets.
/// </summary>
void DTCLib::DTC_Registers::EnableDCSReception()
{
	std::bitset<32> data = ReadRegister_(CFOandDTC_Register_Control);
	data[2] = 1;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_Control);
}
/// <summary>
/// Disable receiving DCS packets. Any DCS packets received will be ignored
/// </summary>
void DTCLib::DTC_Registers::DisableDCSReception()
{
	std::bitset<32> data = ReadRegister_(CFOandDTC_Register_Control);
	data[2] = 0;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_Control);
}
/// <summary>
/// Read the status of the DCS Enable bit
/// </summary>
/// <returns>Whether DCS packet reception is enabled</returns>
bool DTCLib::DTC_Registers::ReadDCSReception(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_Control);
	return data[2];
}

/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatDTCControl()
{
	auto form = CreateFormatter(CFOandDTC_Register_Control);
	form.description = "DTC Control";
	form.vals.push_back("([ x = 1 (hi) ])");

	// ~~~	DTC Control Register (0x9100) ~~~
	// Bit Position	Mode	Default Value	Description
	// 31	WO	0b0	DTC Soft Reset (Self-clearing)
	// 30	RW	0b0	CFO Emulation Enable
	// 29	RW	0b0	Reserved (Formerly CFO Emulation Enable Continuous)
	// 28	RW	0b0	CFO Link Output Control
	// 27	RW	0b0	Reset DDR Write Address
	// 26	RW	0b0	Reserved (Formerly Reset DDR Read Address)
	// 25	RW	0b0	Reserved (DDR Interface Reset)
	// 24	RW	0b0	CFO Emulator DRP Enable
	// 23	RW	0b0	DTC Autogenerate DRP Enable
	// 22	RW	0b0	Reserved (Formerly Software DRP Enable)
	// 21	RW	0b0	Reserved (Formerly local EVB Source Buffer Reset)
	// 20	RW	0b0	Reserved (Formerly EVB DMA Buffer Reset)
	// 19	RW	0b0	Down LED 0
	// 18	RW	0b0	Up LED 1
	// 17	RW	0b0	Up LED 0
	// 16	RW	0b0	LED 7
	// 15	RW	0b0	CFO Emulation Mode
	// 14	RW	0b0	Reserved (Formerly EVB Debug Count Enable)
	// 13	RW	0b0	Reserved (Formerly Trigger Filter Enable)
	// 12	RW	0b0	DRP Prefetch Enable
	// 11	RW	0b0	Reserved (Formerly EVM Buffer Reset)
	// 10	RW	0b0	Drop Subevent Data to Emulate Hardware Event Building Reserved (Formerly Sequence Number Disable)
	// 9	RW	0b0	Punch Enable on RJ-45 Output
	// 8	RW	0b0	SERDES Global Reset
	// 7	RW	0b0	Reserved (Formerly Global Buffer Reset)
	// 6	RW	0b0	Do Force External CFO Sample Edge (Formerly RX Packet Error Feedback Enable)
	// 5	RW	0b0	Force External CFO Sample Edge Select (Formerly Comma Tolerance Enable)
	// 4	RW	0b0	Fanout Clock Input Select
	// 3	RW	0b0	CFO Emulator Loopback Test Launch Control
	// 2	RW	0b0	DCS Enable
	// 1	RW	0b0	Reserved (Formerly SERDES Comma Align)
	// 0	WO	0b0	DTC Hard Reset (Self-clearing)

	form.vals.push_back(std::string("Bit-31 DTC Soft Reset (Self-clearing):       [") + (ReadSoftReset(form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("Bit-30 CFO Emulation Enable:                 [") + (ReadCFOEmulationEnabled(form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("Bit-28 CFO Link Timing Card not-in-Loopback: [") + (ReadCFOLoopback(form.value) ? "x" : " ") + "]");
	// form.vals.push_back(std::string("Bit-27 Reset DDR Write Address:         [") + (ReadResetDDRWriteAddress(form.value) ? "x" : " ") + "]");
	// form.vals.push_back(std::string("Bit-26 Reset DDR Read Address:          [") + (ReadResetDDRReadAddress(form.value) ? "x" : " ") + "]");
	// form.vals.push_back(std::string("Bit-25 Reset DDR Interface:             [") + (ReadResetDDR(form.value) ? "x" : " ") + "]");
	// form.vals.push_back(std::string("Bit-24 CFO Emulator DRP Enable:         [") + (ReadCFOEmulatorDRP(form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("Bit-23 DTC Autogenerate DRP:                 [") + (ReadAutogenDRP(form.value) ? "x" : " ") + "]");
	// form.vals.push_back(std::string("Bit-22 Software DRP:                    [") + (ReadSoftwareDRP(form.value) ? "x" : " ") + "]");
	// form.vals.push_back(std::string("Bit-22 Software DRP Enable:             [") + (ReadSoftwareDRP(form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("Bit-19 Down LED 0:                           [") + (ReadDownLED0State(form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("Bit-18 Up LED 1:                             [") + (ReadUpLED1State(form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("Bit-17 Up LED 0:                             [") + (ReadUpLED0State(form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("Bit-16 LED 6:                                [") + (ReadLED6State(form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("Bit-15 CFO Emulation Mode:                   [") + (ReadCFOEmulationMode(form.value) ? "x" : " ") + "]");
	// form.vals.push_back(std::string("Bit-31 Data Filter Enable:              [") + (ReadDataFilterEnable(form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("Bit-12 DRP Prefetch Enable:                  [") + (ReadDRPPrefetchEnable(form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("Bit-10 Skip-by-32 Subevent Readout Enable:   [") + (ReadDropDataToEmulateEventBuilding(form.value) ? "x" : " ") + "]");
	// form.vals.push_back(std::string("Bit-31 ROC Interface Soft Reset:        [") + (ReadROCInterfaceSoftReset(form.value) ? "x" : " ") + "]");
	// form.vals.push_back(std::string("Bit-31 Sequence Number Disable:         [") + (ReadSequenceNumberDisable(form.value) ? "x" : " ") + "]");
	// form.vals.push_back(std::string("Bit-31 Punch Enable:                    [") + (ReadPunchEnable(form.value) ? "x" : " ") + "]");

	form.vals.push_back(std::string("Bit-09 Punched Clock Enable:                 [") + (ReadPunchEnable(form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("Bit-08 SERDES Global Reset:                  [") + (CFOandDTC_Registers::ReadResetSERDES(form.value) ? "x" : " ") + "]");

	form.vals.push_back(std::string("Bit-06 Enable CFO-RTF Offset Control:        [") + (((ReadExternalCFOSampleEdgeMode(form.value) >> 1) & 1) ? "x" : " ") + "]");
	form.vals.push_back(std::string("Bit-05 CFO-RTF Edge Select:                  [") + ((ReadExternalCFOSampleEdgeMode(form.value) & 1) ? "x" : " ") + "]");

	// form.vals.push_back(std::string("Bit-31 RX Packet Error Feedback Enable: [") + (ReadRxPacketErrorFeedbackEnable(form.value) ? "x" : " ") + "]");
	// form.vals.push_back(std::string("Bit-31 Comma Tolerance Enable:          [") + (ReadCommaToleranceEnable(form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("Bit-04 Fanout Clock Input Select:            [") + (ReadFanoutClockInput(form.value) ? "FMC SFP Rx" : "FPGA") + "]");
	form.vals.push_back(std::string("Bit-02 DCS Enable:                           [") + (ReadDCSReception(form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("Bit-00 DTC Hard Reset (Self-clearing):       [") + (ReadHardReset(form.value) ? "x" : " ") + "]");
	return form;
}

// DMA Transfer Length Register
/// <summary>
/// Set the DMA buffer size which will automatically trigger a DMA
/// </summary>
/// <param name="length">Size, in bytes of buffer that will trigger a DMA</param>
void DTCLib::DTC_Registers::SetTriggerDMATransferLength(uint16_t length)
{
	auto data = ReadRegister_(CFOandDTC_Register_DMATransferLength);
	data = (data & 0x0000FFFF) + (length << 16);
	WriteRegister_(data, CFOandDTC_Register_DMATransferLength);
}

/// <summary>
/// Read the DMA buffer size which will automatically trigger a DMA
/// </summary>
/// <returns>The DMA buffer size which will automatically trigger a DMA, in bytes</returns>
uint16_t DTCLib::DTC_Registers::ReadTriggerDMATransferLength(std::optional<uint32_t> val)
{
	auto data = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_DMATransferLength);
	data >>= 16;
	return static_cast<uint16_t>(data);
}

/// <summary>
/// Set the minimum DMA transfer size. Absolute minimum is 64 bytes.
/// Buffers smaller than this size will be padded to the minimum.
/// </summary>
/// <param name="length">Size, in bytes, of the minimum DMA transfer buffer</param>
void DTCLib::DTC_Registers::SetMinDMATransferLength(uint16_t length)
{
	auto data = ReadRegister_(CFOandDTC_Register_DMATransferLength);
	data = (data & 0xFFFF0000) + length;
	WriteRegister_(data, CFOandDTC_Register_DMATransferLength);
}

/// <summary>
/// Read the minimum DMA transfer size.
/// </summary>
/// <returns>The minimum DMA size, in bytes</returns>
uint16_t DTCLib::DTC_Registers::ReadMinDMATransferLength(std::optional<uint32_t> val)
{
	auto data = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_DMATransferLength);
	data = data & 0x0000FFFF;
	dmaSize_ = static_cast<uint16_t>(data);
	return dmaSize_;
}

/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatDMATransferLength()
{
	auto form = CreateFormatter(CFOandDTC_Register_DMATransferLength);
	form.description = "DMA Transfer Length";
	std::stringstream o;
	o << "Trigger Length: 0x" << std::hex << ReadTriggerDMATransferLength(form.value);
	form.vals.push_back(o.str());
	std::stringstream p;
	p << "Minimum Length: 0x" << std::hex << ReadMinDMATransferLength(form.value);
	form.vals.push_back(p.str());
	return form;
}

// SERDES Loopback Enable Register
/// <summary>
/// Set the SERDES Loopback mode for the given link
/// </summary>
/// <param name="link">Link to set for</param>
/// <param name="mode">DTC_SERDESLoopbackMode to set</param>
void DTCLib::DTC_Registers::SetSERDESLoopbackMode(DTC_Link_ID const& link, const DTC_SERDESLoopbackMode& mode)
{
	std::bitset<32> data = ReadRegister_(CFOandDTC_Register_SERDES_LoopbackEnable);
	std::bitset<3> modeSet = mode;
	data[3 * link] = modeSet[0];
	data[3 * link + 1] = modeSet[1];
	data[3 * link + 2] = modeSet[2];
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_SERDES_LoopbackEnable);
}

/// <summary>
/// Read the SERDES Loopback mode for the given link
/// </summary>
/// <param name="link">Link to read</param>
/// <returns>DTC_SERDESLoopbackMode of the link</returns>
DTCLib::DTC_SERDESLoopbackMode DTCLib::DTC_Registers::ReadSERDESLoopback(DTC_Link_ID const& link, std::optional<uint32_t> val)
{
	std::bitset<3> dataSet = (val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_SERDES_LoopbackEnable)) >> 3 * link;
	return static_cast<DTC_SERDESLoopbackMode>(dataSet.to_ulong());
}

/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatSERDESLoopbackEnable()
{
	auto form = CreateFormatter(CFOandDTC_Register_SERDES_LoopbackEnable);
	form.description = "SERDES Loopback Enable";
	form.vals.push_back("");  // translation
	for (auto r : DTC_ROC_Links)
	{
		form.vals.push_back(std::string("Link ") + std::to_string(r) + ": " +
							DTC_SERDESLoopbackModeConverter(ReadSERDESLoopback(r, form.value)).toString());
	}
	form.vals.push_back(std::string("CFO:    ") +
						DTC_SERDESLoopbackModeConverter(ReadSERDESLoopback(DTC_Link_CFO, form.value)).toString());
	form.vals.push_back(std::string("EVB:    ") +
						DTC_SERDESLoopbackModeConverter(ReadSERDESLoopback(DTC_Link_EVB, form.value)).toString());
	return form;
}

// Clock Status Register
/// <summary>
/// Read the SERDES Oscillator IIC Error Bit
/// </summary>
/// <returns>True if the SERDES Oscillator IIC Error is set</returns>
bool DTCLib::DTC_Registers::ReadSERDESOscillatorIICError(std::optional<uint32_t> val)
{
	std::bitset<32> dataSet = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_ClockOscillatorStatus);
	return dataSet[2];
}

/// <summary>
/// Read the DDR Oscillator IIC Error Bit
/// </summary>
/// <returns>True if the DDR Oscillator IIC Error is set</returns>
bool DTCLib::DTC_Registers::ReadDDROscillatorIICError(std::optional<uint32_t> val)
{
	std::bitset<32> dataSet = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_ClockOscillatorStatus);
	return dataSet[18];
}

/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatClockOscillatorStatus()
{
	auto form = CreateFormatter(CFOandDTC_Register_ClockOscillatorStatus);
	form.description = "Clock Oscillator Status";
	form.vals.push_back("([ x = 1 (hi) ])");  // translation
	form.vals.push_back(std::string("SERDES IIC Error:     [") + (ReadSERDESOscillatorIICError(form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("DDR IIC Error:        [") + (ReadDDROscillatorIICError(form.value) ? "x" : " ") + "]");
	return form;
}

// ROC Emulation Enable Register
/// <summary>
/// Enable the ROC emulator on the given link
/// </summary>
/// <param name="link">Link to enable</param>
void DTCLib::DTC_Registers::EnableROCEmulator(DTC_Link_ID const& link, DTC_ROC_Emulation_Type const& type)
{
	std::bitset<32> dataSet = ReadRegister_(DTC_Register_ROCEmulationEnable);
	if (link == DTC_Link_ALL)
		for (const auto& l : DTC_ROC_Links)
			dataSet[l + type * 6] = 1;
	else
		dataSet[link + type * 6] = 1;

	WriteRegister_(dataSet.to_ulong(), DTC_Register_ROCEmulationEnable);
}

/// <summary>
/// Disable the ROC emulator on the given link
/// </summary>
/// <param name="link">Link to disable</param>
void DTCLib::DTC_Registers::DisableROCEmulator(DTC_Link_ID const& link, DTC_ROC_Emulation_Type const& type)
{
	std::bitset<32> dataSet = ReadRegister_(DTC_Register_ROCEmulationEnable);
	if (link == DTC_Link_ALL)
		for (const auto& l : DTC_ROC_Links)
			dataSet[l + type * 6] = 0;
	else
		dataSet[link + type * 6] = 0;

	WriteRegister_(dataSet.to_ulong(), DTC_Register_ROCEmulationEnable);
}

/// <summary>
/// Read the state of the ROC emulator on the given link
/// </summary>
/// <param name="link">Link to read</param>
/// <returns>True if the ROC Emulator is enabled on the link</returns>
bool DTCLib::DTC_Registers::ReadROCEmulator(DTC_Link_ID const& link, DTC_ROC_Emulation_Type const& type, std::optional<uint32_t> val)
{
	if (std::find(DTC_ROC_Links.begin(), DTC_ROC_Links.end(), link) == DTC_ROC_Links.end())
	{
		__SS__ << "Illegal link specified for ROC Emulator target: " << link << __E__;
		__SS_THROW__;
	}

	std::bitset<32> dataSet = val.has_value() ? *val : ReadRegister_(DTC_Register_ROCEmulationEnable);
	return dataSet[link + type * 6];
}
void DTCLib::DTC_Registers::SetROCEmulatorMask(uint32_t rocEnableMask)
{
	WriteRegister_(rocEnableMask, DTC_Register_ROCEmulationEnable);
}
uint32_t DTCLib::DTC_Registers::ReadROCEmulatorMask(std::optional<uint32_t> val)
{
	return val.has_value() ? *val : ReadRegister_(DTC_Register_ROCEmulationEnable);
}

/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatROCEmulationEnable()
{
	auto form = CreateFormatter(DTC_Register_ROCEmulationEnable);
	form.description = "ROC Emulator Enable";
	form.vals.push_back("       ([ Internal, Fiber-Loopback, External ])");  // translation
	for (auto r : DTC_ROC_Links)
		form.vals.push_back(std::string("Link ") + std::to_string(r) +
							": [" + (ReadROCEmulator(r, DTC_ROC_Emulation_Type::ROC_Internal_Emulation, form.value) ? "x" : ".") + (ReadROCEmulator(r, DTC_ROC_Emulation_Type::ROC_FiberLoopback_Emulation, form.value) ? "x" : ".") + (ReadROCEmulator(r, DTC_ROC_Emulation_Type::ROC_External_Emulation, form.value) ? "x" : ".") + "]");

	return form;
}

// Link Enable Register
/// <summary>
/// Enable Receive CFO Link
/// </summary>
void DTCLib::DTC_Registers::EnableReceiveCFOLink()
{
	std::bitset<32> data = ReadRegister_(CFOandDTC_Register_LinkEnable);
	data[14] = 1;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_LinkEnable);
}
/// <summary>
/// Disable Receive CFO Link
/// </summary>
void DTCLib::DTC_Registers::DisableReceiveCFOLink()
{
	std::bitset<32> data = ReadRegister_(CFOandDTC_Register_LinkEnable);
	data[14] = 0;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_LinkEnable);
}
/// <summary>
/// Enable Receive CFO Link
/// </summary>
void DTCLib::DTC_Registers::EnableTransmitCFOLink()
{
	std::bitset<32> data = ReadRegister_(CFOandDTC_Register_LinkEnable);
	data[6] = 1;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_LinkEnable);
}
/// <summary>
/// Disable Receive CFO Link
/// </summary>
void DTCLib::DTC_Registers::DisableTransmitCFOLink()
{
	std::bitset<32> data = ReadRegister_(CFOandDTC_Register_LinkEnable);
	data[6] = 0;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_LinkEnable);
}
/// <summary>
/// Enable a SERDES Link based on the mode tx/rx enable bits (default is enabled)
/// </summary>
/// <param name="link">Link to enable</param>
/// <param name="mode">Link enable bits to set (Default: All)</param>
void DTCLib::DTC_Registers::EnableLink(DTC_Link_ID const& link, const DTC_LinkEnableMode& mode)
{
	std::bitset<32> data = ReadRegister_(CFOandDTC_Register_LinkEnable);
	data[link] = mode.TransmitEnable;
	data[link + 8] = mode.ReceiveEnable;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_LinkEnable);
}

/// <summary>
/// Disable a SERDES Link
/// The given mode bits will be UNSET
/// </summary>
/// <param name="link">Link to disable</param>
/// <param name="mode">Link enable bits to unset (Default: All)</param>
void DTCLib::DTC_Registers::DisableLink(DTC_Link_ID const& link, const DTC_LinkEnableMode& mode)
{
	std::bitset<32> data = ReadRegister_(CFOandDTC_Register_LinkEnable);
	data[link] = data[link] && !mode.TransmitEnable;
	data[link + 8] = data[link + 8] && !mode.ReceiveEnable;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_LinkEnable);
}

/// <summary>
/// Read the Link Enable bits for a given SERDES link
/// </summary>
/// <param name="link">Link to read</param>
/// <returns>DTC_LinkEnableMode containing TX, RX, and CFO bits</returns>
DTCLib::DTC_LinkEnableMode DTCLib::DTC_Registers::ReadLinkEnabled(DTC_Link_ID const& link, std::optional<uint32_t> val)
{
	std::bitset<32> dataSet = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_LinkEnable);
	return DTC_LinkEnableMode(dataSet[link], dataSet[link + 8]);
}
uint32_t DTCLib::DTC_Registers::ReadLinkEnabledData()
{
	return ReadRegister_(CFOandDTC_Register_LinkEnable);
}

/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatLinkEnable()
{
	auto form = CreateFormatter(CFOandDTC_Register_LinkEnable);
	form.description = "Link Enable";
	form.vals.push_back("       ([TX, RX])");
	for (auto r : DTC_ROC_Links)
	{
		auto re = ReadLinkEnabled(r, form.value);
		form.vals.push_back(std::string("Link ") + std::to_string(r) + ": [" + (re.TransmitEnable ? "x" : ".") + "" +
							(re.ReceiveEnable ? "x" : ".") + "]");
	}
	{
		auto ce = ReadLinkEnabled(DTC_Link_CFO, form.value);
		form.vals.push_back(std::string("CFO:    [") + (ce.TransmitEnable ? "x" : ".") + "" +
							(ce.ReceiveEnable ? "x" : ".") + "]");
	}
	{
		auto ee = ReadLinkEnabled(DTC_Link_EVB, form.value);
		form.vals.push_back(std::string("EVB:    [") + (ee.TransmitEnable ? "x" : ".") + "" +
							(ee.ReceiveEnable ? "x" : ".") + "]");
	}
	return form;
}

// SERDES Reset Register
/// <summary>
/// Reset the SERDES TX side
/// Will poll the Reset SERDES TX Done flag until the SERDES reset is complete
/// </summary>
/// <param name="link">Link to reset</param>
/// <param name="interval">Polling interval, in microseconds</param>
void DTCLib::DTC_Registers::ResetSERDESTX(DTC_Link_ID const& link, int interval)
{
	TLOG(TLVL_INFO) << __COUT_HDR__ << "Entering SERDES TX Reset Loop for Link " << link;
	std::bitset<32> data = ReadRegister_(CFOandDTC_Register_SERDES_Reset);
	if (link == DTC_Link_ALL)
	{
		for (uint8_t i = 0; i < 8; ++i)
			data[i + 24] = 1;
	}
	else
		data[link + 24] = 1;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_SERDES_Reset);

	usleep(interval);

	data = ReadRegister_(CFOandDTC_Register_SERDES_Reset);
	if (link == DTC_Link_ALL)
	{
		for (uint8_t i = 0; i < 8; ++i)
			data[i + 24] = 0;
	}
	else
		data[link + 24] = 0;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_SERDES_Reset);

	auto resetDone = false;
	uint32_t loops = 0;
	while (!resetDone && ++loops < 10)
	{
		usleep(interval);

		// DO NOT CHECK until bit 7-0 resets, that seems to update all reset done bits
		resetDone = true;
		// if(link == DTC_Link_ALL)
		// {
		// 	//Ignore CFO link reset since it depends on CFO emulation mode and/or CFO presence
		// 	resetDone = (ReadRegister_(CFOandDTC_Register_SERDES_ResetDone) & 0xBF) == 0xBF;
		// }
		// else
		// 	resetDone = ReadResetTXSERDESDone(link);

		TLOG(TLVL_INFO) << __COUT_HDR__ << "End of SERDES TX Reset loop=" << loops << ", done=" << std::boolalpha << resetDone;
	}

	if (loops >= 10)
	{
		__SS__ << "Timeout waiting for SERDES TX Reset loop.";
		__SS_THROW__;
		// throw DTC_IOErrorException("Timeout waiting for SERDES TX Reset loop.");
	}
}

/// <summary>
/// Read if a SERDES TX reset is currently in progress
/// </summary>
/// <param name="link">Link to read</param>
/// <returns>True if a SERDES TX reset is in progress</returns>
bool DTCLib::DTC_Registers::ReadResetSERDESTX(DTC_Link_ID const& link, std::optional<uint32_t> val)
{
	std::bitset<32> dataSet = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_SERDES_Reset);
	return dataSet[link + 24];
}

/// <summary>
/// Reset the SERDES RX side
/// Will poll the Reset SERDES RX Done flag until the SERDES reset is complete
/// </summary>
/// <param name="link">Link to reset</param>
/// <param name="interval">Polling interval, in microseconds</param>
void DTCLib::DTC_Registers::ResetSERDESRX(DTC_Link_ID const& link, int interval)
{
	TLOG(TLVL_INFO) << __COUT_HDR__ << "Entering SERDES RX Reset Loop for Link " << link;
	std::bitset<32> data = ReadRegister_(CFOandDTC_Register_SERDES_Reset);
	if (link == DTC_Link_ALL)
	{
		for (uint8_t i = 0; i < 8; ++i)
			data[i + 16] = 1;
	}
	else
		data[link + 16] = 1;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_SERDES_Reset);

	usleep(interval);

	data = ReadRegister_(CFOandDTC_Register_SERDES_Reset);
	if (link == DTC_Link_ALL)
	{
		for (uint8_t i = 0; i < 8; ++i)
			data[i + 16] = 0;
	}
	else
		data[link + 16] = 0;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_SERDES_Reset);

	auto resetDone = false;
	uint32_t loops = 0;
	while (!resetDone && ++loops < 10)
	{
		usleep(interval);

		// DO NOT CHECK until bit 7-0 resets, that seems to update all reset done bits
		resetDone = true;
		// if(link == DTC_Link_ALL)
		// {
		// 	//Ignore CFO link reset since it depends on CFO emulation mode and/or CFO presence
		// 	resetDone = ((ReadRegister_(CFOandDTC_Register_SERDES_ResetDone) >> 16) & 0xBF) == 0xBF;
		// }
		// else
		// 	resetDone = ReadResetRXSERDESDone(link);

		TLOG(TLVL_INFO) << __COUT_HDR__ << "End of SERDES RX Reset loop=" << loops << ", done=" << std::boolalpha << resetDone;
	}

	if (loops >= 10)
	{
		__SS__ << "Timeout waiting for SERDES RX Reset loop.";
		__SS_THROW__;
		// throw DTC_IOErrorException("Timeout waiting for SERDES RX Reset loop.");
	}
}

/// <summary>
/// Read if a SERDES RX reset is currently in progress
/// </summary>
/// <param name="link">Link to read</param>
/// <returns>True if a SERDES reset is in progress</returns>
bool DTCLib::DTC_Registers::ReadResetSERDESRX(DTC_Link_ID const& link, std::optional<uint32_t> val)
{
	std::bitset<32> dataSet = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_SERDES_Reset);
	return dataSet[link + 16];
}

/// <summary>
/// Reset the SERDES PLL
/// Will poll the Reset SERDES PLL Done flag until the SERDES reset is complete
/// </summary>
/// <param name="pll">PLL to reset</param>
/// <param name="interval">Polling interval, in microseconds</param>
void DTCLib::DTC_Registers::ResetSERDESPLL(const DTC_PLL_ID& pll, int interval)
{
	std::bitset<32> data = ReadRegister_(CFOandDTC_Register_SERDES_Reset);
	data[pll + 8] = 1;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_SERDES_Reset);

	usleep(interval);

	data = ReadRegister_(CFOandDTC_Register_SERDES_Reset);
	data[pll + 8] = 0;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_SERDES_Reset);
}

/// <summary>
/// Read if a SERDES PLL reset is currently in progress
/// </summary>
/// <param name="pll">PLL to read</param>
/// <returns>True if a SERDES reset is in progress</returns>
bool DTCLib::DTC_Registers::ReadResetSERDESPLL(const DTC_PLL_ID& pll, std::optional<uint32_t> val)
{
	std::bitset<32> dataSet = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_SERDES_Reset);
	return dataSet[pll + 8];
}

/// <summary>
/// Reset the SERDES
/// Will poll the Reset SERDES Done flag until the SERDES reset is complete
/// </summary>
/// <param name="link">Link to reset</param>
/// <param name="interval">Polling interval, in microseconds</param>
void DTCLib::DTC_Registers::ResetSERDES(DTC_Link_ID const& link, int interval)
{
	TLOG(TLVL_INFO) << __COUT_HDR__ << "Entering SERDES Reset Loop for Link " << link;
	std::bitset<32> data = ReadRegister_(CFOandDTC_Register_SERDES_Reset);
	if (link == DTC_Link_ALL)
	{
		for (uint8_t i = 0; i < 8; ++i)
			data[i] = 1;
	}
	else
		data[link] = 1;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_SERDES_Reset);

	// usleep(interval);

	data = ReadRegister_(CFOandDTC_Register_SERDES_Reset);
	if (link == DTC_Link_ALL)
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

		if (link == DTC_Link_ALL)
		{
			// Ignore CFO link reset since it depends on CFO emulation mode and/or CFO presence
			//	 For new DTC versions since December 2023, EVB is not instantiated, so also ignore
			//  resetDone = (ReadRegister_(CFOandDTC_Register_SERDES_ResetDone) & 0xBF) == 0xBF;
			//  resetDone = resetDone &&
			//  	( ((ReadRegister_(CFOandDTC_Register_SERDES_ResetDone) >> 16) & 0xBF) == 0xBF);
			resetDone = (ReadRegister_(CFOandDTC_Register_SERDES_ResetDone) & 0x3F) == 0x3F;
			resetDone = resetDone &&
						(((ReadRegister_(CFOandDTC_Register_SERDES_ResetDone) >> 16) & 0x3F) == 0x3F);
		}
		else
		{
			resetDone = ReadResetRXSERDESDone(link);
			resetDone = resetDone && ReadResetTXSERDESDone(link);
		}
		TLOG(TLVL_INFO) << __COUT_HDR__ << "End of SERDES Reset loop=" << loops << ", done=" << std::boolalpha << resetDone;
	}

	if (loops >= 100)
	{
		__SS__ << "Timeout waiting for SERDES Reset loop=" << loops;
		__SS_THROW__;
		// throw DTC_IOErrorException("Timeout waiting for SERDES Reset loop.");
	}
}

DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatRXDiagFifo(DTC_Link_ID const& link)
{
	__COUT__ << "FormatRXDiagFifo.";
	DTC_Register reg;
	switch (link)
	{
		case DTC_Link_0:
			reg = DTC_Register_RXDataDiagnosticFIFO_Link0;
			break;
		case DTC_Link_1:
			reg = DTC_Register_RXDataDiagnosticFIFO_Link1;
			break;
		case DTC_Link_2:
			reg = DTC_Register_RXDataDiagnosticFIFO_Link2;
			break;
		case DTC_Link_3:
			reg = DTC_Register_RXDataDiagnosticFIFO_Link3;
			break;
		case DTC_Link_4:
			reg = DTC_Register_RXDataDiagnosticFIFO_Link4;
			break;
		case DTC_Link_5:
			reg = DTC_Register_RXDataDiagnosticFIFO_Link5;
			break;
		case DTC_Link_CFO:
			reg = DTC_Register_RXDataDiagnosticFIFO_LinkCFO;
			break;
		default: {
			__SS__ << "Invalid DTC Link";
			__SS_THROW__;
			// throw std::runtime_error("Invalid DTC Link");
		}
	}

	auto form = CreateFormatter(reg);
	form.description = "Rx Diag FIFO";
	form.vals.push_back("");  // translation
	for (uint8_t i = 0; i < 100; ++i)
	{
		__COUT__ << "FormatRXDiagFifo loop." << i;
		std::ostringstream o;
		o << std::hex << std::setfill('0');
		o << "    0x" << std::setw(4) << static_cast<int>(form.address) << "  | 0x" << std::setw(8)
		  << static_cast<int>(ReadRegister_(reg)) << " | ";
		form.vals.push_back(o.str());
	}

	__COUT__ << "FormatRXDiagFifo loop done.";
	return form;
}

DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatTXDiagFifo(DTC_Link_ID const& link)
{
	DTC_Register reg;
	switch (link)
	{
		case DTC_Link_0:
			reg = DTC_Register_TXDataDiagnosticFIFO_Link0;
			break;
		case DTC_Link_1:
			reg = DTC_Register_TXDataDiagnosticFIFO_Link1;
			break;
		case DTC_Link_2:
			reg = DTC_Register_TXDataDiagnosticFIFO_Link2;
			break;
		case DTC_Link_3:
			reg = DTC_Register_TXDataDiagnosticFIFO_Link3;
			break;
		case DTC_Link_4:
			reg = DTC_Register_TXDataDiagnosticFIFO_Link4;
			break;
		case DTC_Link_5:
			reg = DTC_Register_TXDataDiagnosticFIFO_Link5;
			break;
		case DTC_Link_CFO:
			reg = DTC_Register_TXDataDiagnosticFIFO_LinkCFO;
			break;
		default: {
			__SS__ << "Invalid DTC Link";
			__SS_THROW__;
			// throw std::runtime_error("Invalid DTC Link");
		}
	}
	auto form = CreateFormatter(reg);
	form.description = "Tx Diag FIFO";
	form.vals.push_back("");  // translation
	for (uint8_t i = 0; i < 100; ++i)
	{
		std::ostringstream o;
		o << std::hex << std::setfill('0');
		o << "    0x" << std::setw(4) << static_cast<int>(form.address) << "  | 0x" << std::setw(8)
		  << static_cast<int>(ReadRegister_(reg)) << " | ";
		form.vals.push_back(o.str());
	}

	return form;
}

/// <summary>
/// Read if a SERDES reset is currently in progress
/// </summary>
/// <param name="link">Link to read</param>
/// <returns>True if a SERDES reset is in progress</returns>
bool DTCLib::DTC_Registers::ReadResetSERDES(DTC_Link_ID const& link, std::optional<uint32_t> val)
{
	std::bitset<32> dataSet = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_SERDES_Reset);
	return dataSet[link];
}

/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatSERDESReset()
{
	auto form = CreateFormatter(CFOandDTC_Register_SERDES_Reset);
	form.description = "SERDES Reset";
	form.vals.push_back("           ([TX,RX,Link])");
	for (auto r : DTC_ROC_Links)
		form.vals.push_back(std::string("Link ") + std::to_string(r) + ":     [" +
							(ReadResetSERDESTX(r, form.value) ? "x" : ".") +
							(ReadResetSERDESRX(r, form.value) ? "x" : ".") +
							(ReadResetSERDES(r, form.value) ? "x" : ".") + "]");
	form.vals.push_back(std::string("CFO:        [") +
						(ReadResetSERDESTX(DTC_Link_CFO, form.value) ? "x" : ".") +
						(ReadResetSERDESRX(DTC_Link_CFO, form.value) ? "x" : ".") +
						(ReadResetSERDES(DTC_Link_CFO, form.value) ? "x" : ".") + "]");
	form.vals.push_back(std::string("EVB:        [") +
						(ReadResetSERDESTX(DTC_Link_EVB, form.value) ? "x" : ".") +
						(ReadResetSERDESRX(DTC_Link_EVB, form.value) ? "x" : ".") +
						(ReadResetSERDES(DTC_Link_EVB, form.value) ? "x" : ".") + "]");

	form.vals.push_back("           ([ x = 1 (hi) ])");
	for (auto r : DTC_PLLs)
		form.vals.push_back(std::string("PLL Link ") + std::to_string(r) + ": [" +
							(ReadResetSERDESPLL(r, form.value) ? "x" : ".") + "]");
	form.vals.push_back(std::string("PLL CFO RX: [") +
						(ReadResetSERDESPLL(DTC_PLL_CFO_RX, form.value) ? "x" : ".") + "]");
	form.vals.push_back(std::string("PLL CFO TX: [") +
						(ReadResetSERDESPLL(DTC_PLL_CFO_TX, form.value) ? "x" : ".") + "]");

	return form;
}

// SERDES RX Disparity Error Register
/// <summary>
/// Read the SERDES RX Dispatity Error bits
/// </summary>
/// <param name="link">Link to read</param>
/// <returns>DTC_SERDESRXDisparityError object with error bits</returns>
DTCLib::DTC_SERDESRXDisparityError DTCLib::DTC_Registers::ReadSERDESRXDisparityError(DTC_Link_ID const& link, std::optional<uint32_t> val)
{
	return DTC_SERDESRXDisparityError(val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_SERDES_RXDisparityError), link);
}

/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatSERDESRXDisparityError()
{
	auto form = CreateFormatter(CFOandDTC_Register_SERDES_RXDisparityError);
	form.description = "SERDES RX Disparity Error";
	form.vals.push_back("       ([H,L])");
	for (auto r : DTC_ROC_Links)
	{
		auto re = ReadSERDESRXDisparityError(r, form.value);
		form.vals.push_back(std::string("Link ") + std::to_string(r) + ": [" + to_string(re.GetData()[1]) + "," +
							to_string(re.GetData()[0]) + "]");
	}
	auto ce = ReadSERDESRXDisparityError(DTC_Link_CFO, form.value);
	form.vals.push_back(std::string("CFO:    [") + to_string(ce.GetData()[1]) + "," + to_string(ce.GetData()[0]) + "]");
	return form;
}

// SERDES RX Character Not In Table Error Register
/// <summary>
/// Read the SERDES Character Not In Table Error bits
/// </summary>
/// <param name="link">Link to read</param>
/// <returns>DTC_CharacterNotInTableError object with error bits</returns>
DTCLib::DTC_CharacterNotInTableError DTCLib::DTC_Registers::ReadSERDESRXCharacterNotInTableError(DTC_Link_ID const& link, std::optional<uint32_t> val)
{
	return DTC_CharacterNotInTableError(val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_SERDES_RXCharacterNotInTableError), link);
}

/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatSERDESRXCharacterNotInTableError()
{
	auto form = CreateFormatter(CFOandDTC_Register_SERDES_RXCharacterNotInTableError);
	form.description = "SERDES RX CNIT Error";
	form.vals.push_back("       ([H,L])");
	for (auto r : DTC_ROC_Links)
	{
		auto re = ReadSERDESRXCharacterNotInTableError(r, form.value);
		form.vals.push_back(std::string("Link ") + std::to_string(r) + ": [" + to_string(re.GetData()[1]) + "," +
							to_string(re.GetData()[0]) + "]");
	}
	auto ce = ReadSERDESRXCharacterNotInTableError(DTC_Link_CFO, form.value);
	form.vals.push_back(std::string("CFO:    [") + to_string(ce.GetData()[1]) + "," + to_string(ce.GetData()[0]) + "]");
	return form;
}

// SERDES Unlock Error Register
/// <summary>
/// Read whether the SERDES CDR Unlock Error bit is set
/// </summary>
/// <param name="link">Link to check</param>
/// <returns>True if the SERDES CDR Unlock Error bit is set on the given link</returns>
bool DTCLib::DTC_Registers::ReadSERDESCDRUnlockError(DTC_Link_ID const& link, std::optional<uint32_t> val)
{
	std::bitset<32> dataSet = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_SERDES_UnlockError);
	return dataSet[16 + link];
}

/// <summary>
/// Read whether the SERDES PLL Unlock Error bit is set
/// </summary>
/// <param name="pll">PLL to check</param>
/// <returns>True if the SERDES PLL Unlock Error bit is set for the given PLL</returns>
bool DTCLib::DTC_Registers::ReadSERDESPLLUnlockError(const DTC_PLL_ID& pll, std::optional<uint32_t> val)
{
	std::bitset<32> dataSet = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_SERDES_UnlockError);
	return dataSet[pll];
}

/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatSERDESUnlockError()
{
	auto form = CreateFormatter(CFOandDTC_Register_SERDES_UnlockError);
	form.description = "SERDES Unlock Error";
	form.vals.push_back("([ x = 1 (hi) ])");  // translation
	for (auto r : DTC_ROC_Links)
		form.vals.push_back(std::string("CDR Link ") + std::to_string(r) + ":    [" +
							(ReadSERDESCDRUnlockError(r, form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("CDR CFO:       [") + (ReadSERDESCDRUnlockError(DTC_Link_CFO, form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("CDR EVB:       [") + (ReadSERDESCDRUnlockError(DTC_Link_EVB, form.value) ? "x" : " ") + "]");
	for (auto r : DTC_PLLs)
		form.vals.push_back(std::string("PLL Link ") + std::to_string(r) + ":    [" +
							(ReadSERDESPLLUnlockError(r, form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("PLL CFO RX:    [") + (ReadSERDESPLLUnlockError(DTC_PLL_CFO_RX, form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("PLL CFO TX:    [") + (ReadSERDESPLLUnlockError(DTC_PLL_CFO_TX, form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("PLL EVB:       [") + (ReadSERDESPLLUnlockError(DTC_PLL_EVB_TXRX, form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("PLL PunchClock:[") + (ReadSERDESPLLUnlockError(DTC_PLL_PunchedClock, form.value) ? "x" : " ") + "]");
	return form;
}

// SERDES PLL Locked Register
/// <summary>
/// Read if the SERDES PLL is locked for the given SERDES link
/// </summary>
/// <param name="link">Link to read</param>
/// <returns>True if the PLL is locked, false otherwise</returns>
bool DTCLib::DTC_Registers::ReadSERDESPLLLocked(DTC_Link_ID const& link, std::optional<uint32_t> val)
{
	std::bitset<32> dataSet = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_SERDES_PLLLocked);
	return dataSet[link];
}

/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatSERDESPLLLocked()
{
	auto form = CreateFormatter(CFOandDTC_Register_SERDES_PLLLocked);
	form.description = "SERDES PLL Locked";
	form.vals.push_back("([ x = 1 (hi) ])");  // translation
	for (auto r : DTC_ROC_Links)
		form.vals.push_back(std::string("Link ") + std::to_string(r) + ":         [" + (ReadSERDESPLLLocked(r, form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("CFO:            [") + (ReadSERDESPLLLocked(DTC_Link_CFO, form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("EVB:            [") + (ReadSERDESPLLLocked(DTC_Link_EVB, form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("Clock to JA:    [") + ((((form.value) >> 8) & 1) ? "x" : " ") + "]");
	form.vals.push_back(std::string("Clock from JA:  [") + ((((form.value) >> 9) & 1) ? "x" : " ") + "]");
	return form;
}

/// <summary>
/// Disable the SERDES PLL Power Down bit for the given link, enabling that link
/// </summary>
/// <param name="link">Link to set</param>
void DTCLib::DTC_Registers::EnableSERDESPLL(DTC_Link_ID const& link)
{
	std::bitset<32> data = ReadRegister_(CFOandDTC_Register_SERDES_PLLPowerDown);
	data[link] = 0;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_SERDES_PLLPowerDown);
}

/// <summary>
/// Enable the SERDES PLL Power Down bit for the given link, disabling that link
/// </summary>
/// <param name="link">Link to set</param>
void DTCLib::DTC_Registers::DisableSERDESPLL(DTC_Link_ID const& link)
{
	std::bitset<32> data = ReadRegister_(CFOandDTC_Register_SERDES_PLLPowerDown);
	data[link] = 1;
	WriteRegister_(data.to_ulong(), CFOandDTC_Register_SERDES_PLLPowerDown);
}

/// <summary>
/// Read the SERDES PLL Power Down bit for the given link
/// </summary>
/// <param name="link">link to read</param>
/// <returns>Value of the bit</returns>
bool DTCLib::DTC_Registers::ReadSERDESPLLPowerDown(DTC_Link_ID const& link, std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_SERDES_PLLPowerDown);
	return data[link];
}

/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatSERDESPLLPowerDown()
{
	auto form = CreateFormatter(CFOandDTC_Register_SERDES_PLLPowerDown);
	form.description = "SERDES PLL Power Down";
	form.vals.push_back("([ x = 1 (hi) ])");  // translation
	for (auto r : DTC_ROC_Links)
		form.vals.push_back(std::string("Link ") + std::to_string(r) + ": [" + (ReadSERDESPLLPowerDown(r, form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("CFO:    [") + (ReadSERDESPLLPowerDown(DTC_Link_CFO, form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("EVB:    [") + (ReadSERDESPLLPowerDown(DTC_Link_EVB, form.value) ? "x" : " ") + "]");
	return form;
}

// SERDES RX Status Register
/// <summary>
/// Read the SERDES RX Status for the given SERDES Link
/// </summary>
/// <param name="link">Link to read</param>
/// <returns>DTC_RXStatus object</returns>
DTCLib::DTC_RXStatus DTCLib::DTC_Registers::ReadSERDESRXStatus(DTC_Link_ID const& link, std::optional<uint32_t> val)
{
	std::bitset<3> dataSet = (val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_SERDES_RXStatus)) >> 3 * link;
	return static_cast<DTC_RXStatus>(dataSet.to_ulong());
}

/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatSERDESRXStatus()
{
	auto form = CreateFormatter(CFOandDTC_Register_SERDES_RXStatus);
	form.description = "SERDES RX Status";
	form.vals.push_back("");  // translation
	for (auto r : DTC_ROC_Links)
	{
		auto re = ReadSERDESRXStatus(r, form.value);
		form.vals.push_back(std::string("Link ") + std::to_string(r) + ": " + DTC_RXStatusConverter(re).toString());
	}
	auto ce = ReadSERDESRXStatus(DTC_Link_CFO, form.value);
	form.vals.push_back(std::string("CFO:    ") + DTC_RXStatusConverter(ce).toString());

	return form;
}

// SERDES Reset Done Register
/// <summary>
/// Read the SERDES Reset RX FSM Done bit
/// </summary>
/// <param name="link">link to read</param>
/// <returns>Value of the bit</returns>
bool DTCLib::DTC_Registers::ReadResetRXFSMSERDESDone(DTC_Link_ID const& link, std::optional<uint32_t> val)
{
	std::bitset<32> dataSet = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_SERDES_ResetDone);
	return dataSet[link + 24];
}
/// <summary>
/// Read the SERDES Reset RX Done bit
/// </summary>
/// <param name="link">link to read</param>
/// <returns>Value of the bit</returns>
bool DTCLib::DTC_Registers::ReadResetRXSERDESDone(DTC_Link_ID const& link, std::optional<uint32_t> val)
{
	std::bitset<32> dataSet = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_SERDES_ResetDone);
	return dataSet[link + 16];
}
/// <summary>
/// Read the SERDES Reset TX FSM Done bit
/// </summary>
/// <param name="link">link to read</param>
/// <returns>Value of the bit</returns>
bool DTCLib::DTC_Registers::ReadResetTXFSMSERDESDone(DTC_Link_ID const& link, std::optional<uint32_t> val)
{
	std::bitset<32> dataSet = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_SERDES_ResetDone);
	return dataSet[link + 8];
}
/// <summary>
/// Read the SERDES Reset TX Done bit
/// </summary>
/// <param name="link">link to read</param>
/// <returns>Value of the bit</returns>
bool DTCLib::DTC_Registers::ReadResetTXSERDESDone(DTC_Link_ID const& link, std::optional<uint32_t> val)
{
	std::bitset<32> dataSet = val.has_value() ? *val : ReadRegister_(CFOandDTC_Register_SERDES_ResetDone);
	return dataSet[link];
}

/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatSERDESResetDone()
{
	auto form = CreateFormatter(CFOandDTC_Register_SERDES_ResetDone);
	form.description = "SERDES Reset Done";
	form.vals.push_back("       ([RX FSM, RX, TX FSM, TX])");
	for (auto r : DTC_ROC_Links)
		form.vals.push_back(std::string("Link ") + std::to_string(r) + ": [" +
							(ReadResetRXFSMSERDESDone(r, form.value) ? "x" : ".") +
							(ReadResetRXSERDESDone(r, form.value) ? "x" : ".") +
							(ReadResetTXFSMSERDESDone(r, form.value) ? "x" : ".") +
							(ReadResetTXSERDESDone(r, form.value) ? "x" : ".") + "]");
	form.vals.push_back(std::string("CFO:    [") +
						(ReadResetRXFSMSERDESDone(DTC_Link_CFO, form.value) ? "x" : ".") +
						(ReadResetRXSERDESDone(DTC_Link_CFO, form.value) ? "x" : ".") +
						(ReadResetTXFSMSERDESDone(DTC_Link_CFO, form.value) ? "x" : ".") +
						(ReadResetTXSERDESDone(DTC_Link_CFO, form.value) ? "x" : ".") + "]");
	form.vals.push_back(std::string("EVB:    [") +
						(ReadResetRXFSMSERDESDone(DTC_Link_EVB, form.value) ? "x" : ".") +
						(ReadResetRXSERDESDone(DTC_Link_EVB, form.value) ? "x" : ".") +
						(ReadResetTXFSMSERDESDone(DTC_Link_EVB, form.value) ? "x" : ".") +
						(ReadResetTXSERDESDone(DTC_Link_EVB, form.value) ? "x" : ".") + "]");
	return form;
}

// SFP / SERDES Status Register

/// <summary>
/// Read the SERDES CDR Lock bit for the given SERDES Link
/// </summary>
/// <param name="link">Link to read</param>
/// <returns>True if the SERDES CDR Lock bit is set</returns>
bool DTCLib::DTC_Registers::ReadSERDESRXCDRLock(DTC_Link_ID const& link, std::optional<uint32_t> val)
{
	std::bitset<32> dataSet = val.has_value() ? *val : ReadRegister_(DTC_Register_SERDES_RXCDRLockStatus);

	// read the lock value 20x because it might not be stable, consider unlocked if ever low
	if (!val.has_value() && dataSet[link])
	{
		for (int i = 0; i < 20; ++i)
		{
			std::bitset<32> tmpDataSet = ReadRegister_(DTC_Register_SERDES_RXCDRLockStatus);
			if (!tmpDataSet[link])  // override with any exception
				return false;
			usleep(10);
		}
	}
	return dataSet[link];
}

/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatRXCDRLockStatus()
{
	auto form = CreateFormatter(DTC_Register_SERDES_RXCDRLockStatus);
	form.description = "RX CDR Lock Status";
	form.vals.push_back("([ x = 1 (hi) ])");  // translation
	for (auto r : DTC_ROC_Links)
	{
		form.vals.push_back(std::string("ROC Link ") + std::to_string(r) +
							" CDR Lock:   [" + (ReadSERDESRXCDRLock(r) ? "x" : " ") + "]");  // do not use form.value to force multi reread in case of instability
	}
	if (ReadCFOEmulationMode())
		form.vals.push_back(std::string("CFO Emulated CDR Lock: [") + (ReadSERDESRXCDRLock(DTC_Link_CFO, form.value) ? "x" : " ") + "]");
	else
		form.vals.push_back(std::string("CFO CDR Lock:          [") + (ReadSERDESRXCDRLock(DTC_Link_CFO) ? "x" : " ") + "]");  // do not use form.value to force multi reread in case of instability

	form.vals.push_back(std::string("EVB CDR Lock:          [") + (ReadSERDESRXCDRLock(DTC_Link_EVB) ? "x" : " ") + "]");  // do not use form.value to force multi reread in case of instability
	return form;
}

// DMA Timeout Preset Register
/// <summary>
/// Set the maximum time a DMA buffer may be active before it is sent, in 4ns ticks.
/// The default value is 0x800
/// </summary>
/// <param name="preset">Maximum active time for DMA buffers</param>
void DTCLib::DTC_Registers::SetDMATimeoutPreset(uint32_t preset)
{
	WriteRegister_(preset, DTC_Register_DMATimeoutPreset);
}

/// <summary>
/// Read the maximum time a DMA buffer may be active before it is sent, in 4ns ticks.
/// The default value is 0x800
/// </summary>
/// <returns>Maximum active time for DMA buffers</returns>
uint32_t DTCLib::DTC_Registers::ReadDMATimeoutPreset(std::optional<uint32_t> val) { return val.has_value() ? *val : ReadRegister_(DTC_Register_DMATimeoutPreset); }

/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatDMATimeoutPreset()
{
	auto form = CreateFormatter(DTC_Register_DMATimeoutPreset);
	form.description = "DMA Timeout";
	std::stringstream o;
	o << "0x" << std::hex << ReadDMATimeoutPreset(form.value);
	form.vals.push_back(o.str());
	return form;
}

// ROC Timeout Preset Register
/// <summary>
/// Read the timeout between the reception of a Data Header packet from a ROC and receiving all of the associated Data
/// Packets. If a timeout occurrs, the ROCTimeoutError flag will be set. Timeout is in SERDES clock ticks
/// </summary>
/// <returns>Timeout value</returns>
uint32_t DTCLib::DTC_Registers::ReadROCTimeoutPreset(std::optional<uint32_t> val) { return val.has_value() ? *val : ReadRegister_(DTC_Register_ROCReplyTimeout); }

/// <summary>
/// Set the timeout between the reception of a Data Header packet from a ROC and receiving all of the associated Data
/// Packets. If a timeout occurrs, the ROCTimeoutError flag will be set. Timeout is in SERDES clock ticks
/// </summary>
/// <param name="preset">Timeout value. Default: 0x200000</param>
void DTCLib::DTC_Registers::SetROCTimeoutPreset(uint32_t preset)
{
	WriteRegister_(preset, DTC_Register_ROCReplyTimeout);
}

/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatROCReplyTimeout()
{
	auto form = CreateFormatter(DTC_Register_ROCReplyTimeout);
	form.description = "ROC Reply Timeout";
	std::stringstream o;
	o << "0x" << std::hex << ReadROCTimeoutPreset(form.value);
	form.vals.push_back(o.str());
	return form;
}

// ROC Timeout Error Register
/// <summary>
/// Clear the ROC Data Packet timeout error flag for the given SERDES link
/// </summary>
/// <param name="link">Link to clear</param>
bool DTCLib::DTC_Registers::ReadROCTimeoutError(DTC_Link_ID const& link, std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(DTC_Register_ROCReplyTimeoutError);
	return data[static_cast<int>(link)];
}

/// <summary>
/// Read the ROC Data Packet Timeout Error Flag for the given SERDES link
/// </summary>
/// <param name="link">Link to read</param>
/// <returns>True if the error flag is set, false otherwise</returns>
void DTCLib::DTC_Registers::ClearROCTimeoutError(DTC_Link_ID const& link)
{
	std::bitset<32> data = 0x0;
	data[link] = 1;
	WriteRegister_(data.to_ulong(), DTC_Register_ROCReplyTimeoutError);
}

/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatROCReplyTimeoutError()
{
	auto form = CreateFormatter(DTC_Register_ROCReplyTimeoutError);
	form.description = "ROC Reply Timeout Error";
	form.vals.push_back("([ x = 1 (hi) ])");  // translation
	for (auto r : DTC_ROC_Links)
	{
		form.vals.push_back(std::string("Link ") + std::to_string(r) + ": [" +
							(ReadROCTimeoutError(r, form.value) ? "x" : " ") + "]");
	}
	return form;
}

/// <summary>
/// Set the DTC ID and EVB Info register
/// </summary>
/// <returns></returns>
void DTCLib::DTC_Registers::SetEVBInfo(uint8_t dtcid, uint8_t mode,
									   uint8_t partitionId, uint8_t macByte)
{
	uint32_t regVal = dtcid << 24;
	regVal |= mode << 16;
	regVal |= partitionId << 8;
	regVal |= macByte;
	WriteRegister_(regVal, DTC_Register_EVBPartitionID);
}

/// <summary>
/// Set only the DTC ID within the EVB Info register
/// </summary>
/// <returns></returns>
void DTCLib::DTC_Registers::SetDTCID(uint8_t dtcid)
{
	auto regVal = ReadRegister_(DTC_Register_EVBPartitionID) & 0x00FFFFFF;
	regVal += dtcid << 24;
	WriteRegister_(regVal, DTC_Register_EVBPartitionID);
}

// EVB Network Partition ID / EVB Network Local MAC Index Register
uint8_t DTCLib::DTC_Registers::ReadDTCID(std::optional<uint32_t> val)
{
	auto regVal = (val.has_value() ? *val : ReadRegister_(DTC_Register_EVBPartitionID)) & 0xFF000000;
	return static_cast<uint8_t>(regVal >> 24);
}

/// <summary>
/// Set the EVB Mode byte
/// </summary>
/// <param name="mode">New Mode byte</param>
void DTCLib::DTC_Registers::SetEVBMode(uint8_t mode)
{
	auto regVal = ReadRegister_(DTC_Register_EVBPartitionID) & 0xFF00FFFF;
	regVal += mode << 16;
	WriteRegister_(regVal, DTC_Register_EVBPartitionID);
}

/// <summary>
/// Read the EVB Mode byte
/// </summary>
/// <returns>EVB Mode byte</returns>
uint8_t DTCLib::DTC_Registers::ReadEVBMode(std::optional<uint32_t> val)
{
	auto regVal = (val.has_value() ? *val : ReadRegister_(DTC_Register_EVBPartitionID)) & 0xFF0000;
	return static_cast<uint8_t>(regVal >> 16);
}

/// <summary>
/// Set the local partition ID
/// </summary>
/// <param name="id">Local partition ID</param>
void DTCLib::DTC_Registers::SetEVBLocalParitionID(uint8_t partitionId)
{
	auto regVal = ReadRegister_(DTC_Register_EVBPartitionID) & 0xFFFFFCFF;
	regVal += (partitionId & 0x3) << 8;
	WriteRegister_(regVal, DTC_Register_EVBPartitionID);
}

/// <summary>
/// Read the local partition ID
/// </summary>
/// <returns>Partition ID</returns>
uint8_t DTCLib::DTC_Registers::ReadEVBLocalParitionID(std::optional<uint32_t> val)
{
	auto regVal = (val.has_value() ? *val : ReadRegister_(DTC_Register_EVBPartitionID)) & 0xFF0000;
	return static_cast<uint8_t>((regVal >> 8) & 0x3);
}

/// <summary>
/// Set the MAC address for the EVB network (lowest byte)
/// </summary>
/// <param name="macByte">MAC Address</param>
void DTCLib::DTC_Registers::SetEVBLocalMACAddress(uint8_t macByte)
{
	auto regVal = ReadRegister_(DTC_Register_EVBPartitionID) & 0xFFFFFFC0;
	regVal += (macByte & 0x3F);
	WriteRegister_(regVal, DTC_Register_EVBPartitionID);
}

/// <summary>
/// Read the MAC address for the EVB network (lowest byte)
/// </summary>
/// <returns>MAC Address</returns>
uint8_t DTCLib::DTC_Registers::ReadEVBLocalMACAddress(std::optional<uint32_t> val) { return (val.has_value() ? *val : ReadRegister_(DTC_Register_EVBPartitionID)) & 0x3F; }

/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatEVBLocalParitionIDMACIndex()
{
	auto form = CreateFormatter(DTC_Register_EVBPartitionID);
	form.description = "EVB Local Partition ID / MAC Index";
	form.vals.push_back("");  // translation
	std::ostringstream o;
	o << "DTC ID:                         0x" << std::hex << static_cast<int>(ReadDTCID(form.value));
	form.vals.push_back(o.str());
	o.str("");
	o.clear();
	o << "EVB Mode:                       0x" << std::hex << static_cast<int>(ReadEVBMode(form.value));
	form.vals.push_back(o.str());
	o.str("");
	o.clear();
	o << "EVB Parition ID:                0x" << std::hex << static_cast<int>(ReadEVBLocalParitionID(form.value));
	form.vals.push_back(o.str());
	o.str("");
	o.clear();
	o << "EVB Self MAC Address Last Byte: 0x" << std::hex << static_cast<int>(ReadEVBLocalMACAddress(form.value));
	form.vals.push_back(o.str());
	return form;
}

/// EVB Number of Destination Nodes Register
void DTCLib::DTC_Registers::SetEVBClusterInfo(uint16_t deadTime,
											  uint8_t baseDTCAddress, uint8_t numOfDTCs)
{
	uint32_t regVal = (deadTime & 0xFFFF) << 16;
	regVal |= baseDTCAddress << 8;
	regVal |= numOfDTCs;
	WriteRegister_(regVal, DTC_Register_EVBConfiguration);
}

/// <summary>
/// Set the dead time (clocks at start of destination switch to block tx to avoid collissions with previous round-robin tx) of the EVB cluster
/// </summary>
void DTCLib::DTC_Registers::SetEVBDeadTime(uint16_t deadTime)
{
	auto regVal = ReadRegister_(DTC_Register_EVBConfiguration) & 0x0FFFF;
	regVal += (deadTime & 0xFFFF) << 16;
	WriteRegister_(regVal, DTC_Register_EVBConfiguration);
}

/// <summary>
/// Read the dead time (clocks at start of destination switch to block tx to avoid collissions with previous round-robin tx) of the EVB cluster
/// </summary>
uint16_t DTCLib::DTC_Registers::ReadEVBDeadTime(std::optional<uint32_t> val)
{
	return static_cast<uint8_t>((((val.has_value() ? *val : ReadRegister_(DTC_Register_EVBConfiguration)) & 0xFFFF0000)) >> 16);
}

/// <summary>
/// Set the start node in the EVB cluster
/// </summary>
void DTCLib::DTC_Registers::SetEVBStartNode(uint8_t startNode)
{
	auto regVal = ReadRegister_(DTC_Register_EVBConfiguration) & 0xFFFFC0FF;
	regVal += (startNode & 0x3F) << 8;
	WriteRegister_(regVal, DTC_Register_EVBConfiguration);
}

/// <summary>
/// Read the start node in the EVB cluster
/// </summary>
uint8_t DTCLib::DTC_Registers::ReadEVBStartNode(std::optional<uint32_t> val)
{
	return static_cast<uint8_t>((((val.has_value() ? *val : ReadRegister_(DTC_Register_EVBConfiguration)) & 0x3F00)) >> 8);
}

/// <summary>
/// Set the number of destination nodes in the EVB cluster
/// </summary>
/// <param name="numOfNodes">Number of nodes</param>
void DTCLib::DTC_Registers::SetEVBNumberOfDestinationNodes(uint8_t numOfNodes)
{
	auto regVal = ReadRegister_(DTC_Register_EVBConfiguration) & 0xFFFFFFC0;
	regVal += (numOfNodes & 0x3F);
	WriteRegister_(regVal, DTC_Register_EVBConfiguration);
}

/// <summary>
/// Read the number of destination nodes in the EVB cluster
/// </summary>
/// <returns>Number of nodes</returns>
uint8_t DTCLib::DTC_Registers::ReadEVBNumberOfDestinationNodes(std::optional<uint32_t> val)
{
	return static_cast<uint8_t>((val.has_value() ? *val : ReadRegister_(DTC_Register_EVBConfiguration)) & 0x3F);
}

/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatEVBClusterInfo()
{
	auto form = CreateFormatter(DTC_Register_EVBConfiguration);
	form.description = "EVB DTC Cluster Configuration";
	form.vals.push_back("");  // translation
	std::stringstream o;
	o << "EVB Dead Time: " << std::dec << static_cast<int>(ReadEVBDeadTime(form.value));
	form.vals.push_back(o.str());
	o.str("");
	o.clear();
	o << "EVB Start Node: " << std::dec << static_cast<int>(ReadEVBStartNode(form.value));
	form.vals.push_back(o.str());
	o.str("");
	o.clear();
	o << "EVB Number of Destination Nodes: " << std::dec << static_cast<int>(ReadEVBNumberOfDestinationNodes(form.value));
	form.vals.push_back(o.str());
	return form;
}

/// Hardware Event Building Packet Control Info Register
void DTCLib::DTC_Registers::SetEVBPacketControlInfo(uint16_t idlePacketWordCount,
													uint8_t interpacketGap, uint8_t loopbackCalibratedOffset)
{
	uint32_t regVal = idlePacketWordCount << 16;
	regVal |= interpacketGap << 8;
	regVal |= loopbackCalibratedOffset;
	WriteRegister_(regVal, DTC_Register_EVBPacketControl);
}

/// Read the number of words in EVB idle packets
uint16_t DTCLib::DTC_Registers::ReadEVBIdlePacketWordCount(std::optional<uint32_t> val)
{
	return static_cast<uint16_t>((val.has_value() ? *val : ReadRegister_(DTC_Register_EVBPacketControl)) >> 16);
}
/// Read the EVB interpacket gap in units of GbE transmit clocks
uint8_t DTCLib::DTC_Registers::ReadEVBInterpacketGap(std::optional<uint32_t> val)
{
	return static_cast<uint8_t>((val.has_value() ? *val : ReadRegister_(DTC_Register_EVBPacketControl)) >> 8);
}
/// Read the DTC's EVB delay offset in units of 200MHz CFO clocks, which should be calibrated by CFO-DTC loopback
uint8_t DTCLib::DTC_Registers::ReadEVBLoopbackCalibratedOffset(std::optional<uint32_t> val)
{
	return static_cast<uint8_t>((val.has_value() ? *val : ReadRegister_(DTC_Register_EVBPacketControl)) >> 0);
}

/// Formats the Hardware Event Building Packet Control Info Register's current value for register dumps
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatEVBPacketControlInfo()
{
	auto form = CreateFormatter(DTC_Register_EVBPacketControl);
	form.description = "EVB Packet Control Info";
	form.vals.push_back("");  // translation
	std::stringstream o;
	o << "Idle Packet Word Count:                           " << std::dec << static_cast<int>(ReadEVBIdlePacketWordCount(form.value));
	form.vals.push_back(o.str());
	o.str("");
	o.clear();
	o << "Interpacket Gap [GbE tx clocks]:                  " << std::dec << static_cast<int>(ReadEVBInterpacketGap(form.value));
	form.vals.push_back(o.str());
	o.str("");
	o.clear();
	o << "DTC's Loopback Calibrated Offset [200MHz clocks]: " << std::dec << static_cast<int>(ReadEVBLoopbackCalibratedOffset(form.value));
	form.vals.push_back(o.str());
	return form;
}

/// Read theHardware Event Building Stats data based on type and DTC mac address
uint32_t DTCLib::DTC_Registers::ReadEVBStats(DTC_EVBStatsType type, uint8_t dtc_mac, std::optional<uint32_t> val)
{
	if (val.has_value())                              // assum address is already setup when value is passed, and just read updated stat value
		return ReadRegister_(DTC_Register_EVBStats);  //*val;

	uint32_t t = static_cast<uint8_t>(type) << DTC_EVBStatsType_BRAM_TYPE_SIZE;
	if (dtc_mac >= (1 << DTC_EVBStatsType_BRAM_TYPE_SIZE))
	{
		__SS__ << "Illegal DTC MAC adress value: " << (int)dtc_mac << " must be < 2^" << DTC_EVBStatsType_BRAM_TYPE_SIZE << __E__;
		ss << otsStyleStackTrace() << __E__;
		__SS_THROW__;
	}
	t |= dtc_mac;  // address fully built now
	__COUTT__ << "Read EVB Stat address: 0x" << std::hex << t << __E__;

	WriteRegister_(t, DTC_Register_EVBStats);

	return ReadRegister_(DTC_Register_EVBStats);
}  // end ReadEVBStats()

// Get the last packet count and its timestamp for a DTC
std::pair<uint32_t, std::chrono::steady_clock::time_point> DTCLib::DTC_Registers::getPacketCountInfo(uint8_t dtc) const
{
	auto i = lastPacketCount.find(dtc);
	if (i != lastPacketCount.end())
		return i->second;
	return {0, std::chrono::steady_clock::time_point::min()};
}

// Update the packet count and timestamp and calculate packet rate for a DTC
void DTCLib::DTC_Registers::updatePacketCount(uint8_t dtc, uint32_t currentCount)
{
	auto now = std::chrono::steady_clock::now();

	auto [lastCount, lastTime] = getPacketCountInfo(dtc);
	if (lastTime == std::chrono::steady_clock::time_point::min())
	{
		lastPacketCount[dtc] = {currentCount, now};
		return;
	}

	auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now - lastTime).count();
	if (ns > 1e6)
	{
		double rate = (currentCount - lastCount) / (ns / 1e9);
		lastPacketRate[dtc] = rate;
		lastPacketCount[dtc] = {currentCount, now};
	}
}

// Get the most recent packet rate for a DTC
double DTCLib::DTC_Registers::getLastPacketRate(uint8_t dtc) const
{
	auto i = lastPacketRate.find(dtc);
	return i != lastPacketRate.end() ? i->second : 0.0;
}

/// Formats the Hardware Event Building Stats data for all DTC mac addresses, as specified by the EVB Info 'Number Of Destination Nodes'
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatEVBStats(DTCLib::DTC_EVBStatsType type /*  = DTC_EVBStatsType::DTC_EVBStatsType_All */)
{
	uint8_t t = static_cast<uint8_t>(type);
	if (type == DTC_EVBStatsType::DTC_EVBStatsType_All)
		t = 0;
	__COUTV__(t);

	auto form = CreateFormatter(DTC_Register_EVBStats, false /* getValue */);
	form.description = "EVB Stats";
	form.vals.push_back("");  // translation
	std::stringstream o;

	uint8_t baseDTCAddress = ReadEVBStartNode();
	uint8_t numOfDTCs = ReadEVBNumberOfDestinationNodes();

	__COUTV__(numOfDTCs);

	uint32_t idlePacketWordCount = (static_cast<uint32_t>(ReadRegister_(DTC_Register_EVBPacketControl)) >> 16);

	for (; t < DTC_EVBStatsType_BRAM_TYPE_COUNT; ++t)
	{
		for (uint8_t d = 0; d < numOfDTCs; ++d)
		{
			uint32_t v = ReadEVBStats(DTC_EVBStatsType(t), d);

			o << "BRAM Stat - ";
			switch (DTC_EVBStatsType(t))
			{
				case DTC_EVBStatsType_RxCount:
					if (v > 0 &&  // start time for rates as soon as there is non-zero data
						EVB_startDataTime ==
							std::chrono::steady_clock::time_point::min())
						EVB_startDataTime = std::chrono::steady_clock::now();

					EVB_endDataTime = std::chrono::steady_clock::now();

					o << "Received Packet Rate:                  ";
					o << "DTC_mac #" << (baseDTCAddress + d < 10 ? "0" : "") << std::dec << int(baseDTCAddress + d) << " = ";

					{
						auto now = std::chrono::steady_clock::now();
						auto [lastCount, lastTime] = getPacketCountInfo(d);

						if (lastTime == std::chrono::steady_clock::time_point::min())
						{
							updatePacketCount(d, v);
							o << "Initializing";
						}
						else
						{
							auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now - lastTime).count();

							if (ns > 1e6)  // prevent divide by 0
							{
								double rate = (v - lastCount) / (ns / 1e9);
								o << rate << " Packets/s";

								updatePacketCount(d, v);
							}
							else
								o << "T too short.";
						}
					}
					form.vals.push_back(o.str());
					o.str("");
					o.clear();

					o << "Bram Stat - Received Packet Count:                 ";
					break;
				case DTC_EVBStatsType_RxLastSequenceTag:
					o << "Last Received Sequence Tag:            ";
					break;
				case DTC_EVBStatsType_RxMissingPacketCount:
					o << "Rx Missing Packet Count:               ";
					break;
				case DTC_EVBStatsType_RxByteCount:
					o << "Received Byte Rate:                    ";
					o << "DTC_mac #" << (baseDTCAddress + d < 10 ? "0" : "") << std::dec << int(baseDTCAddress + d) << " = ";

					o << getLastPacketRate(d) * idlePacketWordCount * 8 << " Byte/s";
					form.vals.push_back(o.str());
					o.str("");
					o.clear();

					o << "Bram Stat - Received Byte Count:                   ";
					break;
				case DTC_EVBStatsType_RxLastPacketArrival:
					o << "Rx Last Packet Arrival [10gbe clocks]: ";
					break;
				case DTC_EVBStatsType_TxLastSequenceTag:
					o << "Last Transmitted Sequence Tag:         ";
					break;
				default:
					__SS__ << "Invalid DTC EVB Stat type: " << t << __E__;
					__SS_THROW__;
			}
			o << "DTC_mac #" << (baseDTCAddress + d < 10 ? "0" : "") << std::dec << int(baseDTCAddress + d) << " = " << std::hex << "0x" << std::setw(8) << v << "\t" << std::dec << v;
			form.vals.push_back(o.str());
			o.str("");
			o.clear();
		}  // end DTC mac address loop
		if (type != DTC_EVBStatsType::DTC_EVBStatsType_All) break;
	}  // end type loop

	return form;
}  // end FormatEVBStats()

/// <summary>
/// Read the Reset bit of the SERDES IIC Bus
/// </summary>
/// <returns>Reset bit value</returns>
bool DTCLib::DTC_Registers::ReadSERDESOscillatorIICInterfaceReset(std::optional<uint32_t> val)
{
	auto dataSet = std::bitset<32>(val.has_value() ? *val : ReadRegister_(DTC_Register_SERDESClock_IICBusControl));
	return dataSet[31];
}

/// <summary>
/// Reset the SERDES IIC Bus
/// </summary>
void DTCLib::DTC_Registers::ResetSERDESOscillatorIICInterface()
{
	auto bs = std::bitset<32>();
	bs[31] = 1;
	WriteRegister_(bs.to_ulong(), DTC_Register_SERDESClock_IICBusControl);
	while (ReadSERDESOscillatorIICInterfaceReset())
	{
		usleep(1000);
	}
}

/// <summary>
/// Read the current SERDES Oscillator clock speed
/// </summary>
/// <returns>Current SERDES clock speed</returns>
DTCLib::DTC_SerdesClockSpeed DTCLib::DTC_Registers::ReadSERDESOscillatorClock(std::optional<uint32_t> val)
{
	// DTC register definition was removed, all DTC versions only were returning constant value
	auto freq = 0x09502f90;  // 156.25mhz // ReadSERDESOscillatorReferenceFrequency(DTC_IICSERDESBusAddress_EVB, val);

	// Clocks should be accurate to 30 ppm
	if (freq > 156250000 - 4687.5 && freq < 156250000 + 4687.5) return DTC_SerdesClockSpeed_3125Gbps;
	if (freq > 125000000 - 3750 && freq < 125000000 + 3750) return DTC_SerdesClockSpeed_25Gbps;
	return DTC_SerdesClockSpeed_Unknown;
}
/// <summary>
/// Set the SERDES Oscillator clock speed for the given SERDES transfer rate
/// </summary>
/// <param name="speed">Clock speed to set</param>
void DTCLib::DTC_Registers::SetSERDESOscillatorClock(DTC_SerdesClockSpeed speed)
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
		default:
			targetFreq = 0.0;
			break;
	}
	if (SetNewOscillatorFrequency(DTC_OscillatorType_SERDES, targetFreq))
	{
		for (auto& link : DTC_ROC_Links)
		{
			ResetSERDES(link, 1000);
		}
		ResetSERDES(DTC_Link_CFO, 1000);
		// ResetSERDES(DTC_Link_EVB, 1000);
	}
}

/// <summary>
/// Set the Timing Oscillator clock to a given frequency
/// </summary>
/// <param name="freq">Frequency to set the Timing card Oscillator clock</param>
void DTCLib::DTC_Registers::SetTimingOscillatorClock(uint32_t freq)
{
	double targetFreq = freq;
	if (SetNewOscillatorFrequency(DTC_OscillatorType_Timing, targetFreq))
	{
		ResetSERDES(DTC_Link_CFO, 1000);
		// ResetSERDES(DTC_Link_EVB, 1000);
	}
}

// /// <summary>
// /// Formats the register's current value for register dumps
// /// </summary>
// /// <returns>RegisterFormatter object containing register information</returns>
// DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatTimingSERDESOscillatorFrequency()
// {
// 	auto form = CreateFormatter(DTC_Register_SERDESTimingCardOscillatorFrequency);
// 	form.description = "SERDES Timing Card Oscillator Reference Frequency";
// 	std::stringstream o;
// 	o << std::dec << ReadSERDESOscillatorReferenceFrequency(DTC_IICSERDESBusAddress_CFO, form.value);
// 	form.vals.push_back(o.str());
// 	return form;
// }

// /// <summary>
// /// Formats the register's current value for register dumps
// /// </summary>
// /// <returns>RegisterFormatter object containing register information</returns>
// DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatMainBoardSERDESOscillatorFrequency()
// {
// 	auto form = CreateFormatter(DTC_Register_SERDESReferenceClockFrequency);
// 	form.description = "SERDES Main Board Oscillator Reference Frequency";
// 	std::stringstream o;
// 	o << std::dec << ReadSERDESOscillatorReferenceFrequency(DTC_IICSERDESBusAddress_EVB, form.value);
// 	form.vals.push_back(o.str());
// 	return form;
// }
/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatSERDESOscillatorControl()
{
	auto form = CreateFormatter(DTC_Register_SERDESClock_IICBusControl);
	form.description = "SERDES Oscillator IIC Bus Control";
	form.vals.push_back(std::string("Reset:  [") + (ReadSERDESOscillatorIICInterfaceReset(form.value) ? "x" : " ") + "]");
	return form;
}

// FIFO Full Error Flags Registers
/// <summary>
/// Clear all FIFO Full Error Flags for the given link
/// </summary>
/// <param name="link">Link to clear</param>
void DTCLib::DTC_Registers::ClearFIFOFullErrorFlags(DTC_Link_ID const& link)
{
	auto flags = ReadFIFOFullErrorFlags(link);
	std::bitset<32> data0 = 0;
	std::bitset<32> data1 = 0;
	std::bitset<32> data2 = 0;

	data0[link] = flags.OutputData;
	data0[link + 8] = flags.CFOLinkInput;
	data0[link + 16] = flags.ReadoutRequestOutput;
	data0[link + 24] = flags.DataRequestOutput;
	data1[link] = flags.OtherOutput;
	data1[link + 8] = flags.OutputDCS;
	data1[link + 16] = flags.OutputDCSStage2;
	data1[link + 24] = flags.DataInput;
	data2[link] = flags.DCSStatusInput;

	WriteRegister_(data0.to_ulong(), DTC_Register_FIFOFullErrorFlag0);
	WriteRegister_(data1.to_ulong(), DTC_Register_FIFOFullErrorFlag1);
	WriteRegister_(data2.to_ulong(), DTC_Register_FIFOFullErrorFlag2);
}

/// <summary>
/// Read the FIFO Full Error/Status Flags for the given link
/// </summary>
/// <param name="link">Link to read</param>
/// <returns>DTC_FIFOFullErrorFlags object</returns>
DTCLib::DTC_FIFOFullErrorFlags DTCLib::DTC_Registers::ReadFIFOFullErrorFlags(DTC_Link_ID const& link, std::optional<uint32_t> val0,
																			 std::optional<uint32_t> val1, std::optional<uint32_t> val2)
{
	std::bitset<32> data0 = val0.has_value() ? *val0 : ReadRegister_(DTC_Register_FIFOFullErrorFlag0);
	std::bitset<32> data1 = val1.has_value() ? *val1 : ReadRegister_(DTC_Register_FIFOFullErrorFlag1);
	std::bitset<32> data2 = val2.has_value() ? *val2 : ReadRegister_(DTC_Register_FIFOFullErrorFlag2);
	DTC_FIFOFullErrorFlags flags;

	flags.OutputData = data0[link];
	flags.CFOLinkInput = data0[link + 8];
	flags.ReadoutRequestOutput = data0[link + 16];
	flags.DataRequestOutput = data0[link + 24];
	flags.OtherOutput = data1[link];
	flags.OutputDCS = data1[link + 8];
	flags.OutputDCSStage2 = data1[link + 16];
	flags.DataInput = data1[link + 24];
	flags.DCSStatusInput = data2[link];

	return flags;
}

/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatFIFOFullErrorFlag0()
{
	auto form = CreateFormatter(DTC_Register_FIFOFullErrorFlag0);
	form.description = "FIFO Full Error Flags 0";
	form.vals.push_back("       ([DataRequest, ReadoutRequest, CFOLink, OutputData])");
	for (auto r : DTC_ROC_Links)
	{
		auto re = ReadFIFOFullErrorFlags(r, form.value);
		form.vals.push_back(std::string("Link ") + std::to_string(r) + ": [" + (re.DataRequestOutput ? "x" : " ") + "," +
							(re.ReadoutRequestOutput ? "x" : " ") + "," + (re.CFOLinkInput ? "x" : " ") + "," +
							(re.OutputData ? "x" : " ") + "]");
	}
	return form;
}

/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatFIFOFullErrorFlag1()
{
	auto form = CreateFormatter(DTC_Register_FIFOFullErrorFlag1);
	form.description = "FIFO Full Error Flags 1";
	form.vals.push_back("       ([DataInput, OutputDCSStage2, OutputDCS, OtherOutput])");
	for (auto r : DTC_ROC_Links)
	{
		auto re = ReadFIFOFullErrorFlags(r, form.value);
		form.vals.push_back(std::string("Link ") + std::to_string(r) + ": [" + (re.DataInput ? "x" : " ") + "," +
							(re.OutputDCSStage2 ? "x" : " ") + "," + (re.OutputDCS ? "x" : " ") + "," +
							(re.OtherOutput ? "x" : " ") + "]");
	}
	{
		auto ce = ReadFIFOFullErrorFlags(DTC_Link_CFO, form.value);
		form.vals.push_back(std::string("CFO:    [") + +(ce.DataInput ? "x" : " ") + "," +
							(ce.OutputDCSStage2 ? "x" : " ") + "," + (ce.OutputDCS ? "x" : " ") + "," +
							(ce.OtherOutput ? "x" : " ") + "]");
	}
	{
		auto ce = ReadFIFOFullErrorFlags(DTC_Link_EVB, form.value);
		form.vals.push_back(std::string("EVB:    [") + +(ce.DataInput ? "x" : " ") + "," +
							(ce.OutputDCSStage2 ? "x" : " ") + "," + (ce.OutputDCS ? "x" : " ") + "," +
							(ce.OtherOutput ? "x" : " ") + "]");
	}
	return form;
}

/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatFIFOFullErrorFlag2()
{
	auto form = CreateFormatter(DTC_Register_FIFOFullErrorFlag2);
	form.description = "FIFO Full Error Flags 2";
	form.vals.push_back("       ([DCSStatusInput])");
	for (auto r : DTC_ROC_Links)
	{
		auto re = ReadFIFOFullErrorFlags(r, form.value);
		form.vals.push_back(std::string("Link ") + std::to_string(r) + ": [" + (re.DCSStatusInput ? "x" : " ") + "]");
	}
	{
		auto ce = ReadFIFOFullErrorFlags(DTC_Link_CFO, form.value);
		form.vals.push_back(std::string("CFO:    [") + (ce.DCSStatusInput ? "x" : " ") + "]");
	}
	{
		auto ce = ReadFIFOFullErrorFlags(DTC_Link_EVB, form.value);
		form.vals.push_back(std::string("EVB:    [") + (ce.DCSStatusInput ? "x" : " ") + "]");
	}
	return form;
}

// Receive Packet Error Register
/// <summary>
/// Clear the Packet Error Flag for the given link
/// </summary>
/// <param name="link">Link to clear</param>
void DTCLib::DTC_Registers::ClearPacketError(DTC_Link_ID const& link)
{
	std::bitset<32> data = ReadRegister_(DTC_Register_ReceivePacketError);
	data[static_cast<int>(link) + 8] = 0;
	WriteRegister_(data.to_ulong(), DTC_Register_ReceivePacketError);
}

/// <summary>
/// Read the Packet Error Flag for the given link
/// </summary>
/// <param name="link">Link to read</param>
/// <returns>True if the Packet Error Flag is set</returns>
bool DTCLib::DTC_Registers::ReadPacketError(DTC_Link_ID const& link, std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(DTC_Register_ReceivePacketError);
	return data[static_cast<int>(link) + 8];
}

/// <summary>
/// Clear the Packet CRC Error Flag for the given link
/// </summary>
/// <param name="link">Link to clear</param>
void DTCLib::DTC_Registers::ClearPacketCRCError(DTC_Link_ID const& link)
{
	std::bitset<32> data = ReadRegister_(DTC_Register_ReceivePacketError);
	data[static_cast<int>(link)] = 0;
	WriteRegister_(data.to_ulong(), DTC_Register_ReceivePacketError);
}

/// <summary>
/// Read the Packet CRC Error Flag for the given link
/// </summary>
/// <param name="link">Link to read</param>
/// <returns>True if the Packet CRC Error Flag is set</returns>
bool DTCLib::DTC_Registers::ReadPacketCRCError(DTC_Link_ID const& link, std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(DTC_Register_ReceivePacketError);
	return data[static_cast<int>(link)];
}

/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatReceivePacketError()
{
	auto form = CreateFormatter(DTC_Register_ReceivePacketError);
	form.description = "Receive Packet Error";
	form.vals.push_back("       ([CRC, PacketError])");
	for (auto r : DTC_ROC_Links)
		form.vals.push_back(std::string("Link ") + std::to_string(r) + ": [" +
							(ReadPacketCRCError(r, form.value) ? "x" : " ") + "," +
							(ReadPacketError(r, form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("CFO:    [") +
						(ReadPacketCRCError(DTC_Link_CFO, form.value) ? "x" : " ") + "," +
						(ReadPacketError(DTC_Link_CFO, form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("EVB:    [") +
						(ReadPacketCRCError(DTC_Link_EVB, form.value) ? "x" : " ") + "," +
						(ReadPacketError(DTC_Link_EVB, form.value) ? "x" : " ") + "]");
	return form;
}

// CFO Emulation Timestamp Registers
/// <summary>
/// Set the starting DTC_EventWindowTag for the CFO Emulator
/// </summary>
/// <param name="ts">Starting Timestamp for CFO Emulation</param>
void DTCLib::DTC_Registers::SetCFOEmulationTimestamp(const DTC_EventWindowTag& ts)
{
	auto timestamp = ts.GetEventWindowTag();
	auto timestampLow = static_cast<uint32_t>(timestamp.to_ulong());
	timestamp >>= 32;
	auto timestampHigh = static_cast<uint16_t>(timestamp.to_ulong());

	WriteRegister_(timestampLow, DTC_Register_CFOEmulation_TimestampLow);
	WriteRegister_(timestampHigh, DTC_Register_CFOEmulation_TimestampHigh);
}

/// <summary>
/// Read the starting DTC_EventWindowTag for the CFO Emulator
/// </summary>
/// <returns>DTC_EventWindowTag object</returns>
DTCLib::DTC_EventWindowTag DTCLib::DTC_Registers::ReadCFOEmulationTimestamp(std::optional<uint32_t> val)
{
	auto timestampLow = val.has_value() ? *val : ReadRegister_(DTC_Register_CFOEmulation_TimestampLow);
	DTC_EventWindowTag output;
	output.SetEventWindowTag(timestampLow, static_cast<uint16_t>(ReadRegister_(DTC_Register_CFOEmulation_TimestampHigh)));
	return output;
}

/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatCFOEmulationTimestampLow()
{
	auto form = CreateFormatter(DTC_Register_CFOEmulation_TimestampLow);
	form.description = "CFO Emulation Timestamp Low";
	std::stringstream o;
	o << "0x" << std::hex << ReadRegister_(DTC_Register_CFOEmulation_TimestampLow);
	form.vals.push_back(o.str());
	return form;
}

/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatCFOEmulationTimestampHigh()
{
	auto form = CreateFormatter(DTC_Register_CFOEmulation_TimestampHigh);
	form.description = "CFO Emulation Timestamp High";
	std::stringstream o;
	o << "0x" << std::hex << ReadRegister_(DTC_Register_CFOEmulation_TimestampHigh);
	form.vals.push_back(o.str());
	return form;
}

// CFO Emulation Delay Measure
/// <summary>
/// Read the Delay Measure acquired with the CFO Emulator Loopback Test
/// </summary>
uint32_t DTCLib::DTC_Registers::ReadCFOEmulationLoopbackDelayMeasure(std::optional<uint32_t> val)
{
	uint32_t readVal = val.has_value() ? *val : ReadRegister_(DTC_Register_CFOEmulation_LoopbackDelayMeasure);
	uint32_t i = 0;
	while (!(readVal >> 31))
	{
		if (i > 1000)
		{
			__SS__ << "Timeout looking for the CFO Emulator Loopback Test to complete... readval: 0x" << std::hex << readVal << __E__;
			__SS_THROW__;
		}

		usleep(10000);
		readVal = ReadRegister_(DTC_Register_CFOEmulation_LoopbackDelayMeasure);
		++i;
	}
	return readVal & (~(1 << 31));  // clear the Ready bit from return value
}

/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatCFOEmulationLoopbackDelayMeasure()
{
	auto form = CreateFormatter(DTC_Register_CFOEmulation_LoopbackDelayMeasure);
	form.description = "CFO Emu. Loopback Delay Measure";
	std::stringstream o;
	o << "0x" << std::hex << ReadCFOEmulationLoopbackDelayMeasure(form.value) << std::dec << "(" << double(1.0) * ReadCFOEmulationLoopbackDelayMeasure(form.value) * 5 /*ns for 200MHz period*/ / 8.0 /* for 8 samples */ << " ns)";
	form.vals.push_back(o.str());
	return form;
}

// CFO Emulation Request Interval Register
/// <summary>
/// Set the clock interval between CFO Emulator Event Windows.
/// If 0, specifies to execute the On/Off Spill emulation of Event Window intervals.
/// </summary>
/// <param name="interval">Clock cycles between Event Window Markers</param>
void DTCLib::DTC_Registers::SetCFOEmulationEventWindowInterval(uint32_t interval)
{
	WriteRegister_(interval, DTC_Register_CFOEmulation_HeartbeatInterval);
}

/// <summary>
/// Read the clock interval between CFO Emulator Event Windows.
/// If 0, specifies to execute the On/Off Spill emulation of Event Window intervals.
/// </summary>
/// <returns>Clock cycles between Event Window Markers</returns>
uint32_t DTCLib::DTC_Registers::ReadCFOEmulationEventWindowInterval(std::optional<uint32_t> val)
{
	return val.has_value() ? *val : ReadRegister_(DTC_Register_CFOEmulation_HeartbeatInterval);
}

/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatCFOEmulationEventWindowInterval()
{
	auto form = CreateFormatter(DTC_Register_CFOEmulation_HeartbeatInterval);
	form.description = "CFO Emu. EWM Interval";
	std::stringstream o;
	o << "0x" << std::hex << ReadCFOEmulationEventWindowInterval(form.value);
	form.vals.push_back(o.str());
	return form;
}

// CFO Emulation Number of Requests Register
/// <summary>
/// Set the number of Readout Requests the CFO Emulator is configured to send.
/// A value of 0 means that the CFO Emulator will send requests continuously.
/// </summary>
/// <param name="NumHeartbeats">Number of Readout Requests the CFO Emulator will send</param>
void DTCLib::DTC_Registers::SetCFOEmulationNumHeartbeats(uint32_t NumHeartbeats)
{
	WriteRegister_(NumHeartbeats, DTC_Register_CFOEmulation_NumHeartbeats);
}

/// <summary>
/// Reads the number of Readout Requests the CFO Emulator is configured to send.
/// A value of 0 means that the CFO Emulator will send requests continuously.
/// </summary>
/// <returns>Number of Readout Requests the CFO Emulator will send</returns>
uint32_t DTCLib::DTC_Registers::ReadCFOEmulationNumHeartbeats(std::optional<uint32_t> val)
{
	return val.has_value() ? *val : ReadRegister_(DTC_Register_CFOEmulation_NumHeartbeats);
}

/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatCFOEmulationNumHeartbeats()
{
	auto form = CreateFormatter(DTC_Register_CFOEmulation_NumHeartbeats);
	form.description = "CFO Emulator Number Requests";
	std::stringstream o;
	o << "0x" << std::hex << ReadCFOEmulationNumHeartbeats(form.value);
	form.vals.push_back(o.str());
	return form;
}

// CFO Emulation Number of Packets Registers
/// <summary>
/// Set the number of packets the CFO Emulator will request from the link
/// </summary>
/// <param name="link">Link to set</param>
/// <param name="numPackets">Number of packets to request</param>
void DTCLib::DTC_Registers::SetROCEmulationNumPackets(DTC_Link_ID const& link_in, uint16_t numPackets)
{
	uint16_t data = numPackets & 0x7FF;
	DTC_Register reg;

	for (const auto& link : DTC_ROC_Links)
	{
		if (!(link_in == DTC_Link_ALL || link_in == link))
			continue;  // skip ROC links that should not be set

		switch (link)
		{
			case DTC_Link_0:
			case DTC_Link_1:
				reg = DTC_Register_ROCEmulation_NumPacketsLinks10;
				break;
			case DTC_Link_2:
			case DTC_Link_3:
				reg = DTC_Register_ROCEmulation_NumPacketsLinks32;
				break;
			case DTC_Link_4:
			case DTC_Link_5:
				reg = DTC_Register_ROCEmulation_NumPacketsLinks54;
				break;
			default: {
				__SS__ << "Illegal link " << link << " specified." << __E__;
				__SS_THROW__;
			}
		}

		auto regval = ReadRegister_(reg);
		auto upper = (regval & 0xFFFF0000) >> 16;
		auto lower = regval & 0x0000FFFF;
		if (link == DTC_Link_0 || link == DTC_Link_2 || link == DTC_Link_4)
		{
			lower = data;
		}
		else
		{
			upper = data;
		}
		WriteRegister_((upper << 16) + lower, reg);
	}  // end link write loop
}

/// <summary>
/// Read the requested number of packets the ROC Emulator will generate from the given link
/// </summary>
/// <param name="link">Link to read</param>
/// <returns>Number of packets generated by the ROC Emulator</returns>
uint16_t DTCLib::DTC_Registers::ReadROCEmulationNumPackets(DTC_Link_ID const& link, std::optional<uint32_t> val)
{
	DTC_Register reg;
	switch (link)
	{
		case DTC_Link_0:
		case DTC_Link_1:
			reg = DTC_Register_ROCEmulation_NumPacketsLinks10;
			break;
		case DTC_Link_2:
		case DTC_Link_3:
			reg = DTC_Register_ROCEmulation_NumPacketsLinks32;
			break;
		case DTC_Link_4:
		case DTC_Link_5:
			reg = DTC_Register_ROCEmulation_NumPacketsLinks54;
			break;
		default: {
			__SS__ << "Illegal link " << link << " specified." << __E__;
			__SS_THROW__;
		}
	}

	auto regval = val.has_value() ? *val : ReadRegister_(reg);
	auto upper = (regval & 0xFFFF0000) >> 16;
	auto lower = regval & 0x0000FFFF;
	if (link == DTC_Link_0 || link == DTC_Link_2 || link == DTC_Link_4)
	{
		return static_cast<uint16_t>(lower);
	}
	return static_cast<uint16_t>(upper);
}

/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatROCEmulationNumPacketsLink01()
{
	auto form = CreateFormatter(DTC_Register_ROCEmulation_NumPacketsLinks10);
	form.description = "ROC Emulator Num Packets R0,1";
	form.vals.push_back("");  // translation
	std::stringstream o;
	o << "Link 0: 0x" << std::hex << ReadROCEmulationNumPackets(DTC_Link_0, form.value);
	form.vals.push_back(o.str());
	o.str("");
	o.clear();
	o << "Link 1: 0x" << std::hex << ReadROCEmulationNumPackets(DTC_Link_1, form.value);
	form.vals.push_back(o.str());
	return form;
}

/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatROCEmulationNumPacketsLink23()
{
	auto form = CreateFormatter(DTC_Register_ROCEmulation_NumPacketsLinks32);
	form.description = "ROC Emulator Num Packets R2,3";
	form.vals.push_back("");  // translation
	std::stringstream o;
	o << "Link 2: 0x" << std::hex << ReadROCEmulationNumPackets(DTC_Link_2, form.value);
	form.vals.push_back(o.str());
	o.str("");
	o.clear();
	o << "Link 3: 0x" << std::hex << ReadROCEmulationNumPackets(DTC_Link_3, form.value);
	form.vals.push_back(o.str());
	return form;
}

/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatROCEmulationNumPacketsLink45()
{
	auto form = CreateFormatter(DTC_Register_ROCEmulation_NumPacketsLinks54);
	form.description = "ROC Emulator Num Packets R4,5";
	form.vals.push_back("");  // translation
	std::stringstream o;
	o << "Link 4: 0x" << std::hex << ReadROCEmulationNumPackets(DTC_Link_4, form.value);
	form.vals.push_back(o.str());
	o.str("");
	o.clear();
	o << "Link 5: 0x" << std::hex << ReadROCEmulationNumPackets(DTC_Link_5, form.value);
	form.vals.push_back(o.str());
	return form;
}

// CFO Emulation Number of Null Heartbeats Register
/// <summary>
/// Set the number of null heartbeats the CFO Emulator will generate following the requested ones
/// </summary>
/// <param name="count">Number of null heartbeats to generate</param>
void DTCLib::DTC_Registers::SetCFOEmulationNumNullHeartbeats(const uint32_t& count)
{
	WriteRegister_(count, DTC_Register_CFOEmulation_NumNullHeartbeats);
}

/// <summary>
/// Read the requested number of null heartbeats that will follow the configured heartbeats from the CFO Emulator
/// </summary>
/// <returns>Number of null heartbeats to follow "live" heartbeats</returns>
uint32_t DTCLib::DTC_Registers::ReadCFOEmulationNumNullHeartbeats(std::optional<uint32_t> val)
{
	return val.has_value() ? *val : ReadRegister_(DTC_Register_CFOEmulation_NumNullHeartbeats);
}

/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatCFOEmulationNumNullHeartbeats()
{
	auto form = CreateFormatter(DTC_Register_CFOEmulation_NumNullHeartbeats);
	form.description = "CFO Emulator Num Null Heartbeats";
	form.vals.push_back(std::to_string(ReadCFOEmulationNumNullHeartbeats(form.value)));
	return form;
}

void DTCLib::DTC_Registers::SetCFOEventModeRequiredMask(const uint32_t& mask)
{
	WriteRegister_(mask, DTC_Register_CFOEventModeRequiredMask);
}
uint32_t DTCLib::DTC_Registers::ReadCFOEventModeRequiredMask(std::optional<uint32_t> val)
{
	return val.has_value() ? *val : ReadRegister_(DTC_Register_CFOEventModeRequiredMask);
}
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatCFOEventModeRequiredMask()
{
	auto form = CreateFormatter(DTC_Register_CFOEventModeRequiredMask);
	form.description = "CFO Emulation Event Mode Required Mask";
	std::stringstream o;
	o << "0x" << std::hex << ReadCFOEventModeRequiredMask(form.value);
	form.vals.push_back(o.str());
	return form;
}

// CFO Emulation Event Mode Bytes Registers
/// <summary>
/// Set the CFO Emulation Event Mode 48 bits
/// </summary>
void DTCLib::DTC_Registers::SetCFOEmulationEventMode(const uint64_t& eventMode)
{
	WriteRegister_(eventMode, DTC_Register_CFOEmulation_EventMode1);
	WriteRegister_(eventMode >> 32, DTC_Register_CFOEmulation_EventMode2);
}
uint64_t DTCLib::DTC_Registers::ReadCFOEmulationEventMode()
{
	uint64_t eventMode = ReadRegister_(DTC_Register_CFOEmulation_EventMode2);
	eventMode = ReadRegister_(DTC_Register_CFOEmulation_EventMode1) |
				(eventMode << 32);
	return eventMode;
}

/// <summary>
/// Set the given CFO Emulation Mode byte to the given value
/// </summary>
/// <param name="byteNum">Byte to set. Valid range is 0-5</param>
/// <param name="data">Data for byte</param>
void DTCLib::DTC_Registers::SetCFOEmulationModeByte(const uint8_t& byteNum, uint8_t data)
{
	DTC_Register reg;
	if (byteNum == 0 || byteNum == 1 || byteNum == 2 || byteNum == 3)
	{
		reg = DTC_Register_CFOEmulation_EventMode1;
	}
	else if (byteNum == 4 || byteNum == 5)
	{
		reg = DTC_Register_CFOEmulation_EventMode2;
	}
	else
	{
		__SS__ << "Illegal byte index requested: " << byteNum << ". The Emulated Event Mode is only 6 bytes." << __E__;
		__SS_THROW__;
	}
	auto regVal = ReadRegister_(reg);

	switch (byteNum)
	{
		case 0:
			regVal = (regVal & 0xFFFFFF00) + data;
			break;
		case 1:
			regVal = (regVal & 0xFFFF00FF) + (data << 8);
			break;
		case 2:
			regVal = (regVal & 0xFF00FFFF) + (data << 16);
			break;
		case 3:
			regVal = (regVal & 0x00FFFFFF) + (data << 24);
			break;
		case 4:
			regVal = (regVal & 0xFF00) + data;
			break;
		case 5:
			regVal = (regVal & 0x00FF) + (data << 8);
			break;
		default:
			// impossible
			{
				__SS__ << "Illegal byte index requested: " << byteNum << ". The Emulated Event Mode is only 6 bytes." << __E__;
				__SS_THROW__;
			}
	}

	WriteRegister_(regVal, reg);
}

/// <summary>
/// Read the given CFO Emulation Mode byte
/// </summary>
/// <param name="byteNum">Byte to read. Valid range is 0-5</param>
/// <returns>Current value of the mode byte</returns>
uint8_t DTCLib::DTC_Registers::ReadCFOEmulationModeByte(const uint8_t& byteNum, std::optional<uint32_t> val)
{
	DTC_Register reg;
	if (byteNum == 0 || byteNum == 1 || byteNum == 2 || byteNum == 3)
	{
		reg = DTC_Register_CFOEmulation_EventMode1;
	}
	else if (byteNum == 4 || byteNum == 5)
	{
		reg = DTC_Register_CFOEmulation_EventMode2;
	}
	else
	{
		__SS__ << "Illegal byte index requested: " << byteNum << ". The Emulated Event Mode is only 6 bytes." << __E__;
		__SS_THROW__;
	}
	auto regVal = val.has_value() ? *val : ReadRegister_(reg);

	switch (byteNum)
	{
		case 0:
			return static_cast<uint8_t>(regVal & 0xFF);
		case 1:
			return static_cast<uint8_t>((regVal & 0xFF00) >> 8);
		case 2:
			return static_cast<uint8_t>((regVal & 0xFF0000) >> 16);
		case 3:
			return static_cast<uint8_t>((regVal & 0xFF000000) >> 24);
		case 4:
			return static_cast<uint8_t>(regVal & 0xFF);
		case 5:
			return static_cast<uint8_t>((regVal & 0xFF00) >> 8);
		default:
			// impossible
			{
				__SS__ << "Illegal byte index requested: " << byteNum << ". The Emulated Event Mode is only 6 bytes." << __E__;
				__SS_THROW__;
			}
	}
}

/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatCFOEmulationModeBytes03()
{
	auto form = CreateFormatter(DTC_Register_CFOEmulation_EventMode1);
	form.description = "CFO Emulation Event Mode Bytes 0-3";
	form.vals.push_back("");  // translation
	std::ostringstream o;
	o << "Byte 0: 0x" << std::hex << static_cast<int>(ReadCFOEmulationModeByte(0, form.value));
	form.vals.push_back(o.str());
	o.str("");
	o.clear();
	o << "Byte 1: 0x" << std::hex << static_cast<int>(ReadCFOEmulationModeByte(1, form.value));
	form.vals.push_back(o.str());
	o.str("");
	o.clear();
	o << "Byte 2: 0x" << std::hex << static_cast<int>(ReadCFOEmulationModeByte(2, form.value));
	form.vals.push_back(o.str());
	o.str("");
	o.clear();
	o << "Byte 3: 0x" << std::hex << static_cast<int>(ReadCFOEmulationModeByte(3, form.value));
	form.vals.push_back(o.str());
	return form;
}

/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatCFOEmulationModeBytes45()
{
	auto form = CreateFormatter(DTC_Register_CFOEmulation_EventMode2);
	form.description = "CFO Emulation Event Mode Bytes 4-5";
	form.vals.push_back("");  // translation
	std::ostringstream o;
	o << "Byte 4: 0x" << std::hex << static_cast<int>(ReadCFOEmulationModeByte(4, form.value));
	form.vals.push_back(o.str());
	o.str("");
	o.clear();
	o << "Byte 5: 0x" << std::hex << static_cast<int>(ReadCFOEmulationModeByte(5, form.value));
	form.vals.push_back(o.str());
	o.str("");
	o.clear();
	return form;
}

/// <summary>
/// Set the DebugType used by the CFO Emulator
/// </summary>
/// <param name="type">The DTC_DebugType the CFO Emulator will fill into Readout Requests</param>
// void DTCLib::DTC_Registers::SetCFOEmulationDebugType(DTC_DebugType type)
//{
//	std::bitset<32> data = type & 0xF;
//	data[16] = ReadDebugPacketMode();
//	WriteRegister_(data.to_ulong(), DTC_Register_DebugPacketType);
// }

/// <summary>
/// Read the DebugType field filled into Readout Requests generated by the CFO Emulator
/// </summary>
/// <returns>The DTC_DebugType used by the CFO Emulator</returns>
// DTCLib::DTC_DebugType DTCLib::DTC_Registers::ReadCFOEmulationDebugType(std::optional<uint32_t> val)
//{
//	return static_cast<DTC_DebugType>((0xFFFF & val.has_value()) ? *val : ReadRegister_(DTC_Register_DebugPacketType));
// }

/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
// DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatCFOEmulationDebugPacketType()
//{
//	auto form = CreateFormatter(DTC_Register_DebugPacketType);
//	form.description = "CFO Emulation Debug Packet Type";
//	form.vals.push_back("([ x = 1 (hi) ])");  // translation
//	form.vals.push_back(std::string("Debug Mode: [") + (ReadDebugPacketMode(form.value) ? "x" : " ") + "]");
//	std::stringstream o;
//	o << "Debug Packet Type: 0x" << std::hex << ReadCFOEmulationDebugType(form.value);
//	form.vals.push_back(o.str());
//	return form;
// }

// RX Packet Count Error Flags Register
/// <summary>
/// Read the RX Packet Count Error flag for the given link
/// </summary>
/// <param name="link">Link to read</param>
/// <returns>Whether the RX Packet Count Error flag is set on the link</returns>
bool DTCLib::DTC_Registers::ReadRXPacketCountErrorFlags(DTC_Link_ID const& link, std::optional<uint32_t> val)
{
	std::bitset<32> dataSet = val.has_value() ? *val : ReadRegister_(DTC_Register_RXPacketCountErrorFlags);
	return dataSet[link];
}

/// <summary>
/// Clear the RX Packet Count Error flag for the given link
/// </summary>
/// <param name="link">Link to clear</param>
void DTCLib::DTC_Registers::ClearRXPacketCountErrorFlags(DTC_Link_ID const& link)
{
	std::bitset<32> dataSet;
	dataSet[link] = true;
	WriteRegister_(dataSet.to_ulong(), DTC_Register_RXPacketCountErrorFlags);
}

/// <summary>
/// Clear all RX Packet Count Error Flags
/// </summary>
void DTCLib::DTC_Registers::ClearRXPacketCountErrorFlags()
{
	std::bitset<32> dataSet = ReadRegister_(DTC_Register_RXPacketCountErrorFlags);
	WriteRegister_(dataSet.to_ulong(), DTC_Register_RXPacketCountErrorFlags);
}

/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatRXPacketCountErrorFlags()
{
	auto form = CreateFormatter(DTC_Register_RXPacketCountErrorFlags);
	form.description = "RX Packet Count Error Flags";
	form.vals.push_back("([ x = 1 (hi) ])");  // translation
	for (auto r : DTC_ROC_Links)
		form.vals.push_back(std::string("Link ") + std::to_string(r) + ": [" +
							(ReadRXPacketCountErrorFlags(r, form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("CFO:    [") + (ReadRXPacketCountErrorFlags(DTC_Link_CFO, form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("EVB:    [") + (ReadRXPacketCountErrorFlags(DTC_Link_EVB, form.value) ? "x" : " ") + "]");
	return form;
}

// Detector Emulator DMA Count Register
/// <summary>
/// Set the number of DMAs that the Detector Emulator will generate when enabled
/// </summary>
/// <param name="count">The number of DMAs that the Detector Emulator will generate when enabled</param>
void DTCLib::DTC_Registers::SetDetectorEmulationDMACount(uint32_t count)
{
	WriteRegister_(count, DTC_Register_DetEmulation_DMACount);
}

/// <summary>
/// Read the number of DMAs that the Detector Emulator will generate
/// </summary>
/// <returns>The number of DMAs that the Detector Emulator will generate</returns>
uint32_t DTCLib::DTC_Registers::ReadDetectorEmulationDMACount(std::optional<uint32_t> val)
{
	return val.has_value() ? *val : ReadRegister_(DTC_Register_DetEmulation_DMACount);
}

/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatDetectorEmulationDMACount()
{
	auto form = CreateFormatter(DTC_Register_DetEmulation_DMACount);
	form.description = "DetEmu DMA Count";
	std::stringstream o;
	o << "0x" << std::hex << ReadDetectorEmulationDMACount(form.value);
	form.vals.push_back(o.str());
	return form;
}

// Detector Emulator DMA Delay Counter Register
/// <summary>
/// Set the delay between DMAs in Detector Emulator mode
/// </summary>
/// <param name="count">Delay between DMAs in Detector Emulation mode, in 4ns ticks</param>
void DTCLib::DTC_Registers::SetDetectorEmulationDMADelayCount(uint32_t count)
{
	WriteRegister_(count, DTC_Register_DetEmulation_DelayCount);
}

/// <summary>
/// Read the amount of the delay between DMAs in Detector Emulator mode
/// </summary>
/// <returns>The amount of the delay between DMAs in Detector Emulator mode, in 4ns ticks</returns>
uint32_t DTCLib::DTC_Registers::ReadDetectorEmulationDMADelayCount(std::optional<uint32_t> val)
{
	return val.has_value() ? *val : ReadRegister_(DTC_Register_DetEmulation_DelayCount);
}

/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatDetectorEmulationDMADelayCount()
{
	auto form = CreateFormatter(DTC_Register_DetEmulation_DelayCount);
	form.description = "DetEmu DMA Delay Count";
	std::stringstream o;
	o << "0x" << std::hex << ReadDetectorEmulationDMADelayCount(form.value);
	form.vals.push_back(o.str());
	return form;
}

/// <summary>
/// Enable Detector Emulator Mode. This sends all DMA writes to DMA channel 0 (DAQ) to DDR memory
/// </summary>
void DTCLib::DTC_Registers::EnableDetectorEmulatorMode()
{
	std::bitset<32> data = ReadRegister_(DTC_Register_DetEmulation_Control0);
	data[0] = 1;
	WriteRegister_(data.to_ulong(), DTC_Register_DetEmulation_Control0);
}

/// <summary>
/// Disable sending DMA data to DDR memory
/// </summary>
void DTCLib::DTC_Registers::DisableDetectorEmulatorMode()
{
	std::bitset<32> data = ReadRegister_(DTC_Register_DetEmulation_Control0);
	data[0] = 0;
	WriteRegister_(data.to_ulong(), DTC_Register_DetEmulation_Control0);
}

/// <summary>
/// Read whether writes to DMA Channel 0 will be loaded into DDR memory
/// </summary>
/// <returns>Whether the Detector Emulator Mode bit is set</returns>
bool DTCLib::DTC_Registers::ReadDetectorEmulatorMode(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(DTC_Register_DetEmulation_Control0);
	return data[0];
}

/// <summary>
/// Enable the Detector Emulator (Playback Mode)
/// This assumes that data has been loaded into DDR memory using DMA Channel 0 before enabling.
/// </summary>
void DTCLib::DTC_Registers::EnableDetectorEmulator()
{
	std::bitset<32> data = ReadRegister_(DTC_Register_DetEmulation_Control0);
	data[1] = 1;
	WriteRegister_(data.to_ulong(), DTC_Register_DetEmulation_Control0);
}

/// <summary>
/// Turn off the Detector Emulator (Playback Mode)
/// </summary>
void DTCLib::DTC_Registers::DisableDetectorEmulator()
{
	std::bitset<32> data = ReadRegister_(DTC_Register_DetEmulation_Control1);
	data[1] = 1;
	WriteRegister_(data.to_ulong(), DTC_Register_DetEmulation_Control1);
}

/// <summary>
/// Read whether the Detector Emulator is enabled
/// </summary>
/// <returns>Whether the Detector Emulator is enabled</returns>
bool DTCLib::DTC_Registers::ReadDetectorEmulatorEnable(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(DTC_Register_DetEmulation_Control0);
	return data[1];
}

/// <summary>
/// Read whether a Detector Emulator Disable operation is in progress
/// </summary>
/// <returns>Whether the Detector Emulator Enable Clear bit is set</returns>
bool DTCLib::DTC_Registers::ReadDetectorEmulatorEnableClear(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(DTC_Register_DetEmulation_Control1);
	return data[1];
}

/// <summary>
/// Clear the "Detector Emulator In Use" virtual register
/// </summary>
void DTCLib::DTC_Registers::ClearDetectorEmulatorInUse()
{
	DisableDetectorEmulator();
	DisableDetectorEmulatorMode();
	ResetDDRWriteAddress();
	ResetDDRReadAddress();
	SetDDRDataLocalStartAddress(0);
	SetDDRDataLocalEndAddress(0x7000000);
	usingDetectorEmulator_ = false;
}

/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatDetectorEmulationControl0()
{
	auto form = CreateFormatter(DTC_Register_DetEmulation_Control0);
	form.description = "Detector Emulation Control 0";
	form.vals.push_back("([ x = 1 (hi) ])");  // translation
	form.vals.push_back(std::string("Detector Emulation Enable: [") + (ReadDetectorEmulatorEnable(form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("Detector Emulation Mode:   [") + (ReadDetectorEmulatorMode(form.value) ? "x" : " ") + "]");
	return form;
}

/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatDetectorEmulationControl1()
{
	auto form = CreateFormatter(DTC_Register_DetEmulation_Control1);
	form.description = "Detector Emulation Control 1";
	form.vals.push_back(std::string("Detector Emulation Enable Clear: [") +
						(ReadDetectorEmulatorEnableClear(form.value) ? "x" : " ") + "]");
	return form;
}

// DDR Event Data Local Start Address Register
/// <summary>
/// Set the DDR Data Start Address
/// DDR Addresses are in bytes and must be 64-bit aligned
/// </summary>
/// <param name="address">Start address for the DDR data section</param>
void DTCLib::DTC_Registers::SetDDRDataLocalStartAddress(uint32_t address)
{
	WriteRegister_(address, DTC_Register_DetEmulation_DataStartAddress);
}

/// <summary>
/// Read the DDR Data Start Address
/// </summary>
/// <returns>The current DDR data start address</returns>
uint32_t DTCLib::DTC_Registers::ReadDDRDataLocalStartAddress(std::optional<uint32_t> val)
{
	return val.has_value() ? *val : ReadRegister_(DTC_Register_DetEmulation_DataStartAddress);
}

/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatDDRDataLocalStartAddress()
{
	auto form = CreateFormatter(DTC_Register_DetEmulation_DataStartAddress);
	form.description = "DDR Event Data Local Start Address";
	std::stringstream o;
	o << "0x" << std::hex << ReadDDRDataLocalStartAddress(form.value);
	form.vals.push_back(o.str());
	return form;
}

// DDR Event Data Local End Address Register
/// <summary>
/// Set the end address for the DDR data section
/// DDR Addresses are in bytes and must be 64-bit aligned
/// </summary>
/// <param name="address"></param>
void DTCLib::DTC_Registers::SetDDRDataLocalEndAddress(uint32_t address)
{
	WriteRegister_(address, DTC_Register_DetEmulation_DataEndAddress);
}

/// <summary>
/// Read the current end address for the DDR Data section
/// </summary>
/// <returns>End address for the DDR data section</returns>
uint32_t DTCLib::DTC_Registers::ReadDDRDataLocalEndAddress(std::optional<uint32_t> val)
{
	return val.has_value() ? *val : ReadRegister_(DTC_Register_DetEmulation_DataEndAddress);
}

/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatDDRDataLocalEndAddress()
{
	auto form = CreateFormatter(DTC_Register_DetEmulation_DataEndAddress);
	form.description = "DDR Event Data Local End Address";
	std::stringstream o;
	o << "0x" << std::hex << ReadDDRDataLocalEndAddress(form.value);
	form.vals.push_back(o.str());
	return form;
}

uint32_t DTCLib::DTC_Registers::ReadCFOEmulatorInterpacketDelay(std::optional<uint32_t> val)
{
	return val.has_value() ? *val : ReadRegister_(DTC_Register_CFOEmulation_DataRequestDelay);
}

DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatCFOEmulatorInterpacketDelay()
{
	auto form = CreateFormatter(DTC_Register_CFOEmulation_DataRequestDelay);
	form.description = "CFO Emulator Data Request Interpacket Delay";
	std::stringstream o;
	o << std::dec << ReadCFOEmulatorInterpacketDelay(form.value) << " * 5 ns";
	form.vals.push_back(o.str());
	return form;
}

/// <summary>
/// Read the current maximum Ethernet payload size
/// </summary>
/// <returns>The current maximum Ethernet payload size</returns>
uint32_t DTCLib::DTC_Registers::ReadEthernetPayloadSize(std::optional<uint32_t> val)
{
	return val.has_value() ? *val : ReadRegister_(DTC_Register_EthernetFramePayloadSize);
}

/// <summary>
/// Set the maximum Ethernet payload size, in bytes. Maximum is 1492 bytes.
/// </summary>
/// <param name="size">Maximum Ethernet payload size, in bytes</param>
void DTCLib::DTC_Registers::SetEthernetPayloadSize(uint32_t size)
{
	if (size > 1492)
	{
		size = 1492;
	}
	WriteRegister_(size, DTC_Register_EthernetFramePayloadSize);
}

/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatEthernetPayloadSize()
{
	auto form = CreateFormatter(DTC_Register_EthernetFramePayloadSize);
	form.description = "Ethernet Frame Payload Max Size";
	std::stringstream o;
	o << std::dec << ReadEthernetPayloadSize(form.value) << " bytes";
	form.vals.push_back(o.str());
	return form;
}

uint32_t DTCLib::DTC_Registers::ReadCFOEmulation40MHzMarkerInterval(std::optional<uint32_t> val)
{
	return val.has_value() ? *val : ReadRegister_(DTC_Register_CFOEmulation_40MHzClockMarkerInterval);
}

void DTCLib::DTC_Registers::SetCFOEmulation40MHzMarkerInterval(uint32_t interval)
{
	WriteRegister_(interval, DTC_Register_CFOEmulation_40MHzClockMarkerInterval);
}

DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatCFOEmulation40MHzMarkerInterval()
{
	auto form = CreateFormatter(DTC_Register_CFOEmulation_40MHzClockMarkerInterval);
	form.description = "CFO Emulation 40MHz Marker Interval";
	std::stringstream o;
	o << "0x" << std::hex << ReadCFOEmulation40MHzMarkerInterval(form.value);
	form.vals.push_back(o.str());
	return form;
}

// bool DTCLib::DTC_Registers::ReadCFOEmulationEventStartMarkerEnable(DTC_Link_ID const& link, std::optional<uint32_t> val)
// {
// 	std::bitset<32> dataSet = val.has_value()?*val:ReadRegister_(DTC_Register_CFOMarkerEnables);
// 	return dataSet[link + 8];
// }

// void DTCLib::DTC_Registers::SetCFOEmulationEventStartMarkerEnable(DTC_Link_ID const& link, bool enable)
// {
// 	std::bitset<32> data = ReadRegister_(DTC_Register_CFOMarkerEnables);
// 	data[link + 8] = enable;
// 	WriteRegister_(data.to_ulong(), DTC_Register_CFOMarkerEnables);
// }

bool DTCLib::DTC_Registers::ReadCFO40MHzClockMarkerEnable(DTC_Link_ID const& link, std::optional<uint32_t> val)
{
	std::bitset<32> dataSet = val.has_value() ? *val : ReadRegister_(DTC_Register_CFOMarkerEnables);
	return dataSet[link];
}

void DTCLib::DTC_Registers::SetCFO40MHzClockMarkerEnable(DTC_Link_ID const& link, bool enable)
{
	std::bitset<32> data = ReadRegister_(DTC_Register_CFOMarkerEnables);
	if (link == DTC_Link_ALL)
	{
		for (uint8_t i = 0; i < 6; ++i)  // just ROC links
			data[i] = enable;
	}
	else
		data[link] = enable;
	WriteRegister_(data.to_ulong(), DTC_Register_CFOMarkerEnables);
}

DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatCFO40MHzClockMarkerEnables()
{
	auto form = CreateFormatter(CFOandDTC_Register_SERDES_LoopbackEnable);
	form.description = "CFO Emulation Marker Enables";
	// form.vals.push_back("        [Event Start, 40 MHz]");
	form.vals.push_back("        [40 MHz Clock Marker]");
	for (auto r : DTC_ROC_Links)
		form.vals.push_back(std::string("Link ") + std::to_string(r) + ": [" +
							// (ReadCFOEventStartMarkerEnable(r, form.value) ? "x" : " ") + "," +
							(ReadCFO40MHzClockMarkerEnable(r, form.value) ? "x" : " ") + "]");
	return form;
}

uint8_t DTCLib::DTC_Registers::ReadROCCommaLimit(std::optional<uint32_t> val)
{
	return static_cast<uint8_t>(val.has_value() ? *val : ReadRegister_(DTC_Register_ROCFinishThreshold));
}

void DTCLib::DTC_Registers::SetROCCommaLimit(uint8_t limit)
{
	uint32_t data = ReadRegister_(DTC_Register_ROCFinishThreshold) & 0xFFFFFF00;
	data += limit;
	WriteRegister_(data, DTC_Register_ROCFinishThreshold);
}

DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatROCFinishThreshold()
{
	auto form = CreateFormatter(DTC_Register_ROCFinishThreshold);
	form.description = "ROC Finish Threshold";
	std::stringstream o;
	o << "ROC Comma Limit: 0x" << std::hex << static_cast<int>(ReadROCCommaLimit(form.value));
	form.vals.push_back(o.str());
	return form;
}

bool DTCLib::DTC_Registers::ReadTXPRBSForceError(DTC_Link_ID const& link, std::optional<uint32_t> val)
{
	auto dataSet = std::bitset<32>(val.has_value() ? *val : ReadRegister_(DTC_Register_TXPRBSControl));
	return dataSet[link + 24];
}

void DTCLib::DTC_Registers::SetTXPRBSForceError(DTC_Link_ID const& link)
{
	auto dataSet = std::bitset<32>(ReadRegister_(DTC_Register_TXPRBSControl));
	dataSet[link + 24] = 1;
	WriteRegister_(dataSet.to_ulong(), DTC_Register_TXPRBSControl);
}

void DTCLib::DTC_Registers::ClearTXPRBSForceError(DTC_Link_ID const& link)
{
	auto dataSet = std::bitset<32>(ReadRegister_(DTC_Register_TXPRBSControl));
	dataSet[link + 24] = 0;
	WriteRegister_(dataSet.to_ulong(), DTC_Register_TXPRBSControl);
}

DTCLib::DTC_PRBSMode DTCLib::DTC_Registers::ReadTXPRBSMode(DTC_Link_ID const& link, std::optional<uint32_t> val)
{
	auto data = val.has_value() ? *val : ReadRegister_(DTC_Register_TXPRBSControl);
	auto masked = (data >> link) & 0x7;
	return static_cast<DTC_PRBSMode>(masked);
}

void DTCLib::DTC_Registers::SetTXPRBSMode(DTC_Link_ID const& link, DTC_PRBSMode mode)
{
	auto data = ReadRegister_(DTC_Register_TXPRBSControl);
	auto masked = data & ~(0x7 << link);
	auto newMode = static_cast<uint32_t>(mode);
	WriteRegister_(masked + (newMode << link), DTC_Register_TXPRBSControl);
}

DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatSERDESTXPRBSControl()
{
	auto form = CreateFormatter(DTC_Register_TXPRBSControl);
	form.description = "SERDES TX PRBS Control";
	form.vals.push_back("       ([Force Error, Mode])");
	for (auto r : DTC_ROC_Links)
		form.vals.push_back(std::string("Link ") + std::to_string(r) + ": [" +
							(ReadTXPRBSForceError(r, form.value) ? "x" : " ") + "," +
							(DTC_PRBSModeConverter(ReadTXPRBSMode(r, form.value)).toString()) + "]");
	form.vals.push_back(std::string("CFO:    [") + (ReadTXPRBSForceError(DTC_Link_CFO, form.value) ? "x" : " ") + "," +
						(DTC_PRBSModeConverter(ReadTXPRBSMode(DTC_Link_CFO, form.value)).toString()) + "]");
	return form;
}

bool DTCLib::DTC_Registers::ReadRXPRBSError(DTC_Link_ID const& link, std::optional<uint32_t> val)
{
	auto dataSet = std::bitset<32>(val.has_value() ? *val : ReadRegister_(DTC_Register_RXPRBSControl));
	return dataSet[link + 24];
}

DTCLib::DTC_PRBSMode DTCLib::DTC_Registers::ReadRXPRBSMode(DTC_Link_ID const& link, std::optional<uint32_t> val)
{
	auto data = val.has_value() ? *val : ReadRegister_(DTC_Register_RXPRBSControl);
	auto masked = (data >> link) & 0x7;
	return static_cast<DTC_PRBSMode>(masked);
}

void DTCLib::DTC_Registers::SetRXPRBSMode(DTC_Link_ID const& link, DTC_PRBSMode mode)
{
	auto data = ReadRegister_(DTC_Register_RXPRBSControl);
	auto masked = data & ~(0x7 << link);
	auto newMode = static_cast<uint32_t>(mode);
	WriteRegister_(masked + (newMode << link), DTC_Register_RXPRBSControl);
}

DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatSERDESRXPRBSControl()
{
	auto form = CreateFormatter(DTC_Register_RXPRBSControl);
	form.description = "SERDES RX PRBS Control";
	form.vals.push_back("       ([Error, Mode])");
	for (auto r : DTC_ROC_Links)
		form.vals.push_back(std::string("Link ") + std::to_string(r) + ": [" +
							(ReadRXPRBSError(r, form.value) ? "x" : " ") + "," +
							(DTC_PRBSModeConverter(ReadRXPRBSMode(r, form.value)).toString()) + "]");
	form.vals.push_back(std::string("CFO:    [") +
						(ReadRXPRBSError(DTC_Link_CFO, form.value) ? "x" : " ") + "," +
						(DTC_PRBSModeConverter(ReadRXPRBSMode(DTC_Link_CFO, form.value)).toString()) + "]");
	return form;
}

bool DTCLib::DTC_Registers::ReadEventModeTableEnable(std::optional<uint32_t> val)
{
	auto dataSet = std::bitset<32>(val.has_value() ? *val : ReadRegister_(DTC_Register_EventModeLookupTableControl));
	return dataSet[16];
}

void DTCLib::DTC_Registers::SetEventModeTableEnable()
{
	auto dataSet = std::bitset<32>(ReadRegister_(DTC_Register_EventModeLookupTableControl));
	dataSet[16] = 1;
	WriteRegister_(dataSet.to_ulong(), DTC_Register_EventModeLookupTableControl);
}

void DTCLib::DTC_Registers::ClearEventModeTableEnable()
{
	auto dataSet = std::bitset<32>(ReadRegister_(DTC_Register_EventModeLookupTableControl));
	dataSet[16] = 0;
	WriteRegister_(dataSet.to_ulong(), DTC_Register_EventModeLookupTableControl);
}

uint8_t DTCLib::DTC_Registers::ReadEventModeLookupByteSelect(std::optional<uint32_t> val)
{
	auto data = val.has_value() ? *val : ReadRegister_(DTC_Register_EventModeLookupTableControl);
	auto masked = (data) & 0x7;
	return static_cast<uint8_t>(masked);
}

void DTCLib::DTC_Registers::SetEventModeLookupByteSelect(uint8_t byte)
{
	auto data = ReadRegister_(DTC_Register_EventModeLookupTableControl);
	auto masked = data & 0xFFF8;
	WriteRegister_(masked + (byte & 0x7), DTC_Register_EventModeLookupTableControl);
}

DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatEventModeLookupTableControl()
{
	auto form = CreateFormatter(DTC_Register_EventModeLookupTableControl);
	form.description = "Event Mode Lookup Table Control";
	form.vals.push_back("([ x = 1 (hi) ])");  // translation
	form.vals.push_back(std::string("Enabled:  [") +
						(ReadEventModeTableEnable(form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("Byte:          [") +
						std::to_string(ReadEventModeLookupByteSelect(form.value)) + "]");
	return form;
}

bool DTCLib::DTC_Registers::ReadDDRMemoryTestComplete(std::optional<uint32_t> val)
{
	auto dataSet = std::bitset<32>(val.has_value() ? *val : ReadRegister_(DTC_Register_DD3TestRegister));
	return dataSet[8];
}

bool DTCLib::DTC_Registers::ReadDDRMemoryTestError(std::optional<uint32_t> val)
{
	auto dataSet = std::bitset<32>(val.has_value() ? *val : ReadRegister_(DTC_Register_DD3TestRegister));
	return dataSet[0];
}

void DTCLib::DTC_Registers::ClearDDRMemoryTestError()
{
	WriteRegister_(1, DTC_Register_DD3TestRegister);
}

DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatDDRMemoryTestRegister()
{
	auto form = CreateFormatter(DTC_Register_DD3TestRegister);
	form.description = "DDR3 Memory Test";
	form.vals.push_back("([ x = 1 (hi) ])");  // translation
	form.vals.push_back(std::string("Test Complete:  [") +
						(ReadDDRMemoryTestComplete(form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("Error:          [") +
						(ReadDDRMemoryTestError(form.value) ? "x" : " ") + "]");
	return form;
}

// SERDES Serial Inversion Enable Register
/// <summary>
/// Read the Invert SERDES RX Input bit
/// </summary>
/// <param name="link">Link to read</param>
/// <returns>Whether the Invert SERDES RX Input bit is set</returns>
bool DTCLib::DTC_Registers::ReadInvertSERDESRXInput(DTC_Link_ID const& link, std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(DTC_Register_SERDESTXRXInvertEnable);
	return data[link + 8];
}
/// <summary>
/// Set the Invert SERDES RX Input bit
/// </summary>
/// <param name="link">Link to set</param>
/// <param name="invert">Whether to invert</param>
void DTCLib::DTC_Registers::SetInvertSERDESRXInput(DTC_Link_ID const& link, bool invert)
{
	std::bitset<32> data = ReadRegister_(DTC_Register_SERDESTXRXInvertEnable);
	data[link + 8] = invert;
	WriteRegister_(data.to_ulong(), DTC_Register_SERDESTXRXInvertEnable);
}
/// <summary>
/// Read the Invert SERDES TX Output bit
/// </summary>
/// <param name="link">Link to read</param>
/// <returns>Whether the Invert SERDES TX Output bit is set</returns>
bool DTCLib::DTC_Registers::ReadInvertSERDESTXOutput(DTC_Link_ID const& link, std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(DTC_Register_SERDESTXRXInvertEnable);
	return data[link];
}
/// <summary>
/// Set the Invert SERDES TX Output bit
/// </summary>
/// <param name="link">Link to set</param>
/// <param name="invert">Whether to invert</param>
void DTCLib::DTC_Registers::SetInvertSERDESTXOutput(DTC_Link_ID const& link, bool invert)
{
	std::bitset<32> data = ReadRegister_(DTC_Register_SERDESTXRXInvertEnable);
	data[link] = invert;
	WriteRegister_(data.to_ulong(), DTC_Register_SERDESTXRXInvertEnable);
}
/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatSERDESSerialInversionEnable()
{
	auto form = CreateFormatter(DTC_Register_SERDESTXRXInvertEnable);
	form.description = "SERDES Serial Inversion Enable";
	form.vals.push_back("       ([Input, Output])");
	for (auto r : DTC_ROC_Links)
		form.vals.push_back(std::string("Link ") + std::to_string(r) + ": [" +
							(ReadInvertSERDESRXInput(r, form.value) ? "x" : " ") + "," +
							(ReadInvertSERDESTXOutput(r, form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("CFO:    [") +
						(ReadInvertSERDESRXInput(DTC_Link_CFO, form.value) ? "x" : " ") + "," +
						(ReadInvertSERDESTXOutput(DTC_Link_CFO, form.value) ? "x" : " ") + "]");

	return form;
}

// Jitter Attenuator CSR Register
/// <summary>
/// Read the value of the Jitter Attenuator Select
/// </summary>
/// <returns>Jitter Attenuator Select value</returns>
std::bitset<2> DTCLib::DTC_Registers::ReadJitterAttenuatorSelect(std::optional<uint32_t> val) { return CFOandDTC_Registers::ReadJitterAttenuatorSelect(DTC_Register_JitterAttenuatorCSR); }

/// <summary>
/// Set the Jitter Attenuator Select bits. JA reset only needed after a power cycle
/// </summary>
/// <param name="data">Value to set</param>
void DTCLib::DTC_Registers::SetJitterAttenuatorSelect(std::bitset<2> data, bool alsoResetJA /* = false */) { CFOandDTC_Registers::SetJitterAttenuatorSelect(DTC_Register_JitterAttenuatorCSR, data, alsoResetJA); }

/// <summary>
/// Read the Jitter Attenuator Reset bit
/// </summary>
/// <returns>Value of the Jitter Attenuator Reset bit</returns>
bool DTCLib::DTC_Registers::ReadJitterAttenuatorReset(std::optional<uint32_t> val) { return CFOandDTC_Registers::ReadJitterAttenuatorReset(DTC_Register_JitterAttenuatorCSR, val); }
bool DTCLib::DTC_Registers::ReadJitterAttenuatorLocked(std::optional<uint32_t> val) { return CFOandDTC_Registers::ReadJitterAttenuatorLocked(DTC_Register_JitterAttenuatorCSR, val); }

/// <summary>
/// Reset the Jitter Attenuator
/// </summary>
void DTCLib::DTC_Registers::ResetJitterAttenuator() { CFOandDTC_Registers::ResetJitterAttenuator(DTC_Register_JitterAttenuatorCSR); }

/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatJitterAttenuatorCSR() { return CFOandDTC_Registers::FormatJitterAttenuatorCSR(DTC_Register_JitterAttenuatorCSR); }

// SFP IIC Registers

/// <summary>
/// Read the Reset bit of the SFP IIC Bus
/// </summary>
/// <returns>Reset bit value</returns>
bool DTCLib::DTC_Registers::ReadSFPIICInterfaceReset(std::optional<uint32_t> val)
{
	auto dataSet = std::bitset<32>(val.has_value() ? *val : ReadRegister_(DTC_Register_SFP_IICBusControl));
	return dataSet[31];
}
/// <summary>
/// Reset the SFP IIC Bus
/// </summary>
void DTCLib::DTC_Registers::ResetSFPIICInterface()
{
	auto bs = std::bitset<32>();
	bs[31] = 1;
	WriteRegister_(bs.to_ulong(), DTC_Register_SFP_IICBusControl);
	while (ReadSFPIICInterfaceReset())
	{
		usleep(1000);
	}
}
/// <summary>
/// Write a value to the SFP IIC Bus
/// </summary>
/// <param name="device">Device address</param>
/// <param name="address">Register address</param>
/// <param name="data">Data to write</param>
void DTCLib::DTC_Registers::WriteSFPIICInterface(uint8_t device, uint8_t address, uint8_t data)
{
	uint32_t reg_data = (static_cast<uint8_t>(device) << 24) + (address << 16) + (data << 8);
	WriteRegister_(reg_data, DTC_Register_SFP_IICBusLow);
	WriteRegister_(0x1, DTC_Register_SFP_IICBusHigh);
	while (ReadRegister_(DTC_Register_SFP_IICBusHigh) == 0x1)
	{
		usleep(1000);
	}
}
/// <summary>
/// Read a value from the SFP IIC Bus
/// </summary>
/// <param name="device">Device address</param>
/// <param name="address">Register address</param>
/// <returns>Value of register</returns>
uint8_t DTCLib::DTC_Registers::ReadSFPIICInterface(uint8_t device, uint8_t address)
{
	uint32_t reg_data = (static_cast<uint8_t>(device) << 24) + (address << 16);
	WriteRegister_(reg_data, DTC_Register_SFP_IICBusLow);
	WriteRegister_(0x2, DTC_Register_SFP_IICBusHigh);
	while (ReadRegister_(DTC_Register_SFP_IICBusHigh) == 0x2)
	{
		usleep(1000);
	}
	auto data = ReadRegister_(DTC_Register_SFP_IICBusLow);
	return static_cast<uint8_t>(data);
}

/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatSFPIICControl()
{
	auto form = CreateFormatter(DTC_Register_SFP_IICBusControl);
	form.description = "SFP Oscillator IIC Bus Control";
	form.vals.push_back(std::string("Reset:  [") + (ReadSFPIICInterfaceReset(form.value) ? "x" : " ") + "]");
	return form;
}
/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatSFPIICParameterLow()
{
	auto form = CreateFormatter(DTC_Register_SFP_IICBusLow);
	form.description = "SFP Oscillator IIC Bus Low";
	form.vals.push_back("");  // translation
	auto data = form.value;
	std::ostringstream s1, s2, s3, s4;
	s1 << "Device:     " << std::showbase << std::hex << ((data & 0xFF000000) >> 24);
	form.vals.push_back(s1.str());
	s2 << "ASFPess:    " << std::showbase << std::hex << ((data & 0xFF0000) >> 16);
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
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatSFPIICParameterHigh()
{
	auto form = CreateFormatter(DTC_Register_SFP_IICBusHigh);
	auto data = form.value;
	form.description = "SFP Oscillator IIC Bus High";
	form.vals.push_back("([ x = 1 (hi) ])");  // translation
	form.vals.push_back(std::string("Write:  [") + (data & 0x1 ? "x" : " ") + "]");
	form.vals.push_back(std::string("Read:   [") + (data & 0x2 ? "x" : " ") + "]");
	return form;
}

uint32_t DTCLib::DTC_Registers::ReadRetransmitRequestCount(DTC_Link_ID const& link, std::optional<uint32_t> val)
{
	switch (link)
	{
		case DTC_Link_0:
			return val.has_value() ? *val : ReadRegister_(DTC_Register_RetransmitRequestCount_Link0);
		case DTC_Link_1:
			return val.has_value() ? *val : ReadRegister_(DTC_Register_RetransmitRequestCount_Link1);
		case DTC_Link_2:
			return val.has_value() ? *val : ReadRegister_(DTC_Register_RetransmitRequestCount_Link2);
		case DTC_Link_3:
			return val.has_value() ? *val : ReadRegister_(DTC_Register_RetransmitRequestCount_Link3);
		case DTC_Link_4:
			return val.has_value() ? *val : ReadRegister_(DTC_Register_RetransmitRequestCount_Link4);
		case DTC_Link_5:
			return val.has_value() ? *val : ReadRegister_(DTC_Register_RetransmitRequestCount_Link5);
		default: {
			__SS__ << "Illegal link index provided: " << link << __E__;
			__SS_THROW__;
		}
	}
}
void DTCLib::DTC_Registers::ClearRetransmitRequestCount(DTC_Link_ID const& link)
{
	switch (link)
	{
		case DTC_Link_0:
			WriteRegister_(1, DTC_Register_RetransmitRequestCount_Link0);
			break;
		case DTC_Link_1:
			WriteRegister_(1, DTC_Register_RetransmitRequestCount_Link1);
			break;
		case DTC_Link_2:
			WriteRegister_(1, DTC_Register_RetransmitRequestCount_Link2);
			break;
		case DTC_Link_3:
			WriteRegister_(1, DTC_Register_RetransmitRequestCount_Link3);
			break;
		case DTC_Link_4:
			WriteRegister_(1, DTC_Register_RetransmitRequestCount_Link4);
			break;
		case DTC_Link_5:
			WriteRegister_(1, DTC_Register_RetransmitRequestCount_Link5);
			break;
		default: {
			__SS__ << "Illegal link index provided: " << link << __E__;
			__SS_THROW__;
		}
	}
}

/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatRetransmitRequestCountLink0()
{
	auto form = CreateFormatter(DTC_Register_RetransmitRequestCount_Link0);
	form.description = "ROC Retransmit Request Count Link 0";
	std::stringstream o;
	o << std::dec << ReadRetransmitRequestCount(DTC_Link_0, form.value);
	form.vals.push_back(o.str());
	return form;
}

/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatRetransmitRequestCountLink1()
{
	auto form = CreateFormatter(DTC_Register_RetransmitRequestCount_Link1);
	form.description = "ROC Retransmit Request Count Link 1";
	std::stringstream o;
	o << std::dec << ReadRetransmitRequestCount(DTC_Link_1, form.value);
	form.vals.push_back(o.str());
	return form;
}

/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatRetransmitRequestCountLink2()
{
	auto form = CreateFormatter(DTC_Register_RetransmitRequestCount_Link2);
	form.description = "ROC Retransmit Request Count Link 2";
	std::stringstream o;
	o << std::dec << ReadRetransmitRequestCount(DTC_Link_2, form.value);
	form.vals.push_back(o.str());
	return form;
}

/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatRetransmitRequestCountLink3()
{
	auto form = CreateFormatter(DTC_Register_RetransmitRequestCount_Link3);
	form.description = "ROC Retransmit Request Count Link 3";
	std::stringstream o;
	o << std::dec << ReadRetransmitRequestCount(DTC_Link_3, form.value);
	form.vals.push_back(o.str());
	return form;
}

/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatRetransmitRequestCountLink4()
{
	auto form = CreateFormatter(DTC_Register_RetransmitRequestCount_Link4);
	form.description = "ROC Retransmit Request Count Link 4";
	std::stringstream o;
	o << std::dec << ReadRetransmitRequestCount(DTC_Link_4, form.value);
	form.vals.push_back(o.str());
	return form;
}

/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatRetransmitRequestCountLink5()
{
	auto form = CreateFormatter(DTC_Register_RetransmitRequestCount_Link5);
	form.description = "ROC Retransmit Request Count Link 5";
	std::stringstream o;
	o << std::dec << ReadRetransmitRequestCount(DTC_Link_5, form.value);
	form.vals.push_back(o.str());
	return form;
}

// Missed CFO Packet Count Registers
/**
 * @brief Read the missed CFO Packet Count for the given link
 * @param link Link to read
 * @returns Number of missed CFO packets for the given link, or 0 if an invalid link is given
 */
uint32_t DTCLib::DTC_Registers::ReadMissedCFOPacketCount(DTC_Link_ID const& link, std::optional<uint32_t> val)
{
	switch (link)
	{
		case DTC_Link_0:
			return val.has_value() ? *val : ReadRegister_(DTC_Register_MissedCFOPacketCount_Link0);
		case DTC_Link_1:
			return val.has_value() ? *val : ReadRegister_(DTC_Register_MissedCFOPacketCount_Link1);
		case DTC_Link_2:
			return val.has_value() ? *val : ReadRegister_(DTC_Register_MissedCFOPacketCount_Link2);
		case DTC_Link_3:
			return val.has_value() ? *val : ReadRegister_(DTC_Register_MissedCFOPacketCount_Link3);
		case DTC_Link_4:
			return val.has_value() ? *val : ReadRegister_(DTC_Register_MissedCFOPacketCount_Link4);
		case DTC_Link_5:
			return val.has_value() ? *val : ReadRegister_(DTC_Register_MissedCFOPacketCount_Link5);
		default: {
			__SS__ << "Illegal link index provided: " << link << __E__;
			__SS_THROW__;
		}
	}
}

/// <summary>
/// Clears the Missed CFO Packet Count for the given link
/// </summary>
/// <param name="link">Link to clear</param>
void DTCLib::DTC_Registers::ClearMissedCFOPacketCount(DTC_Link_ID const& link)
{
	switch (link)
	{
		case DTC_Link_0:
			WriteRegister_(1, DTC_Register_MissedCFOPacketCount_Link0);
			break;
		case DTC_Link_1:
			WriteRegister_(1, DTC_Register_MissedCFOPacketCount_Link1);
			break;
		case DTC_Link_2:
			WriteRegister_(1, DTC_Register_MissedCFOPacketCount_Link2);
			break;
		case DTC_Link_3:
			WriteRegister_(1, DTC_Register_MissedCFOPacketCount_Link3);
			break;
		case DTC_Link_4:
			WriteRegister_(1, DTC_Register_MissedCFOPacketCount_Link4);
			break;
		case DTC_Link_5:
			WriteRegister_(1, DTC_Register_MissedCFOPacketCount_Link5);
			break;
		default: {
			__SS__ << "Illegal link index provided: " << link << __E__;
			__SS_THROW__;
		}
	}
}

/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatMissedCFOPacketCountLink0()
{
	auto form = CreateFormatter(DTC_Register_MissedCFOPacketCount_Link0);
	form.description = "Missed CFO Packet Count Link 0";
	std::stringstream o;
	o << std::dec << ReadMissedCFOPacketCount(DTC_Link_0, form.value);
	form.vals.push_back(o.str());
	return form;
}

/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatMissedCFOPacketCountLink1()
{
	auto form = CreateFormatter(DTC_Register_MissedCFOPacketCount_Link1);
	form.description = "Missed CFO Packet Count Link 1";
	std::stringstream o;
	o << std::dec << ReadMissedCFOPacketCount(DTC_Link_1, form.value);
	form.vals.push_back(o.str());
	return form;
}

/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatMissedCFOPacketCountLink2()
{
	auto form = CreateFormatter(DTC_Register_MissedCFOPacketCount_Link2);
	form.description = "Missed CFO Packet Count Link 2";
	std::stringstream o;
	o << std::dec << ReadMissedCFOPacketCount(DTC_Link_2, form.value);
	form.vals.push_back(o.str());
	return form;
}

/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatMissedCFOPacketCountLink3()
{
	auto form = CreateFormatter(DTC_Register_MissedCFOPacketCount_Link3);
	form.description = "Missed CFO Packet Count Link 3";
	std::stringstream o;
	o << std::dec << ReadMissedCFOPacketCount(DTC_Link_3, form.value);
	form.vals.push_back(o.str());
	return form;
}

/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatMissedCFOPacketCountLink4()
{
	auto form = CreateFormatter(DTC_Register_MissedCFOPacketCount_Link4);
	form.description = "Missed CFO Packet Count Link 4";
	std::stringstream o;
	o << std::dec << ReadMissedCFOPacketCount(DTC_Link_4, form.value);
	form.vals.push_back(o.str());
	return form;
}

/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatMissedCFOPacketCountLink5()
{
	auto form = CreateFormatter(DTC_Register_MissedCFOPacketCount_Link5);
	form.description = "Missed CFO Packet Count Link 5";
	std::stringstream o;
	o << std::dec << ReadMissedCFOPacketCount(DTC_Link_5, form.value);
	form.vals.push_back(o.str());
	return form;
}

// Local Fragment Drop Count Register
/// <summary>
/// Reads the current value of the Local Fragment Drop Counter
/// </summary>
/// <returns>The number of fragments dropped by the DTC</returns>
uint32_t DTCLib::DTC_Registers::ReadLocalFragmentDropCount(std::optional<uint32_t> val)
{
	return val.has_value() ? *val : ReadRegister_(DTC_Register_LocalFragmentDropCount);
}

/// <summary>
/// Clears the Local Fragment Drop Counter
/// </summary>
void DTCLib::DTC_Registers::ClearLocalFragmentDropCount() { WriteRegister_(1, DTC_Register_LocalFragmentDropCount); }

/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatLocalFragmentDropCount()
{
	auto form = CreateFormatter(DTC_Register_LocalFragmentDropCount);
	form.description = "Local Data Fragment Drop Count";
	std::stringstream o;
	o << std::dec << ReadLocalFragmentDropCount(form.value);
	form.vals.push_back(o.str());
	return form;
}

uint32_t DTCLib::DTC_Registers::ReadEVBSubEventReceiveTimer(std::optional<uint32_t> val)
{
	return val.has_value() ? *val : ReadRegister_(DTC_Register_EVBSubEventReceiveTimerPreset);
}

void DTCLib::DTC_Registers::SetEVBSubEventReceiveTimer(uint32_t timer)
{
	WriteRegister_(timer, DTC_Register_EVBSubEventReceiveTimerPreset);
}

DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatEVBSubEventReceiveTimer()
{
	auto form = CreateFormatter(DTC_Register_EVBSubEventReceiveTimerPreset);
	form.description = "EVB SubEvent Receive Timer (*4ns)";
	std::stringstream o;
	o << std::dec << ReadEVBSubEventReceiveTimer(form.value);
	form.vals.push_back(o.str());
	return form;
}

// EVB SERDES PRBS Register
/// <summary>
/// Determine if an error was detected in the EVB SERDES PRBS module
/// </summary>
/// <returns>True if error condition was detected, false otherwise</returns>
bool DTCLib::DTC_Registers::ReadEVBSERDESPRBSErrorFlag(std::optional<uint32_t> val)
{
	std::bitset<32> regVal(val.has_value() ? *val : ReadRegister_(DTC_Register_EVBSERDESPRBSControlStatus));
	return regVal[31];
}

/// <summary>
/// Read the EVB SERDES TX PRBS Select
/// </summary>
/// <returns>Value of the EVB SERDES TX PRBS Select</returns>
uint8_t DTCLib::DTC_Registers::ReadEVBSERDESTXPRBSSEL(std::optional<uint32_t> val)
{
	uint8_t regVal = static_cast<uint8_t>(((val.has_value() ? *val : ReadRegister_(DTC_Register_EVBSERDESPRBSControlStatus)) & 0x7000) >> 12);
	return regVal;
}

/// <summary>
/// Set the EVB SERDES TX PRBS Select
/// </summary>
/// <param name="byte">Value of the EVB SERDES TX PRBS Select</param>
void DTCLib::DTC_Registers::SetEVBSERDESTXPRBSSEL(uint8_t byte)
{
	uint8_t regVal = (ReadRegister_(DTC_Register_EVBSERDESPRBSControlStatus) & 0xFFFF8FFF);
	regVal += ((byte & 0x7) << 12);
	WriteRegister_(regVal, DTC_Register_EVBSERDESPRBSControlStatus);
}

/// <summary>
/// Read the EVB SERDES RX PRBS Select
/// </summary>
/// <returns>Value of the EVB SERDES RX PRBS Select</returns>
uint8_t DTCLib::DTC_Registers::ReadEVBSERDESRXPRBSSEL(std::optional<uint32_t> val)
{
	uint8_t regVal = static_cast<uint8_t>(((val.has_value() ? *val : ReadRegister_(DTC_Register_EVBSERDESPRBSControlStatus)) & 0x700) >> 8);
	return regVal;
}

/// <summary>
/// Set the EVB SERDES RX PRBS Select
/// </summary>
/// <param name="byte">Value of the EVB SERDES RX PRBS Select</param>
void DTCLib::DTC_Registers::SetEVBSERDESRXPRBSSEL(uint8_t byte)
{
	uint8_t regVal = (ReadRegister_(DTC_Register_EVBSERDESPRBSControlStatus) & 0xFFFFF8FF);
	regVal += ((byte & 0x7) << 8);
	WriteRegister_(regVal, DTC_Register_EVBSERDESPRBSControlStatus);
}

/// <summary>
/// Read the state of the EVB SERDES PRBS Force Error bit
/// </summary>
/// <returns>True if the EVB SERDES PRBS Force Error bit is high</returns>
bool DTCLib::DTC_Registers::ReadEVBSERDESPRBSForceError(std::optional<uint32_t> val)
{
	std::bitset<32> regVal(val.has_value() ? *val : ReadRegister_(DTC_Register_EVBSERDESPRBSControlStatus));
	return regVal[1];
}

/// <summary>
/// Set the EVB SERDES PRBS Force Error bit
/// </summary>
/// <param name="flag">New value for the EVB SERDES PRBS Reset bit</param>
void DTCLib::DTC_Registers::SetEVBSERDESPRBSForceError(bool flag)
{
	std::bitset<32> regVal(ReadRegister_(DTC_Register_EVBSERDESPRBSControlStatus));
	regVal[1] = flag;
	WriteRegister_(regVal.to_ulong(), DTC_Register_EVBSERDESPRBSControlStatus);
}

/// <summary>
/// Toggle the EVB SERDES PRBS Force Error bit (make it true if false, false if true)
/// </summary>
void DTCLib::DTC_Registers::ToggleEVBSERDESPRBSForceError()
{
	std::bitset<32> regVal(ReadRegister_(DTC_Register_EVBSERDESPRBSControlStatus));
	regVal.flip(0);
	WriteRegister_(regVal.to_ulong(), DTC_Register_EVBSERDESPRBSControlStatus);
}

/// <summary>
/// Read the state of the EVB SERDES PRBS Reset bit
/// </summary>
/// <returns>True if the EVB SERDES PRBS Reset bit is high</returns>
bool DTCLib::DTC_Registers::ReadEVBSERDESPRBSReset(std::optional<uint32_t> val)
{
	std::bitset<32> regVal(val.has_value() ? *val : ReadRegister_(DTC_Register_EVBSERDESPRBSControlStatus));
	return regVal[0];
}

/// <summary>
/// Set the EVB SERDES PRBS Reset bit
/// </summary>
/// <param name="flag">New value for the EVB SERDES PRBS Reset bit</param>
void DTCLib::DTC_Registers::SetEVBSERDESPRBSReset(bool flag)
{
	std::bitset<32> regVal(ReadRegister_(DTC_Register_EVBSERDESPRBSControlStatus));
	regVal[0] = flag;
	WriteRegister_(regVal.to_ulong(), DTC_Register_EVBSERDESPRBSControlStatus);
}

/// <summary>
/// Toggle the EVB SERDES PRBS Reset bit (make it true if false, false if true)
/// </summary>
void DTCLib::DTC_Registers::ToggleEVBSERDESPRBSReset()
{
	std::bitset<32> regVal(ReadRegister_(DTC_Register_EVBSERDESPRBSControlStatus));
	regVal.flip(0);
	WriteRegister_(regVal.to_ulong(), DTC_Register_EVBSERDESPRBSControlStatus);
}

/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatEVBSERDESPRBSControl()
{
	auto form = CreateFormatter(DTC_Register_EVBSERDESPRBSControlStatus);
	form.description = "EVB SERDES PRBS Control / Status";
	form.vals.push_back("([ x = 1 (hi) ])");  // translation
	form.vals.push_back(std::string("PRBS Error: [") + (ReadEVBSERDESPRBSErrorFlag(form.value) ? "x" : " ") + "]");
	std::stringstream o;
	o << "EVB SERDES TX PRBS Select: 0x" << std::hex << static_cast<int>(ReadEVBSERDESTXPRBSSEL(form.value));
	form.vals.push_back(o.str());
	o.str("");
	o.clear();
	o << "EVB SERDES RX PRBS Select: 0x" << std::hex << static_cast<int>(ReadEVBSERDESRXPRBSSEL(form.value));
	form.vals.push_back(std::string("PRBS Force Error: [") + (ReadEVBSERDESPRBSForceError(form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("PRBS Reset: [") + (ReadEVBSERDESPRBSReset(form.value) ? "x" : " ") + "]");

	return form;
}

// Event Builder Error Register
/// <summary>
/// Read the Event Builder SubEvent Receiver Flags Buffer Error bit
/// </summary>
/// <returns>Value of the bit</returns>
bool DTCLib::DTC_Registers::ReadEventBuilder_SubEventReceiverFlagsBufferError(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(DTC_Register_EventBuilderErrorFlags);
	return data[24];
}
/// <summary>
/// Read the Event Builder Ethernet Input FIFO Full bit
/// </summary>
/// <returns>Value of the bit</returns>
bool DTCLib::DTC_Registers::ReadEventBuilder_EthernetInputFIFOFull(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(DTC_Register_EventBuilderErrorFlags);
	return data[16];
}
/// <summary>
/// Read the Event Builder Link Error bit
/// </summary>
/// <returns>Value of the bit</returns>
bool DTCLib::DTC_Registers::ReadEventBuilder_LinkError(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(DTC_Register_EventBuilderErrorFlags);
	return data[9];
}
/// <summary>
/// Read the Event Builder TX Packet Error bit
/// </summary>
/// <returns>Value of the bit</returns>
bool DTCLib::DTC_Registers::ReadEventBuilder_TXPacketError(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(DTC_Register_EventBuilderErrorFlags);
	return data[8];
}
/// <summary>
/// Read the Event Builder Local Data Pointer FIFO Queue Error bit
/// </summary>
/// <returns>Value of the bit</returns>
bool DTCLib::DTC_Registers::ReadEventBuilder_LocalDataPointerFIFOQueueError(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(DTC_Register_EventBuilderErrorFlags);
	return data[1];
}
/// <summary>
/// Read the Event Builder Transmit DMA Byte Count FIFO Full bit
/// </summary>
/// <returns>Value of the bit</returns>
bool DTCLib::DTC_Registers::ReadEventBuilder_TransmitDMAByteCountFIFOFull(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(DTC_Register_EventBuilderErrorFlags);
	return data[0];
}
/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatEventBuilderErrorRegister()
{
	auto form = CreateFormatter(DTC_Register_EventBuilderErrorFlags);
	form.description = "Event Builder Error Flags";
	form.vals.push_back("([ x = 1 (hi) ])");  // translation
	form.vals.push_back(std::string("Sub-Event Received Flags Buffer Error: [") +
						(ReadEventBuilder_SubEventReceiverFlagsBufferError(form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("Input FIFO Full:                       [") +
						(ReadEventBuilder_EthernetInputFIFOFull(form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("Link Error:                            [") +
						(ReadEventBuilder_LinkError(form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("TX Packet Error:                       [") +
						(ReadEventBuilder_TXPacketError(form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("Local Data Pointer FIFO Queue Error:   [") +
						(ReadEventBuilder_LocalDataPointerFIFOQueueError(form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("Transmit DMA Byte Count FIFO Full:     [") +
						(ReadEventBuilder_TransmitDMAByteCountFIFOFull(form.value) ? "x" : " ") + "]");
	return form;
}

// SERDES VFIFO Error Register
/// <summary>
/// Read the SERDES VFIFO Egress FIFO Full bit
/// </summary>
/// <returns>Value of the bit</returns>
bool DTCLib::DTC_Registers::ReadSERDESVFIFO_EgressFIFOFull(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(DTC_Register_InputBufferErrorFlags);
	return data[10];
}
/// <summary>
/// Read the SERDES VFIFO Ingress FIFO Full bit
/// </summary>
/// <returns>Value of the bit</returns>
bool DTCLib::DTC_Registers::ReadSERDESVFIFO_IngressFIFOFull(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(DTC_Register_InputBufferErrorFlags);
	return data[9];
}
/// <summary>
/// Read the SERDES VFIFO Event Byte Count Total Error bit
/// </summary>
/// <returns>Value of the bit</returns>
bool DTCLib::DTC_Registers::ReadSERDESVFIFO_EventByteCountTotalError(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(DTC_Register_InputBufferErrorFlags);
	return data[8];
}
/// <summary>
/// Read the SERDES VFIFO Last Word Written Timeout Error bit
/// </summary>
/// <returns>Value of the bit</returns>
bool DTCLib::DTC_Registers::ReadSERDESVFIFO_LastWordWrittenTimeoutError(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(DTC_Register_InputBufferErrorFlags);
	return data[2];
}
/// <summary>
/// Read the SERDES VFIFO Fragment Count Error bit
/// </summary>
/// <returns>Value of the bit</returns>
bool DTCLib::DTC_Registers::ReadSERDESVFIFO_FragmentCountError(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(DTC_Register_InputBufferErrorFlags);
	return data[1];
}
/// <summary>
/// Read the SERDES VFIFO DDR Full Error bit
/// </summary>
/// <returns>Value of the bit</returns>
bool DTCLib::DTC_Registers::ReadSERDESVFIFO_DDRFullError(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(DTC_Register_InputBufferErrorFlags);
	return data[0];
}
/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatSERDESVFIFOError()
{
	auto form = CreateFormatter(DTC_Register_InputBufferErrorFlags);
	form.description = "SERDES VFIFO Error Flags";
	form.vals.push_back("([ x = 1 (hi) ])");  // translation
	form.vals.push_back(std::string("Egress FIFO Full:             [") + (ReadSERDESVFIFO_EgressFIFOFull(form.value) ? "x" : " ") +
						"]");
	form.vals.push_back(std::string("Ingress FIFO Full:            [") + (ReadSERDESVFIFO_IngressFIFOFull(form.value) ? "x" : " ") +
						"]");
	form.vals.push_back(std::string("Event Byte Count Total Error: [") +
						(ReadSERDESVFIFO_EventByteCountTotalError(form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("Last Word Written Timeout:    [") +
						(ReadSERDESVFIFO_LastWordWrittenTimeoutError(form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("Fragment count Error:         [") +
						(ReadSERDESVFIFO_FragmentCountError(form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("DDR Full Error:               [") + (ReadSERDESVFIFO_DDRFullError(form.value) ? "x" : " ") +
						"]");
	return form;
}

// PCI VFIFO Error Register
/// <summary>
/// Read the PCI VIFO DDR Full bit
/// </summary>
/// <returns>Value of the bit</returns>
bool DTCLib::DTC_Registers::ReadPCIVFIFO_DDRFull(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(DTC_Register_OutputBufferErrorFlags);
	return data[12];
}
/// <summary>
/// Read the PCI VIFO Memmory Mapped Write Complete FIFO Full bit
/// </summary>
/// <returns>Value of the bit</returns>
bool DTCLib::DTC_Registers::ReadPCIVFIFO_MemoryMappedWriteCompleteFIFOFull(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(DTC_Register_OutputBufferErrorFlags);
	return data[11];
}
/// <summary>
/// Read the PCI VIFO PCI Write Event FIFO Full bit
/// </summary>
/// <returns>Value of the bit</returns>
bool DTCLib::DTC_Registers::ReadPCIVFIFO_PCIWriteEventFIFOFull(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(DTC_Register_OutputBufferErrorFlags);
	return data[10];
}
/// <summary>
/// Read the PCI VIFO Local Data Pointer FIFO Full bit
/// </summary>
/// <returns>Value of the bit</returns>
bool DTCLib::DTC_Registers::ReadPCIVFIFO_LocalDataPointerFIFOFull(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(DTC_Register_OutputBufferErrorFlags);
	return data[9];
}
/// <summary>
/// Read the PCI VIFO Egress FIFO Full bit
/// </summary>
/// <returns>Value of the bit</returns>
bool DTCLib::DTC_Registers::ReadPCIVFIFO_EgressFIFOFull(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(DTC_Register_OutputBufferErrorFlags);
	return data[8];
}
/// <summary>
/// Read the PCI VIFO RX Buffer Select FIFO Full bit
/// </summary>
/// <returns>Value of the bit</returns>
bool DTCLib::DTC_Registers::ReadPCIVFIFO_RXBufferSelectFIFOFull(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(DTC_Register_OutputBufferErrorFlags);
	return data[2];
}
/// <summary>
/// Read the PCI VIFO Ingress FIFO Full bit
/// </summary>
/// <returns>Value of the bit</returns>
bool DTCLib::DTC_Registers::ReadPCIVFIFO_IngressFIFOFull(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(DTC_Register_OutputBufferErrorFlags);
	return data[1];
}
/// <summary>
/// Read the PCI VIFO Event Byte Count Total Error bit
/// </summary>
/// <returns>Value of the bit</returns>
bool DTCLib::DTC_Registers::ReadPCIVFIFO_EventByteCountTotalError(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(DTC_Register_OutputBufferErrorFlags);
	return data[0];
}
/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatPCIVFIFOError()
{
	auto form = CreateFormatter(DTC_Register_OutputBufferErrorFlags);
	form.description = "PCI VFIFO Error Flags";
	form.vals.push_back("([ x = 1 (hi) ])");  // translation
	form.vals.push_back(std::string("DDR Full Error:               [") + (ReadPCIVFIFO_DDRFull(form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("Memmap Write Cmplt FIFO Full: [") +
						(ReadPCIVFIFO_MemoryMappedWriteCompleteFIFOFull(form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("PCI Write Event FIFO Full:    [") +
						(ReadPCIVFIFO_PCIWriteEventFIFOFull(form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("Local Data Pointer FIFO Full: [") +
						(ReadPCIVFIFO_LocalDataPointerFIFOFull(form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("Egress FIFO Full:             [") + (ReadPCIVFIFO_EgressFIFOFull(form.value) ? "x" : " ") +
						"]");
	form.vals.push_back(std::string("RX Buffer Select FIFO Full:   [") +
						(ReadPCIVFIFO_RXBufferSelectFIFOFull(form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("Ingress FIFO Ful:             [") + (ReadPCIVFIFO_IngressFIFOFull(form.value) ? "x" : " ") +
						"]");
	form.vals.push_back(std::string("Event Byte Count Total Error: [") +
						(ReadPCIVFIFO_EventByteCountTotalError(form.value) ? "x" : " ") + "]");
	return form;
}

// ROC Link Error Registers
/// <summary>
/// Read the Receive Data Request Sync Error bit for the given link
/// </summary>
/// <param name="link">Link to read</param>
/// <returns>Value of the bit</returns>
bool DTCLib::DTC_Registers::ReadROCLink_ROCDataRequestSyncError(DTC_Link_ID const& link, std::optional<uint32_t> val)
{
	std::bitset<32> data;
	switch (link)
	{
		case DTC_Link_0:
			data = val.has_value() ? *val : ReadRegister_(DTC_Register_Link0ErrorFlags);
			break;
		case DTC_Link_1:
			data = val.has_value() ? *val : ReadRegister_(DTC_Register_Link1ErrorFlags);
			break;
		case DTC_Link_2:
			data = val.has_value() ? *val : ReadRegister_(DTC_Register_Link2ErrorFlags);
			break;
		case DTC_Link_3:
			data = val.has_value() ? *val : ReadRegister_(DTC_Register_Link3ErrorFlags);
			break;
		case DTC_Link_4:
			data = val.has_value() ? *val : ReadRegister_(DTC_Register_Link4ErrorFlags);
			break;
		case DTC_Link_5:
			data = val.has_value() ? *val : ReadRegister_(DTC_Register_Link5ErrorFlags);
			break;
		default: {
			__SS__ << "Illegal link index provided: " << link << __E__;
			__SS_THROW__;
		}
	}
	return data[5];
}
/// <summary>
/// Read the Receive RX Packet Count Error bit for the given link
/// </summary>
/// <param name="link">Link to read</param>
/// <returns>Value of the bit</returns>
bool DTCLib::DTC_Registers::ReadROCLink_RXPacketCountError(DTC_Link_ID const& link, std::optional<uint32_t> val)
{
	std::bitset<32> data;
	switch (link)
	{
		case DTC_Link_0:
			data = val.has_value() ? *val : ReadRegister_(DTC_Register_Link0ErrorFlags);
			break;
		case DTC_Link_1:
			data = val.has_value() ? *val : ReadRegister_(DTC_Register_Link1ErrorFlags);
			break;
		case DTC_Link_2:
			data = val.has_value() ? *val : ReadRegister_(DTC_Register_Link2ErrorFlags);
			break;
		case DTC_Link_3:
			data = val.has_value() ? *val : ReadRegister_(DTC_Register_Link3ErrorFlags);
			break;
		case DTC_Link_4:
			data = val.has_value() ? *val : ReadRegister_(DTC_Register_Link4ErrorFlags);
			break;
		case DTC_Link_5:
			data = val.has_value() ? *val : ReadRegister_(DTC_Register_Link5ErrorFlags);
			break;
		default: {
			__SS__ << "Illegal link index provided: " << link << __E__;
			__SS_THROW__;
		}
	}
	return data[4];
}
/// <summary>
/// Read the Receive RX Packet Error bit for the given link
/// </summary>
/// <param name="link">Link to read</param>
/// <returns>Value of the bit</returns>
bool DTCLib::DTC_Registers::ReadROCLink_RXPacketError(DTC_Link_ID const& link, std::optional<uint32_t> val)
{
	std::bitset<32> data;
	switch (link)
	{
		case DTC_Link_0:
			data = val.has_value() ? *val : ReadRegister_(DTC_Register_Link0ErrorFlags);
			break;
		case DTC_Link_1:
			data = val.has_value() ? *val : ReadRegister_(DTC_Register_Link1ErrorFlags);
			break;
		case DTC_Link_2:
			data = val.has_value() ? *val : ReadRegister_(DTC_Register_Link2ErrorFlags);
			break;
		case DTC_Link_3:
			data = val.has_value() ? *val : ReadRegister_(DTC_Register_Link3ErrorFlags);
			break;
		case DTC_Link_4:
			data = val.has_value() ? *val : ReadRegister_(DTC_Register_Link4ErrorFlags);
			break;
		case DTC_Link_5:
			data = val.has_value() ? *val : ReadRegister_(DTC_Register_Link5ErrorFlags);
			break;
		default: {
			__SS__ << "Illegal link index provided: " << link << __E__;
			__SS_THROW__;
		}
	}
	return data[3];
}
/// <summary>
/// Read the Receive RX Packet CRC Error bit for the given link
/// </summary>
/// <param name="link">Link to read</param>
/// <returns>Value of the bit</returns>
bool DTCLib::DTC_Registers::ReadROCLink_RXPacketCRCError(DTC_Link_ID const& link, std::optional<uint32_t> val)
{
	std::bitset<32> data;
	switch (link)
	{
		case DTC_Link_0:
			data = val.has_value() ? *val : ReadRegister_(DTC_Register_Link0ErrorFlags);
			break;
		case DTC_Link_1:
			data = val.has_value() ? *val : ReadRegister_(DTC_Register_Link1ErrorFlags);
			break;
		case DTC_Link_2:
			data = val.has_value() ? *val : ReadRegister_(DTC_Register_Link2ErrorFlags);
			break;
		case DTC_Link_3:
			data = val.has_value() ? *val : ReadRegister_(DTC_Register_Link3ErrorFlags);
			break;
		case DTC_Link_4:
			data = val.has_value() ? *val : ReadRegister_(DTC_Register_Link4ErrorFlags);
			break;
		case DTC_Link_5:
			data = val.has_value() ? *val : ReadRegister_(DTC_Register_Link5ErrorFlags);
			break;
		default: {
			__SS__ << "Illegal link index provided: " << link << __E__;
			__SS_THROW__;
		}
	}
	return data[2];
}
/// <summary>
/// Read the Receive Data Pending Timeout Error bit for the given link
/// </summary>
/// <param name="link">Link to read</param>
/// <returns>Value of the bit</returns>
bool DTCLib::DTC_Registers::ReadROCLink_DataPendingTimeoutError(DTC_Link_ID const& link, std::optional<uint32_t> val)
{
	std::bitset<32> data;
	switch (link)
	{
		case DTC_Link_0:
			data = val.has_value() ? *val : ReadRegister_(DTC_Register_Link0ErrorFlags);
			break;
		case DTC_Link_1:
			data = val.has_value() ? *val : ReadRegister_(DTC_Register_Link1ErrorFlags);
			break;
		case DTC_Link_2:
			data = val.has_value() ? *val : ReadRegister_(DTC_Register_Link2ErrorFlags);
			break;
		case DTC_Link_3:
			data = val.has_value() ? *val : ReadRegister_(DTC_Register_Link3ErrorFlags);
			break;
		case DTC_Link_4:
			data = val.has_value() ? *val : ReadRegister_(DTC_Register_Link4ErrorFlags);
			break;
		case DTC_Link_5:
			data = val.has_value() ? *val : ReadRegister_(DTC_Register_Link5ErrorFlags);
			break;
		default: {
			__SS__ << "Illegal link index provided: " << link << __E__;
			__SS_THROW__;
		}
	}
	return data[1];
}
/// <summary>
/// Read the Receive Data Packet Count Error bit for the given link
/// </summary>
/// <param name="link">Link to read</param>
/// <returns>Value of the bit</returns>
bool DTCLib::DTC_Registers::ReadROCLink_ReceiveDataPacketCountError(DTC_Link_ID const& link, std::optional<uint32_t> val)
{
	std::bitset<32> data;
	switch (link)
	{
		case DTC_Link_0:
			data = val.has_value() ? *val : ReadRegister_(DTC_Register_Link0ErrorFlags);
			break;
		case DTC_Link_1:
			data = val.has_value() ? *val : ReadRegister_(DTC_Register_Link1ErrorFlags);
			break;
		case DTC_Link_2:
			data = val.has_value() ? *val : ReadRegister_(DTC_Register_Link2ErrorFlags);
			break;
		case DTC_Link_3:
			data = val.has_value() ? *val : ReadRegister_(DTC_Register_Link3ErrorFlags);
			break;
		case DTC_Link_4:
			data = val.has_value() ? *val : ReadRegister_(DTC_Register_Link4ErrorFlags);
			break;
		case DTC_Link_5:
			data = val.has_value() ? *val : ReadRegister_(DTC_Register_Link5ErrorFlags);
			break;
		default: {
			__SS__ << "Illegal link index provided: " << link << __E__;
			__SS_THROW__;
		}
	}
	return data[0];
}
/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatRocLink0Error()
{
	auto form = CreateFormatter(DTC_Register_Link0ErrorFlags);
	form.description = "ROC Link 0 Error";
	form.vals.push_back("([ x = 1 (hi) ])");  // translation
	form.vals.push_back(std::string("ROC Data Request Sync Error:     [") +
						(ReadROCLink_ROCDataRequestSyncError(DTC_Link_0, form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("RX Packet Count Error:           [") +
						(ReadROCLink_RXPacketCountError(DTC_Link_0, form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("RX Packet Error:                 [") +
						(ReadROCLink_RXPacketError(DTC_Link_0, form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("RX Packet CRC Error:             [") +
						(ReadROCLink_RXPacketCRCError(DTC_Link_0, form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("Data Pending Timeout Error:      [") +
						(ReadROCLink_DataPendingTimeoutError(DTC_Link_0, form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("Receive Data Packet Count Error: [") +
						(ReadROCLink_ReceiveDataPacketCountError(DTC_Link_0, form.value) ? "x" : " ") + "]");
	return form;
}
/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatRocLink1Error()
{
	auto form = CreateFormatter(DTC_Register_Link1ErrorFlags);
	form.description = "ROC Link 1 Error";
	form.vals.push_back("([ x = 1 (hi) ])");  // translation
	form.vals.push_back(std::string("ROC Data Request Sync Error:     [") +
						(ReadROCLink_ROCDataRequestSyncError(DTC_Link_1, form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("RX Packet Count Error:           [") +
						(ReadROCLink_RXPacketCountError(DTC_Link_1, form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("RX Packet Error:                 [") +
						(ReadROCLink_RXPacketError(DTC_Link_1, form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("RX Packet CRC Error:             [") +
						(ReadROCLink_RXPacketCRCError(DTC_Link_1, form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("Data Pending Timeout Error:      [") +
						(ReadROCLink_DataPendingTimeoutError(DTC_Link_1, form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("Receive Data Packet Count Error: [") +
						(ReadROCLink_ReceiveDataPacketCountError(DTC_Link_1, form.value) ? "x" : " ") + "]");
	return form;
}
/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatRocLink2Error()
{
	auto form = CreateFormatter(DTC_Register_Link2ErrorFlags);
	form.description = "ROC Link 2 Error";
	form.vals.push_back("([ x = 1 (hi) ])");  // translation
	form.vals.push_back(std::string("ROC Data Request Sync Error:     [") +
						(ReadROCLink_ROCDataRequestSyncError(DTC_Link_2, form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("RX Packet Count Error:           [") +
						(ReadROCLink_RXPacketCountError(DTC_Link_2, form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("RX Packet Error:                 [") +
						(ReadROCLink_RXPacketError(DTC_Link_2, form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("RX Packet CRC Error:             [") +
						(ReadROCLink_RXPacketCRCError(DTC_Link_2, form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("Data Pending Timeout Error:      [") +
						(ReadROCLink_DataPendingTimeoutError(DTC_Link_2, form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("Receive Data Packet Count Error: [") +
						(ReadROCLink_ReceiveDataPacketCountError(DTC_Link_2, form.value) ? "x" : " ") + "]");
	return form;
}
/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatRocLink3Error()
{
	auto form = CreateFormatter(DTC_Register_Link3ErrorFlags);
	form.description = "ROC Link 3 Error";
	form.vals.push_back("([ x = 1 (hi) ])");  // translation
	form.vals.push_back(std::string("ROC Data Request Sync Error:     [") +
						(ReadROCLink_ROCDataRequestSyncError(DTC_Link_3, form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("RX Packet Count Error:           [") +
						(ReadROCLink_RXPacketCountError(DTC_Link_3, form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("RX Packet Error:                 [") +
						(ReadROCLink_RXPacketError(DTC_Link_3, form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("RX Packet CRC Error:             [") +
						(ReadROCLink_RXPacketCRCError(DTC_Link_3, form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("Data Pending Timeout Error:      [") +
						(ReadROCLink_DataPendingTimeoutError(DTC_Link_3, form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("Receive Data Packet Count Error: [") +
						(ReadROCLink_ReceiveDataPacketCountError(DTC_Link_3, form.value) ? "x" : " ") + "]");
	return form;
}
/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatRocLink4Error()
{
	auto form = CreateFormatter(DTC_Register_Link4ErrorFlags);
	form.description = "ROC Link 4 Error";
	form.vals.push_back("([ x = 1 (hi) ])");  // translation
	form.vals.push_back(std::string("ROC Data Request Sync Error:     [") +
						(ReadROCLink_ROCDataRequestSyncError(DTC_Link_4, form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("RX Packet Count Error:           [") +
						(ReadROCLink_RXPacketCountError(DTC_Link_4, form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("RX Packet Error:                 [") +
						(ReadROCLink_RXPacketError(DTC_Link_4, form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("RX Packet CRC Error:             [") +
						(ReadROCLink_RXPacketCRCError(DTC_Link_4, form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("Data Pending Timeout Error:      [") +
						(ReadROCLink_DataPendingTimeoutError(DTC_Link_4, form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("Receive Data Packet Count Error: [") +
						(ReadROCLink_ReceiveDataPacketCountError(DTC_Link_4, form.value) ? "x" : " ") + "]");
	return form;
}
/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatRocLink5Error()
{
	auto form = CreateFormatter(DTC_Register_Link5ErrorFlags);
	form.description = "ROC Link 5 Error";
	form.vals.push_back("([ x = 1 (hi) ])");  // translation
	form.vals.push_back(std::string("ROC Data Request Sync Error:     [") +
						(ReadROCLink_ROCDataRequestSyncError(DTC_Link_5, form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("RX Packet Count Error:           [") +
						(ReadROCLink_RXPacketCountError(DTC_Link_5, form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("RX Packet Error:                 [") +
						(ReadROCLink_RXPacketError(DTC_Link_5, form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("RX Packet CRC Error:             [") +
						(ReadROCLink_RXPacketCRCError(DTC_Link_5, form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("Data Pending Timeout Error:      [") +
						(ReadROCLink_DataPendingTimeoutError(DTC_Link_5, form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("Receive Data Packet Count Error: [") +
						(ReadROCLink_ReceiveDataPacketCountError(DTC_Link_5, form.value) ? "x" : " ") + "]");
	return form;
}

// CFO Link Error Register
/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatCFOLinkError()
{
	auto form = CreateFormatter(DTC_Register_CFOLinkErrorFlags);
	form.description = "CFO Link Settings & Error Flags";
	form.vals.push_back("([ x = 1 (hi) ])");  // translation

	// bit 9 - Event Start marker tx error at CFO Interface
	// bit 10 - Clock marker tx error at CFO Interface
	// bit 11 - RTF 40MHz clock phase has shifted
	// bit 12 - Illegal marker timing in RTF 40MHz clock count
	// bit 13 - Moving data from CFO rx to tx clock domain has marker corruption at "external" CFO Interface
	form.vals.push_back(std::string("CFO Event Start Marker tx Error:     [") +
						(((form.value >> 9) & 1) ? "x" : " ") + "]");
	form.vals.push_back(std::string("CFO Clock Marker tx Error:           [") +
						(((form.value >> 10) & 1) ? "x" : " ") + "]");
	form.vals.push_back(std::string("CFO RTF 40MHz Phase Shift Error:     [") +
						(((form.value >> 11) & 1) ? "x" : " ") + "]");
	form.vals.push_back(std::string("CFO Illegal Marker Over Link Timing: [") +
						(((form.value >> 12) & 1) ? "x" : " ") + "]");
	form.vals.push_back(std::string("CFO Rx-to-Tx Data Corruption Error:  [") +
						(((form.value >> 13) & 1) ? "x" : " ") + "]");
	int measuredPos = (form.value >> 16) & 7;
	int impliedPos = 2 - measuredPos;  // legal values are -2 -1 0 1 2 (if measured value is 4 3 2 1 0, respsectively)
	form.vals.push_back(std::string("CFO Measured Marker position {0,4}:  [") +
						std::to_string(measuredPos) + "] ==> " + std::to_string(impliedPos));
	form.vals.push_back(std::string("CFO Permanent Offset setting {-2,2}: [") +
						std::to_string(ReadCFOSamplePermanentOffset(form.value)) + "]");

	form.vals.push_back("");  // spacer

	// also show link enables for CFO and EVB
	uint32_t val = ReadRegister_(CFOandDTC_Register_LinkEnable);
	form.vals.push_back(std::string("CFO Link Rx Enabled:                 [") +
						(ReadLinkEnabled(DTC_Link_CFO, val).ReceiveEnable ? "x" : " ") + "]");
	form.vals.push_back(std::string("CFO Link Tx Enabled:                 [") +
						(ReadLinkEnabled(DTC_Link_CFO, val).TransmitEnable ? "x" : " ") + "]");
	form.vals.push_back(std::string("EVB Link Rx Enabled:                 [") +
						(ReadLinkEnabled(DTC_Link_EVB, val).ReceiveEnable ? "x" : " ") + "]");
	form.vals.push_back(std::string("EVB Link Tx Enabled:                 [") +
						(ReadLinkEnabled(DTC_Link_EVB, val).TransmitEnable ? "x" : " ") + "]");

	return form;
}

// Link Mux Error Register
/// <summary>
/// Read the DCS Mux Decode Error bit
/// </summary>
/// <returns>Value of the bit</returns>
bool DTCLib::DTC_Registers::ReadDCSMuxDecodeError(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(DTC_Register_LinkMuxErrorFlags);
	return data[1];
}
/// <summary>
/// Read the Data Mux Decode Error bit
/// </summary>
/// <returns>Value of the bit</returns>
bool DTCLib::DTC_Registers::ReadDataMuxDecodeError(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(DTC_Register_LinkMuxErrorFlags);
	return data[0];
}
/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatLinkMuxError()
{
	auto form = CreateFormatter(DTC_Register_LinkMuxErrorFlags);
	form.description = "Link Mux Error Flags";
	form.vals.push_back("([ x = 1 (hi) ])");  // translation
	form.vals.push_back(std::string("DCS Mux Decode Error:  [") + (ReadDCSMuxDecodeError(form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("Data Mux Decode Error: [") + (ReadDataMuxDecodeError(form.value) ? "x" : " ") + "]");
	return form;
}

// SFP Control/Status Register
/// <summary>
/// Read the SFP Present bit
/// </summary>
/// <returns>Value of the bit</returns>
bool DTCLib::DTC_Registers::ReadSFPPresent(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(DTC_Register_SFPControlStatus);
	return data[31];
}

/// <summary>
/// Read the SFP LOS bit
/// </summary>
/// <returns>Value of the bit</returns>
bool DTCLib::DTC_Registers::ReadSFPLOS(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(DTC_Register_SFPControlStatus);
	return data[17];
}

/// <summary>
/// Read the SFP TX Fault bit
/// </summary>
/// <returns>Value of the bit</returns>
bool DTCLib::DTC_Registers::ReadSFPTXFault(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(DTC_Register_SFPControlStatus);
	return data[16];
}

/// <summary>
/// Set the SFP Rate Select bit high
/// </summary>
void DTCLib::DTC_Registers::EnableSFPRateSelect()
{
	std::bitset<32> data = ReadRegister_(DTC_Register_SFPControlStatus);
	data[1] = 1;
	WriteRegister_(data.to_ulong(), DTC_Register_SFPControlStatus);
}

/// <summary>
/// Set the SFP Rate Select bit low
/// </summary>
void DTCLib::DTC_Registers::DisableSFPRateSelect()
{
	std::bitset<32> data = ReadRegister_(DTC_Register_SFPControlStatus);
	data[1] = 0;
	WriteRegister_(data.to_ulong(), DTC_Register_SFPControlStatus);
}

/// <summary>
/// Read the value of the SFP Rate Select bit
/// </summary>
/// <returns>The value of the SFP Rate Select bit</returns>
bool DTCLib::DTC_Registers::ReadSFPRateSelect(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(DTC_Register_SFPControlStatus);
	return data[1];
}

/// <summary>
/// Disable SFP TX
/// </summary>
void DTCLib::DTC_Registers::DisableSFPTX()
{
	std::bitset<32> data = ReadRegister_(DTC_Register_SFPControlStatus);
	data[0] = 1;
	WriteRegister_(data.to_ulong(), DTC_Register_SFPControlStatus);
}

/// <summary>
/// Enable SFP TX
/// </summary>
void DTCLib::DTC_Registers::EnableSFPTX()
{
	std::bitset<32> data = ReadRegister_(DTC_Register_SFPControlStatus);
	data[0] = 0;
	WriteRegister_(data.to_ulong(), DTC_Register_SFPControlStatus);
}

/// <summary>
/// Read the SFP TX Disable bit
/// </summary>
/// <returns>Value of the SFP TX Disable bit</returns>
bool DTCLib::DTC_Registers::ReadSFPTXDisable(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(DTC_Register_SFPControlStatus);
	return data[0];
}

/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatSFPControlStatus()
{
	auto form = CreateFormatter(DTC_Register_SFPControlStatus);
	form.description = "SFP Control Status";
	form.vals.push_back("([ x = 1 (hi) ])");  // translation
	form.vals.push_back(std::string("SFP Present:     [") + (ReadSFPPresent(form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("SFP LOS:         [") + (ReadSFPLOS(form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("SFP TX Fault:    [") + (ReadSFPTXFault(form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("SFP Rate Select: [") + (ReadSFPRateSelect(form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("SFP TX Disable:  [") + (ReadSFPTXDisable(form.value) ? "x" : " ") + "]");
	return form;
}

uint32_t DTCLib::DTC_Registers::ReadRXCDRUnlockCount(DTC_Link_ID const& link, std::optional<uint32_t> val)
{
	switch (link)
	{
		case DTC_Link_0:
			return val.has_value() ? *val : ReadRegister_(DTC_Register_RXCDRUnlockCount_Link0);
		case DTC_Link_1:
			return val.has_value() ? *val : ReadRegister_(DTC_Register_RXCDRUnlockCount_Link1);
		case DTC_Link_2:
			return val.has_value() ? *val : ReadRegister_(DTC_Register_RXCDRUnlockCount_Link2);
		case DTC_Link_3:
			return val.has_value() ? *val : ReadRegister_(DTC_Register_RXCDRUnlockCount_Link3);
		case DTC_Link_4:
			return val.has_value() ? *val : ReadRegister_(DTC_Register_RXCDRUnlockCount_Link4);
		case DTC_Link_5:
			return val.has_value() ? *val : ReadRegister_(DTC_Register_RXCDRUnlockCount_Link5);
		case DTC_Link_CFO:
			return val.has_value() ? *val : ReadRegister_(DTC_Register_RXCDRUnlockCount_CFOLink);
		default: {
			__SS__ << "Illegal link index provided: " << link << __E__;
			__SS_THROW__;
		}
	}
}

void DTCLib::DTC_Registers::ClearRXCDRUnlockCount(DTC_Link_ID const& link)
{
	switch (link)
	{
		case DTC_Link_0:
			WriteRegister_(0, DTC_Register_RXCDRUnlockCount_Link0);
			break;
		case DTC_Link_1:
			WriteRegister_(0, DTC_Register_RXCDRUnlockCount_Link1);
			break;
		case DTC_Link_2:
			WriteRegister_(0, DTC_Register_RXCDRUnlockCount_Link2);
			break;
		case DTC_Link_3:
			WriteRegister_(0, DTC_Register_RXCDRUnlockCount_Link3);
			break;
		case DTC_Link_4:
			WriteRegister_(0, DTC_Register_RXCDRUnlockCount_Link4);
			break;
		case DTC_Link_5:
			WriteRegister_(0, DTC_Register_RXCDRUnlockCount_Link5);
			break;
		case DTC_Link_CFO:
			WriteRegister_(0, DTC_Register_RXCDRUnlockCount_CFOLink);
			break;
		default: {
			__SS__ << "Illegal link index provided: " << link << __E__;
			__SS_THROW__;
		}
	}
}

DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatRXCDRUnlockCountLink0()
{
	auto form = CreateFormatter(DTC_Register_RXCDRUnlockCount_Link0);
	form.description = "RX CDR Unlock Count Link 0";
	std::stringstream o;
	o << std::dec << ReadRXCDRUnlockCount(DTC_Link_0, form.value);
	form.vals.push_back(o.str());
	return form;
}

DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatRXCDRUnlockCountLink1()
{
	auto form = CreateFormatter(DTC_Register_RXCDRUnlockCount_Link1);
	form.description = "RX CDR Unlock Count Link 1";
	std::stringstream o;
	o << std::dec << ReadRXCDRUnlockCount(DTC_Link_1, form.value);
	form.vals.push_back(o.str());
	return form;
}

DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatRXCDRUnlockCountLink2()
{
	auto form = CreateFormatter(DTC_Register_RXCDRUnlockCount_Link2);
	form.description = "RX CDR Unlock Count Link 2";
	std::stringstream o;
	o << std::dec << ReadRXCDRUnlockCount(DTC_Link_2, form.value);
	form.vals.push_back(o.str());
	return form;
}

DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatRXCDRUnlockCountLink3()
{
	auto form = CreateFormatter(DTC_Register_RXCDRUnlockCount_Link3);
	form.description = "RX CDR Unlock Count Link 3";
	std::stringstream o;
	o << std::dec << ReadRXCDRUnlockCount(DTC_Link_3, form.value);
	form.vals.push_back(o.str());
	return form;
}

DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatRXCDRUnlockCountLink4()
{
	auto form = CreateFormatter(DTC_Register_RXCDRUnlockCount_Link4);
	form.description = "RX CDR Unlock Count Link 4";
	std::stringstream o;
	o << std::dec << ReadRXCDRUnlockCount(DTC_Link_4, form.value);
	form.vals.push_back(o.str());
	return form;
}

DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatRXCDRUnlockCountLink5()
{
	auto form = CreateFormatter(DTC_Register_RXCDRUnlockCount_Link5);
	form.description = "RX CDR Unlock Count Link 5";
	std::stringstream o;
	o << std::dec << ReadRXCDRUnlockCount(DTC_Link_5, form.value);
	form.vals.push_back(o.str());
	return form;
}

DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatRXCDRUnlockCountCFOLink()
{
	auto form = CreateFormatter(DTC_Register_RXCDRUnlockCount_CFOLink);
	form.description = "RX CDR Unlock Count CFO Link";
	std::stringstream o;
	o << std::dec << ReadRXCDRUnlockCount(DTC_Link_CFO, form.value);
	form.vals.push_back(o.str());
	return form;
}

uint32_t DTCLib::DTC_Registers::ReadJitterAttenuatorUnlockCount(std::optional<uint32_t> val)
{
	return val.has_value() ? *val : ReadRegister_(DTC_Register_JitterAttenuatorLossOfLockCount);
}

void DTCLib::DTC_Registers::ClearJitterAttenuatorUnlockCount()
{
	WriteRegister_(1, DTC_Register_JitterAttenuatorLossOfLockCount);
}

DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatJitterAttenuatorUnlockCount()
{
	auto form = CreateFormatter(DTC_Register_JitterAttenuatorLossOfLockCount);
	form.description = "RX Jitter Attenuator Unlock Count";
	std::stringstream o;
	o << std::dec << ReadJitterAttenuatorUnlockCount(form.value);
	form.vals.push_back(o.str());
	return form;
}

uint32_t DTCLib::DTC_Registers::ReadRXCFOLinkEventStartCharacterErrorCount(std::optional<uint32_t> val)
{
	return val.has_value() ? *val : ReadRegister_(DTC_Register_CFOLinkEventStartErrorCount);
}

void DTCLib::DTC_Registers::ClearRXCFOLinkEventStartCharacterErrorCount()
{
	WriteRegister_(0, DTC_Register_CFOLinkEventStartErrorCount);
}

DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatRXCFOLinkEventStartCharacterErrorCount()
{
	auto form = CreateFormatter(DTC_Register_CFOLinkEventStartErrorCount);
	form.description = "CFO Link Event Start Error Count";
	std::stringstream o;
	o << std::dec << ReadRXCFOLinkEventStartCharacterErrorCount(form.value);
	form.vals.push_back(o.str());
	return form;
}

uint32_t DTCLib::DTC_Registers::ReadRXCFOLink40MHzCharacterErrorCount(std::optional<uint32_t> val)
{
	return val.has_value() ? *val : ReadRegister_(DTC_Register_CFOLink40MHzErrorCount);
}

void DTCLib::DTC_Registers::ClearRXCFOLink40MHzCharacterErrorCount()
{
	WriteRegister_(0, DTC_Register_CFOLink40MHzErrorCount);
}

DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatRXCFOLink40MHzCharacterErrorCount()
{
	auto form = CreateFormatter(DTC_Register_CFOLink40MHzErrorCount);
	form.description = "CFO Link 40 MHz Error Count";
	std::stringstream o;
	o << std::dec << ReadRXCFOLink40MHzCharacterErrorCount(form.value);
	form.vals.push_back(o.str());
	return form;
}

uint32_t DTCLib::DTC_Registers::ReadInputBufferFragmentDumpCount(std::optional<uint32_t> val)
{
	return val.has_value() ? *val : ReadRegister_(DTC_Register_InputBufferDropCount);
}

void DTCLib::DTC_Registers::ClearInputBufferFragmentDumpCount()
{
	WriteRegister_(0, DTC_Register_InputBufferDropCount);
}

DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatInputBufferFragmentDumpCount()
{
	auto form = CreateFormatter(DTC_Register_InputBufferDropCount);
	form.description = "Input Buffer Fragment Drop Count";
	std::stringstream o;
	o << std::dec << ReadInputBufferFragmentDumpCount(form.value);
	form.vals.push_back(o.str());
	return form;
}

uint32_t DTCLib::DTC_Registers::ReadOutputBufferFragmentDumpCount(std::optional<uint32_t> val)
{
	return val.has_value() ? *val : ReadRegister_(DTC_Register_OutputBufferDropCount);
}

void DTCLib::DTC_Registers::ClearOutputBufferFragmentDumpCount()
{
	WriteRegister_(0, DTC_Register_OutputBufferDropCount);
}

DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatOutputBufferFragmentDumpCount()
{
	auto form = CreateFormatter(DTC_Register_OutputBufferDropCount);
	form.description = "Output Buffer Fragment Drop Count";
	std::stringstream o;
	o << std::dec << ReadOutputBufferFragmentDumpCount(form.value);
	form.vals.push_back(o.str());
	return form;
}

uint32_t DTCLib::DTC_Registers::ReadROCDCSResponseTimer(std::optional<uint32_t> val)
{
	__SS__ << "The SetROCDCSResponseTimer register was removed as of December 2023 and set to a 1ms constant value in the DTC. Do not use." << __E__;
	__SS_THROW__;
	return val.has_value() ? *val : ReadRegister_(DTC_Register_ROCDCSTimerPreset);
}

void DTCLib::DTC_Registers::SetROCDCSResponseTimer(uint32_t timer)
{
	__SS__ << "The SetROCDCSResponseTimer register was removed as of December 2023 and set to a 1ms constant value in the DTC. Do not use." << __E__;
	__SS_THROW__;
	WriteRegister_(timer, DTC_Register_ROCDCSTimerPreset);
}

DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatROCDCSResponseTimerPreset()
{
	auto form = CreateFormatter(DTC_Register_ROCDCSTimerPreset);
	form.description = "ROC DCS Response Timer Preset (*5ns)";
	std::stringstream o;
	o << std::dec << ReadROCDCSResponseTimer(form.value);
	form.vals.push_back(o.str());
	return form;
}

void DTCLib::DTC_Registers::SetSoftwareDataRequest(const DTC_EventWindowTag& ts)
{
	auto timestamp = ts.GetEventWindowTag();
	auto timestampLow = static_cast<uint32_t>(timestamp.to_ulong());
	timestamp >>= 32;
	auto timestampHigh = static_cast<uint16_t>(timestamp.to_ulong());

	WriteRegister_(timestampHigh, DTC_Register_DataRequest_High);
	WriteRegister_(timestampLow, DTC_Register_DataRequest_Low);  // this triggers the DR
}

DTCLib::DTC_EventWindowTag DTCLib::DTC_Registers::ReadSoftwareDataRequest(std::optional<uint32_t> val)
{
	auto timestampLow = val.has_value() ? *val : ReadRegister_(DTC_Register_DataRequest_Low);
	DTC_EventWindowTag output;
	output.SetEventWindowTag(timestampLow, static_cast<uint16_t>(ReadRegister_(DTC_Register_DataRequest_High)));
	return output;
}

DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatSoftwareDataRequestLow()
{
	auto form = CreateFormatter(DTC_Register_DataRequest_Low);
	form.description = "Software Data Request Low";
	std::stringstream o;
	o << "0x" << std::hex << ReadRegister_(DTC_Register_DataRequest_Low);
	form.vals.push_back(o.str());
	return form;
}

DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatSoftwareDataRequestHigh()
{
	auto form = CreateFormatter(DTC_Register_DataRequest_High);
	form.description = "Software Data Request High";
	std::stringstream o;
	o << "0x" << std::hex << ReadRegister_(DTC_Register_DataRequest_High);
	form.vals.push_back(o.str());
	return form;
}

// FPGA PROM Program Status Register
/// <summary>
/// Read the full bit on the FPGA PROM FIFO
/// </summary>
/// <returns>the full bit on the FPGA PROM FIFO</returns>
bool DTCLib::DTC_Registers::ReadFPGAPROMProgramFIFOFull(std::optional<uint32_t> val)
{
	std::bitset<32> dataSet = val.has_value() ? *val : ReadRegister_(DTC_Register_FPGAPROMProgramStatus);
	return dataSet[1];
}

/// <summary>
/// Read whether the FPGA PROM is ready for data
/// </summary>
/// <returns>whether the FPGA PROM is ready for data</returns>
bool DTCLib::DTC_Registers::ReadFPGAPROMReady(std::optional<uint32_t> val)
{
	std::bitset<32> dataSet = val.has_value() ? *val : ReadRegister_(DTC_Register_FPGAPROMProgramStatus);
	return dataSet[0];
}

/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatFPGAPROMProgramStatus()
{
	auto form = CreateFormatter(DTC_Register_FPGAPROMProgramStatus);
	form.description = "FPGA PROM Program Status";
	form.vals.push_back("([ x = 1 (hi) ])");  // translation
	form.vals.push_back(std::string("FPGA PROM Program FIFO Full: [") + (ReadFPGAPROMProgramFIFOFull(form.value) ? "x" : " ") +
						"]");
	form.vals.push_back(std::string("FPGA PROM Ready:             [") + (ReadFPGAPROMReady(form.value) ? "x" : " ") + "]");
	return form;
}

// FPGA Core Access Register
/// <summary>
/// Performs the chants necessary to reload the DTC firmware
/// </summary>
void DTCLib::DTC_Registers::ReloadFPGAFirmware()
{
	WriteRegister_(0xFFFFFFFF, DTC_Register_FPGACoreAccess);
	while (ReadFPGACoreAccessFIFOFull())
	{
		usleep(10);
	}
	WriteRegister_(0xAA995566, DTC_Register_FPGACoreAccess);
	while (ReadFPGACoreAccessFIFOFull())
	{
		usleep(10);
	}
	WriteRegister_(0x20000000, DTC_Register_FPGACoreAccess);
	while (ReadFPGACoreAccessFIFOFull())
	{
		usleep(10);
	}
	WriteRegister_(0x30020001, DTC_Register_FPGACoreAccess);
	while (ReadFPGACoreAccessFIFOFull())
	{
		usleep(10);
	}
	WriteRegister_(0x00000000, DTC_Register_FPGACoreAccess);
	while (ReadFPGACoreAccessFIFOFull())
	{
		usleep(10);
	}
	WriteRegister_(0x30008001, DTC_Register_FPGACoreAccess);
	while (ReadFPGACoreAccessFIFOFull())
	{
		usleep(10);
	}
	WriteRegister_(0x0000000F, DTC_Register_FPGACoreAccess);
	while (ReadFPGACoreAccessFIFOFull())
	{
		usleep(10);
	}
	WriteRegister_(0x20000000, DTC_Register_FPGACoreAccess);
}

/// <summary>
/// Read the FPGA Core Access FIFO Full bit
/// </summary>
/// <returns>Whether the FPGA Core Access FIFO is full</returns>
bool DTCLib::DTC_Registers::ReadFPGACoreAccessFIFOFull(std::optional<uint32_t> val)
{
	std::bitset<32> dataSet = val.has_value() ? *val : ReadRegister_(DTC_Register_FPGACoreAccess);
	return dataSet[1];
}

/// <summary>
/// Read the FPGA Core Access FIFO Empty bit
/// </summary>
/// <returns>Whether the FPGA Core Access FIFO is empty</returns>
bool DTCLib::DTC_Registers::ReadFPGACoreAccessFIFOEmpty(std::optional<uint32_t> val)
{
	std::bitset<32> dataSet = val.has_value() ? *val : ReadRegister_(DTC_Register_FPGACoreAccess);
	return dataSet[0];
}

/// <summary>
/// Formats the register's current value for register dumps
/// </summary>
/// <returns>RegisterFormatter object containing register information</returns>
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatFPGACoreAccess()
{
	auto form = CreateFormatter(DTC_Register_FPGACoreAccess);
	form.description = "FPGA Core Access";
	form.vals.push_back("([ x = 1 (hi) ])");  // translation
	form.vals.push_back(std::string("FPGA Core Access FIFO Full:  [") + (ReadFPGACoreAccessFIFOFull(form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("FPGA Core Access FIFO Empty: [") + (ReadFPGACoreAccessFIFOEmpty(form.value) ? "x" : " ") +
						"]");

	return form;
}

bool DTCLib::DTC_Registers::ReadRXOKErrorSlowOpticalLink3(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(DTC_Register_SlowOpticalLinksDiag);
	return data[9];
}

bool DTCLib::DTC_Registers::ReadRXOKErrorSlowOpticalLink2(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(DTC_Register_SlowOpticalLinksDiag);
	return data[8];
}

bool DTCLib::DTC_Registers::ReadRXOKErrorSlowOpticalLink1(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(DTC_Register_SlowOpticalLinksDiag);
	return data[7];
}

bool DTCLib::DTC_Registers::ReadRXOKErrorSlowOpticalLink0(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(DTC_Register_SlowOpticalLinksDiag);
	return data[6];
}

bool DTCLib::DTC_Registers::ReadLatchedSpareSMAInputOKError(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(DTC_Register_SlowOpticalLinksDiag);
	return data[5];
}

void DTCLib::DTC_Registers::ClearLatchedSpareSMAInputOKError()
{
	std::bitset<32> data = ReadRegister_(DTC_Register_SlowOpticalLinksDiag);
	data[5] = 1;
	WriteRegister_(data.to_ulong(), DTC_Register_SlowOpticalLinksDiag);
}

bool DTCLib::DTC_Registers::ReadLatchedEventMarkerSMAInputOKError(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(DTC_Register_SlowOpticalLinksDiag);
	return data[4];
}

void DTCLib::DTC_Registers::ClearLatchedEventMarkerSMASMAInputOKError()
{
	std::bitset<32> data = ReadRegister_(DTC_Register_SlowOpticalLinksDiag);
	data[4] = 1;
	WriteRegister_(data.to_ulong(), DTC_Register_SlowOpticalLinksDiag);
}

bool DTCLib::DTC_Registers::ReadLatchedRXOKErrorSlowOpticalLink3(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(DTC_Register_SlowOpticalLinksDiag);
	return data[3];
}

void DTCLib::DTC_Registers::ClearLatchedRXOKErrorSlowOpticalLink3()
{
	std::bitset<32> data = ReadRegister_(DTC_Register_SlowOpticalLinksDiag);
	data[3] = 1;
	WriteRegister_(data.to_ulong(), DTC_Register_SlowOpticalLinksDiag);
}

bool DTCLib::DTC_Registers::ReadLatchedRXOKErrorSlowOpticalLink2(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(DTC_Register_SlowOpticalLinksDiag);
	return data[2];
}

void DTCLib::DTC_Registers::ClearLatchedRXOKErrorSlowOpticalLink2()
{
	std::bitset<32> data = ReadRegister_(DTC_Register_SlowOpticalLinksDiag);
	data[2] = 1;
	WriteRegister_(data.to_ulong(), DTC_Register_SlowOpticalLinksDiag);
}

bool DTCLib::DTC_Registers::ReadLatchedRXOKErrorSlowOpticalLink1(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(DTC_Register_SlowOpticalLinksDiag);
	return data[1];
}

void DTCLib::DTC_Registers::ClearLatchedRXOKErrorSlowOpticalLink1()
{
	std::bitset<32> data = ReadRegister_(DTC_Register_SlowOpticalLinksDiag);
	data[1] = 1;
	WriteRegister_(data.to_ulong(), DTC_Register_SlowOpticalLinksDiag);
}

bool DTCLib::DTC_Registers::ReadLatchedRXOKErrorSlowOpticalLink0(std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(DTC_Register_SlowOpticalLinksDiag);
	return data[0];
}

void DTCLib::DTC_Registers::ClearLatchedRXOKErrorSlowOpticalLink0()
{
	std::bitset<32> data = ReadRegister_(DTC_Register_SlowOpticalLinksDiag);
	data[0] = 1;
	WriteRegister_(data.to_ulong(), DTC_Register_SlowOpticalLinksDiag);
}

DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatSlowOpticalLinkControlStatus()
{
	auto form = CreateFormatter(DTC_Register_SFPControlStatus);
	form.description = "Slow Optical Link Control Status";
	form.vals.push_back("([ x = 1 (hi) ])");  // translation
	form.vals.push_back(std::string("RX OK SOL Link 3:           [") + (ReadRXOKErrorSlowOpticalLink3(form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("RX OK SOL Link 2:           [") + (ReadRXOKErrorSlowOpticalLink2(form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("RX OK SOL Link 1:           [") + (ReadRXOKErrorSlowOpticalLink1(form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("RX OK SOL Link 0:           [") + (ReadRXOKErrorSlowOpticalLink0(form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("Latched Spare SMA Input:    [") + (ReadLatchedSpareSMAInputOKError(form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("Latched Event Marker Input: [") + (ReadLatchedEventMarkerSMAInputOKError(form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("Latched RX OK SOL Link 3:   [") + (ReadLatchedRXOKErrorSlowOpticalLink3(form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("Latched RX OK SOL Link 2:   [") + (ReadLatchedRXOKErrorSlowOpticalLink2(form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("Latched RX OK SOL Link 1:   [") + (ReadLatchedRXOKErrorSlowOpticalLink1(form.value) ? "x" : " ") + "]");
	form.vals.push_back(std::string("Latched RX OK SOL Link 0:   [") + (ReadLatchedRXOKErrorSlowOpticalLink0(form.value) ? "x" : " ") + "]");
	return form;
}

bool DTCLib::DTC_Registers::ReadSERDESInduceErrorEnable(DTC_Link_ID const& link, std::optional<uint32_t> val)
{
	std::bitset<32> data = val.has_value() ? *val : ReadRegister_(DTC_Register_DiagSERDESErrorEnable);
	return data[link];
}

void DTCLib::DTC_Registers::EnableSERDESInduceError(DTC_Link_ID const& link)
{
	SetBit_(DTC_Register_DiagSERDESErrorEnable, link, true);
}

void DTCLib::DTC_Registers::DisableSERDESInduceError(DTC_Link_ID const& link)
{
	std::bitset<32> data = ReadRegister_(DTC_Register_DiagSERDESErrorEnable);
	data[link] = 0;
	WriteRegister_(data.to_ulong(), DTC_Register_DiagSERDESErrorEnable);
}

DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatSERDESInduceErrorEnable()
{
	auto form = CreateFormatter(DTC_Register_SERDESTXRXInvertEnable);
	form.description = "SERDES Induce Error Enable";
	form.vals.push_back("([ x = 1 (hi) ])");  // translation
	for (auto r : DTC_ROC_Links)
		form.vals.push_back(std::string("Link ") + std::to_string(r) + ": [" +
							(ReadSERDESInduceErrorEnable(r, form.value) ? "x" : " ") + "]");

	return form;
}

uint32_t DTCLib::DTC_Registers::ReadSERDESInduceErrorSequenceNumber(DTC_Link_ID const& link, std::optional<uint32_t> val)
{
	switch (link)
	{
		case DTC_Link_0:
			return val.has_value() ? *val : ReadRegister_(DTC_Register_DiagSERDESPacket0);
		case DTC_Link_1:
			return val.has_value() ? *val : ReadRegister_(DTC_Register_DiagSERDESPacket1);
		case DTC_Link_2:
			return val.has_value() ? *val : ReadRegister_(DTC_Register_DiagSERDESPacket2);
		case DTC_Link_3:
			return val.has_value() ? *val : ReadRegister_(DTC_Register_DiagSERDESPacket3);
		case DTC_Link_4:
			return val.has_value() ? *val : ReadRegister_(DTC_Register_DiagSERDESPacket4);
		case DTC_Link_5:
			return val.has_value() ? *val : ReadRegister_(DTC_Register_DiagSERDESPacket5);
		default: {
			__SS__ << "Illegal link index provided: " << link << __E__;
			__SS_THROW__;
		}
	}
}

void DTCLib::DTC_Registers::SetSERDESInduceErrorSequenceNumber(DTC_Link_ID const& link, uint32_t sequence)
{
	switch (link)
	{
		case DTC_Link_0:
			WriteRegister_(sequence, DTC_Register_DiagSERDESPacket0);
			break;
		case DTC_Link_1:
			WriteRegister_(sequence, DTC_Register_DiagSERDESPacket1);
			break;
		case DTC_Link_2:
			WriteRegister_(sequence, DTC_Register_DiagSERDESPacket2);
			break;
		case DTC_Link_3:
			WriteRegister_(sequence, DTC_Register_DiagSERDESPacket3);
			break;
		case DTC_Link_4:
			WriteRegister_(sequence, DTC_Register_DiagSERDESPacket4);
			break;
		case DTC_Link_5:
			WriteRegister_(sequence, DTC_Register_DiagSERDESPacket5);
			break;
		default: {
			__SS__ << "Illegal link index provided: " << link << __E__;
			__SS_THROW__;
		}
	}
}

DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatSERDESInduceErrorSequenceNumberLink0()
{
	auto form = CreateFormatter(DTC_Register_DiagSERDESPacket0);
	form.description = "Link 0 Induced Error Sequence Number";
	std::stringstream o;
	o << std::dec << ReadSERDESInduceErrorSequenceNumber(DTC_Link_0, form.value);
	form.vals.push_back(o.str());
	return form;
}

DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatSERDESInduceErrorSequenceNumberLink1()
{
	auto form = CreateFormatter(DTC_Register_DiagSERDESPacket1);
	form.description = "Link 1 Induced Error Sequence Number";
	std::stringstream o;
	o << std::dec << ReadSERDESInduceErrorSequenceNumber(DTC_Link_1, form.value);
	form.vals.push_back(o.str());
	return form;
}

DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatSERDESInduceErrorSequenceNumberLink2()
{
	auto form = CreateFormatter(DTC_Register_DiagSERDESPacket2);
	form.description = "Link 2 Induced Error Sequence Number";
	std::stringstream o;
	o << std::dec << ReadSERDESInduceErrorSequenceNumber(DTC_Link_2, form.value);
	form.vals.push_back(o.str());
	return form;
}

DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatSERDESInduceErrorSequenceNumberLink3()
{
	auto form = CreateFormatter(DTC_Register_DiagSERDESPacket3);
	form.description = "Link 3 Induced Error Sequence Number";
	std::stringstream o;
	o << std::dec << ReadSERDESInduceErrorSequenceNumber(DTC_Link_3, form.value);
	form.vals.push_back(o.str());
	return form;
}

DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatSERDESInduceErrorSequenceNumberLink4()
{
	auto form = CreateFormatter(DTC_Register_DiagSERDESPacket4);
	form.description = "Link 4 Induced Error Sequence Number";
	std::stringstream o;
	o << std::dec << ReadSERDESInduceErrorSequenceNumber(DTC_Link_4, form.value);
	form.vals.push_back(o.str());
	return form;
}

DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatSERDESInduceErrorSequenceNumberLink5()
{
	auto form = CreateFormatter(DTC_Register_DiagSERDESPacket5);
	form.description = "Link 5 Induced Error Sequence Number";
	std::stringstream o;
	o << std::dec << ReadSERDESInduceErrorSequenceNumber(DTC_Link_5, form.value);
	form.vals.push_back(o.str());
	return form;
}

// DTCLib::DTC_DDRFlags DTCLib::DTC_Registers::ReadDDRFlags(uint8_t buffer_id)
// {
// 	return DTC_DDRFlags(ReadDDRLinkBufferFullFlags()[buffer_id],
// 						ReadDDRLinkBufferEmptyFlags()[buffer_id],
// 						ReadDDRLinkBufferHalfFullFlags()[buffer_id],
// 						ReadDDREventBuilderBufferFullFlags()[buffer_id],
// 						ReadDDREventBuilderBufferEmptyFlags()[buffer_id],
// 						ReadDDREventBuilderBufferHalfFullFlags()[buffer_id]);
// }

// std::bitset<128> DTCLib::DTC_Registers::ReadDDRLinkBufferFullFlags()
// {
// 	uint32_t word0 = ReadRegister_(DTC_Register_DDR3LinkBufferFullFlags0);
// 	uint32_t word1 = ReadRegister_(DTC_Register_DDR3LinkBufferFullFlags1);
// 	uint32_t word2 = ReadRegister_(DTC_Register_DDR3LinkBufferFullFlags2);
// 	uint32_t word3 = ReadRegister_(DTC_Register_DDR3LinkBufferFullFlags3);
// 	std::bitset<128> out;
// 	for (size_t ii = 0; ii < 32; ++ii)
// 	{
// 		out[ii] = (word0 >> ii) & 0x1;
// 		out[ii + 32] = (word1 >> ii) & 0x1;
// 		out[ii + 64] = (word2 >> ii) & 0x1;
// 		out[ii + 96] = (word3 >> ii) & 0x1;
// 	}
// 	return out;
// }

// std::bitset<128> DTCLib::DTC_Registers::ReadDDRLinkBufferEmptyFlags()
// {
// 	uint32_t word0 = ReadRegister_(DTC_Register_DDR3LinkBufferEmptyFlags0);
// 	uint32_t word1 = ReadRegister_(DTC_Register_DDR3LinkBufferEmptyFlags1);
// 	uint32_t word2 = ReadRegister_(DTC_Register_DDR3LinkBufferEmptyFlags2);
// 	uint32_t word3 = ReadRegister_(DTC_Register_DDR3LinkBufferEmptyFlags3);
// 	std::bitset<128> out;
// 	for (size_t ii = 0; ii < 32; ++ii)
// 	{
// 		out[ii] = (word0 >> ii) & 0x1;
// 		out[ii + 32] = (word1 >> ii) & 0x1;
// 		out[ii + 64] = (word2 >> ii) & 0x1;
// 		out[ii + 96] = (word3 >> ii) & 0x1;
// 	}
// 	return out;
// }

// std::bitset<128> DTCLib::DTC_Registers::ReadDDRLinkBufferHalfFullFlags()
// {
// 	uint32_t word0 = ReadRegister_(DTC_Register_DDR3LinkBufferHalfFullFlags0);
// 	uint32_t word1 = ReadRegister_(DTC_Register_DDR3LinkBufferHalfFullFlags1);
// 	uint32_t word2 = ReadRegister_(DTC_Register_DDR3LinkBufferHalfFullFlags2);
// 	uint32_t word3 = ReadRegister_(DTC_Register_DDR3LinkBufferHalfFullFlags3);
// 	std::bitset<128> out;
// 	for (size_t ii = 0; ii < 32; ++ii)
// 	{
// 		out[ii] = (word0 >> ii) & 0x1;
// 		out[ii + 32] = (word1 >> ii) & 0x1;
// 		out[ii + 64] = (word2 >> ii) & 0x1;
// 		out[ii + 96] = (word3 >> ii) & 0x1;
// 	}
// 	return out;
// }

// std::bitset<128> DTCLib::DTC_Registers::ReadDDREventBuilderBufferFullFlags()
// {
// 	uint32_t word0 = ReadRegister_(DTC_Register_DDR3EVBBufferFullFlags0);
// 	uint32_t word1 = ReadRegister_(DTC_Register_DDR3EVBBufferFullFlags1);
// 	uint32_t word2 = ReadRegister_(DTC_Register_DDR3EVBBufferFullFlags2);
// 	uint32_t word3 = ReadRegister_(DTC_Register_DDR3EVBBufferFullFlags3);
// 	std::bitset<128> out;
// 	for (size_t ii = 0; ii < 32; ++ii)
// 	{
// 		out[ii] = (word0 >> ii) & 0x1;
// 		out[ii + 32] = (word1 >> ii) & 0x1;
// 		out[ii + 64] = (word2 >> ii) & 0x1;
// 		out[ii + 96] = (word3 >> ii) & 0x1;
// 	}
// 	return out;
// }

// std::bitset<128> DTCLib::DTC_Registers::ReadDDREventBuilderBufferEmptyFlags()
// {
// 	uint32_t word0 = ReadRegister_(DTC_Register_DDR3EVBBufferEmptyFlags0);
// 	uint32_t word1 = ReadRegister_(DTC_Register_DDR3EVBBufferEmptyFlags1);
// 	uint32_t word2 = ReadRegister_(DTC_Register_DDR3EVBBufferEmptyFlags2);
// 	uint32_t word3 = ReadRegister_(DTC_Register_DDR3EVBBufferEmptyFlags3);
// 	std::bitset<128> out;
// 	for (size_t ii = 0; ii < 32; ++ii)
// 	{
// 		out[ii] = (word0 >> ii) & 0x1;
// 		out[ii + 32] = (word1 >> ii) & 0x1;
// 		out[ii + 64] = (word2 >> ii) & 0x1;
// 		out[ii + 96] = (word3 >> ii) & 0x1;
// 	}
// 	return out;
// }

// std::bitset<128> DTCLib::DTC_Registers::ReadDDREventBuilderBufferHalfFullFlags()
// {
// 	uint32_t word0 = ReadRegister_(DTC_Register_DDR3EVBBufferHalfFullFlags0);
// 	uint32_t word1 = ReadRegister_(DTC_Register_DDR3EVBBufferHalfFullFlags1);
// 	uint32_t word2 = ReadRegister_(DTC_Register_DDR3EVBBufferHalfFullFlags2);
// 	uint32_t word3 = ReadRegister_(DTC_Register_DDR3EVBBufferHalfFullFlags3);
// 	std::bitset<128> out;
// 	for (size_t ii = 0; ii < 32; ++ii)
// 	{
// 		out[ii] = (word0 >> ii) & 0x1;
// 		out[ii + 32] = (word1 >> ii) & 0x1;
// 		out[ii + 64] = (word2 >> ii) & 0x1;
// 		out[ii + 96] = (word3 >> ii) & 0x1;
// 	}
// 	return out;
// }

// DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatDDRLinkBufferEmptyFlags0()
// {
// 	auto form = CreateFormatter(DTC_Register_DDR3LinkBufferEmptyFlags0);
// 	form.description = "DDR Link Buffer Empty Flags 0-31";
// 	return form;
// }

// DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatDDRLinkBufferEmptyFlags1()
// {
// 	auto form = CreateFormatter(DTC_Register_DDR3LinkBufferEmptyFlags1);
// 	form.description = "DDR Link Buffer Empty Flags 63-32";
// 	return form;
// }

// DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatDDRLinkBufferEmptyFlags2()
// {
// 	auto form = CreateFormatter(DTC_Register_DDR3LinkBufferEmptyFlags2);
// 	form.description = "DDR Link Buffer Empty Flags 95-64";
// 	return form;
// }

// DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatDDRLinkBufferEmptyFlags3()
// {
// 	auto form = CreateFormatter(DTC_Register_DDR3LinkBufferEmptyFlags3);
// 	form.description = "DDR Link Buffer Empty Flags 127-96";
// 	return form;
// }

// DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatDDRLinkBufferHalfFullFlags0()
// {
// 	auto form = CreateFormatter(DTC_Register_DDR3LinkBufferHalfFullFlags0);
// 	form.description = "DDR Link Buffer Half-Full Flags 0-31";
// 	return form;
// }

// DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatDDRLinkBufferHalfFullFlags1()
// {
// 	auto form = CreateFormatter(DTC_Register_DDR3LinkBufferHalfFullFlags1);
// 	form.description = "DDR Link Buffer Half-Full Flags 63-32";
// 	return form;
// }

// DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatDDRLinkBufferHalfFullFlags2()
// {
// 	auto form = CreateFormatter(DTC_Register_DDR3LinkBufferHalfFullFlags2);
// 	form.description = "DDR Link Buffer Half-Full Flags 95-64";
// 	return form;
// }

// DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatDDRLinkBufferHalfFullFlags3()
// {
// 	auto form = CreateFormatter(DTC_Register_DDR3LinkBufferHalfFullFlags3);
// 	form.description = "DDR Link Buffer Half-Full Flags 127-96";
// 	return form;
// }

// DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatDDRLinkBufferFullFlags0()
// {
// 	auto form = CreateFormatter(DTC_Register_DDR3LinkBufferEmptyFlags0);
// 	form.description = "DDR Link Buffer Full Flags 0-31";
// 	return form;
// }

// DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatDDRLinkBufferFullFlags1()
// {
// 	auto form = CreateFormatter(DTC_Register_DDR3LinkBufferEmptyFlags1);
// 	form.description = "DDR Link Buffer Full Flags 63-32";
// 	return form;
// }

// DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatDDRLinkBufferFullFlags2()
// {
// 	auto form = CreateFormatter(DTC_Register_DDR3LinkBufferEmptyFlags2);
// 	form.description = "DDR Link Buffer Full Flags 95-64";
// 	return form;
// }

// DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatDDRLinkBufferFullFlags3()
// {
// 	auto form = CreateFormatter(DTC_Register_DDR3LinkBufferEmptyFlags3);
// 	form.description = "DDR Link Buffer Full Flags 127-96";
// 	return form;
// }

// DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatDDREventBuilderBufferEmptyFlags0()
// {
// 	auto form = CreateFormatter(DTC_Register_DDR3EVBBufferEmptyFlags0);
// 	form.description = "DDR EVB Buffer Empty Flags 0-31";
// 	return form;
// }

// DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatDDREventBuilderBufferEmptyFlags1()
// {
// 	auto form = CreateFormatter(DTC_Register_DDR3EVBBufferEmptyFlags1);
// 	form.description = "DDR EVB Buffer Empty Flags 63-32";
// 	return form;
// }

// DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatDDREventBuilderBufferEmptyFlags2()
// {
// 	auto form = CreateFormatter(DTC_Register_DDR3EVBBufferEmptyFlags2);
// 	form.description = "DDR EVB Buffer Empty Flags 95-64";
// 	return form;
// }

// DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatDDREventBuilderBufferEmptyFlags3()
// {
// 	auto form = CreateFormatter(DTC_Register_DDR3EVBBufferEmptyFlags3);
// 	form.description = "DDR EVB Buffer Empty Flags 127-96";
// 	return form;
// }

// DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatDDREventBuilderBufferHalfFullFlags0()
// {
// 	auto form = CreateFormatter(DTC_Register_DDR3LinkBufferHalfFullFlags0);
// 	form.description = "DDR Link Buffer Half-Full Flags 0-31";
// 	return form;
// }

// DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatDDREventBuilderBufferHalfFullFlags1()
// {
// 	auto form = CreateFormatter(DTC_Register_DDR3LinkBufferHalfFullFlags1);
// 	form.description = "DDR Link Buffer Half-Full Flags 63-32";
// 	return form;
// }

// DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatDDREventBuilderBufferHalfFullFlags2()
// {
// 	auto form = CreateFormatter(DTC_Register_DDR3LinkBufferHalfFullFlags2);
// 	form.description = "DDR Link Buffer Half-Full Flags 95-64";
// 	return form;
// }

// DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatDDREventBuilderBufferHalfFullFlags3()
// {
// 	auto form = CreateFormatter(DTC_Register_DDR3LinkBufferHalfFullFlags3);
// 	form.description = "DDR Link Buffer Half-Full Flags 127-96";
// 	return form;
// }

// DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatDDREventBuilderBufferFullFlags0()
// {
// 	auto form = CreateFormatter(DTC_Register_DDR3LinkBufferFullFlags0);
// 	form.description = "DDR Link Buffer Full Flags 0-31";
// 	return form;
// }

// DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatDDREventBuilderBufferFullFlags1()
// {
// 	auto form = CreateFormatter(DTC_Register_DDR3LinkBufferFullFlags1);
// 	form.description = "DDR Link Buffer Full Flags 63-32";
// 	return form;
// }

// DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatDDREventBuilderBufferFullFlags2()
// {
// 	auto form = CreateFormatter(DTC_Register_DDR3LinkBufferFullFlags2);
// 	form.description = "DDR Link Buffer Full Flags 95-64";
// 	return form;
// }

// DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatDDREventBuilderBufferFullFlags3()
// {
// 	auto form = CreateFormatter(DTC_Register_DDR3LinkBufferFullFlags3);
// 	form.description = "DDR Link Buffer Full Flags 127-96";
// 	return form;
// }

uint32_t DTCLib::DTC_Registers::ReadDataPendingDiagnosticTimer(DTC_Link_ID const& link, std::optional<uint32_t> val)
{
	switch (link)
	{
		case DTC_Link_0:
			return val.has_value() ? *val : ReadRegister_(DTC_Register_DataPendingDiagTimer_Link0);
		case DTC_Link_1:
			return val.has_value() ? *val : ReadRegister_(DTC_Register_DataPendingDiagTimer_Link1);
		case DTC_Link_2:
			return val.has_value() ? *val : ReadRegister_(DTC_Register_DataPendingDiagTimer_Link2);
		case DTC_Link_3:
			return val.has_value() ? *val : ReadRegister_(DTC_Register_DataPendingDiagTimer_Link3);
		case DTC_Link_4:
			return val.has_value() ? *val : ReadRegister_(DTC_Register_DataPendingDiagTimer_Link4);
		case DTC_Link_5:
			return val.has_value() ? *val : ReadRegister_(DTC_Register_DataPendingDiagTimer_Link5);
		default: {
			__SS__ << "Illegal link index provided: " << link << __E__;
			__SS_THROW__;
		}
	}
}

void DTCLib::DTC_Registers::ResetDataPendingDiagnosticTimerFIFO(DTC_Link_ID const& link)
{
	switch (link)
	{
		case DTC_Link_0:
			WriteRegister_(0, DTC_Register_DataPendingDiagTimer_Link0);
			break;
		case DTC_Link_1:
			WriteRegister_(0, DTC_Register_DataPendingDiagTimer_Link1);
			break;
		case DTC_Link_2:
			WriteRegister_(0, DTC_Register_DataPendingDiagTimer_Link2);
			break;
		case DTC_Link_3:
			WriteRegister_(0, DTC_Register_DataPendingDiagTimer_Link3);
			break;
		case DTC_Link_4:
			WriteRegister_(0, DTC_Register_DataPendingDiagTimer_Link4);
			break;
		case DTC_Link_5:
			WriteRegister_(0, DTC_Register_DataPendingDiagTimer_Link5);
			break;
		default: {
			__SS__ << "Illegal link index provided: " << link << __E__;
			__SS_THROW__;
		}
	}
}

DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatDataPendingDiagnosticTimerLink0()
{
	auto form = CreateFormatter(DTC_Register_DataPendingDiagTimer_Link0);
	form.description = "Data Pending Diagnostic Timer Link 0";
	std::stringstream o;
	o << std::dec << ReadDataPendingDiagnosticTimer(DTC_Link_0, form.value);
	form.vals.push_back(o.str());
	return form;
}

DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatDataPendingDiagnosticTimerLink1()
{
	auto form = CreateFormatter(DTC_Register_DataPendingDiagTimer_Link1);
	form.description = "Data Pending Diagnostic Timer Link 1";
	std::stringstream o;
	o << std::dec << ReadDataPendingDiagnosticTimer(DTC_Link_1, form.value);
	form.vals.push_back(o.str());
	return form;
}

DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatDataPendingDiagnosticTimerLink2()
{
	auto form = CreateFormatter(DTC_Register_DataPendingDiagTimer_Link2);
	form.description = "Data Pending Diagnostic Timer Link 2";
	std::stringstream o;
	o << std::dec << ReadDataPendingDiagnosticTimer(DTC_Link_2, form.value);
	form.vals.push_back(o.str());
	return form;
}

DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatDataPendingDiagnosticTimerLink3()
{
	auto form = CreateFormatter(DTC_Register_DataPendingDiagTimer_Link3);
	form.description = "Data Pending Diagnostic Timer Link 3";
	std::stringstream o;
	o << std::dec << ReadDataPendingDiagnosticTimer(DTC_Link_3, form.value);
	form.vals.push_back(o.str());
	return form;
}

DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatDataPendingDiagnosticTimerLink4()
{
	auto form = CreateFormatter(DTC_Register_DataPendingDiagTimer_Link4);
	form.description = "Data Pending Diagnostic Timer Link 4";
	std::stringstream o;
	o << std::dec << ReadDataPendingDiagnosticTimer(DTC_Link_4, form.value);
	form.vals.push_back(o.str());
	return form;
}

DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatDataPendingDiagnosticTimerLink5()
{
	auto form = CreateFormatter(DTC_Register_DataPendingDiagTimer_Link5);
	form.description = "Data Pending Diagnostic Timer Link 5";
	std::stringstream o;
	o << std::dec << ReadDataPendingDiagnosticTimer(DTC_Link_5, form.value);
	form.vals.push_back(o.str());
	return form;
}

uint32_t DTCLib::DTC_Registers::ReadSERDESCharacterNotInTableErrorCount(DTC_Link_ID const& link, std::optional<uint32_t> val)
{
	DTC_Register reg;
	switch (link)
	{
		case DTC_Link_0:
			reg = DTC_Register_SERDES_CharacterNotInTableErrorCount_Link0;
			break;
		case DTC_Link_1:
			reg = DTC_Register_SERDES_CharacterNotInTableErrorCount_Link1;
			break;
		case DTC_Link_2:
			reg = DTC_Register_SERDES_CharacterNotInTableErrorCount_Link2;
			break;
		case DTC_Link_3:
			reg = DTC_Register_SERDES_CharacterNotInTableErrorCount_Link3;
			break;
		case DTC_Link_4:
			reg = DTC_Register_SERDES_CharacterNotInTableErrorCount_Link4;
			break;
		case DTC_Link_5:
			reg = DTC_Register_SERDES_CharacterNotInTableErrorCount_Link5;
			break;
		case DTC_Link_CFO:
			reg = DTC_Register_SERDES_CharacterNotInTableErrorCount_CFOLink;
			break;
		default: {
			__SS__ << "Illegal link index provided: " << link << __E__;
			__SS_THROW__;
		}
	}
	return val.has_value() ? *val : ReadRegister_(reg);
}
void DTCLib::DTC_Registers::ClearSERDESCharacterNotInTableErrorCount(DTC_Link_ID const& link)
{
	DTC_Register reg;
	switch (link)
	{
		case DTC_Link_0:
			reg = DTC_Register_SERDES_CharacterNotInTableErrorCount_Link0;
			break;
		case DTC_Link_1:
			reg = DTC_Register_SERDES_CharacterNotInTableErrorCount_Link1;
			break;
		case DTC_Link_2:
			reg = DTC_Register_SERDES_CharacterNotInTableErrorCount_Link2;
			break;
		case DTC_Link_3:
			reg = DTC_Register_SERDES_CharacterNotInTableErrorCount_Link3;
			break;
		case DTC_Link_4:
			reg = DTC_Register_SERDES_CharacterNotInTableErrorCount_Link4;
			break;
		case DTC_Link_5:
			reg = DTC_Register_SERDES_CharacterNotInTableErrorCount_Link5;
			break;
		case DTC_Link_CFO:
			reg = DTC_Register_SERDES_CharacterNotInTableErrorCount_CFOLink;
			break;
		default: {
			__SS__ << "Illegal link index provided: " << link << __E__;
			__SS_THROW__;
		}
	}
	WriteRegister_(1, reg);
}

DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatSERDESCharacterNotInTableErrorCountLink0()
{
	auto form = CreateFormatter(DTC_Register_SERDES_CharacterNotInTableErrorCount_Link0);
	form.description = "SERDES Character Not In Table Error Count Link 0";
	std::stringstream o;
	o << std::dec << ReadSERDESCharacterNotInTableErrorCount(DTC_Link_0);
	form.vals.push_back(o.str());
	return form;
}
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatSERDESCharacterNotInTableErrorCountLink1()
{
	auto form = CreateFormatter(DTC_Register_SERDES_CharacterNotInTableErrorCount_Link1);
	form.description = "SERDES Character Not In Table Error Count Link 1";
	std::stringstream o;
	o << std::dec << ReadSERDESCharacterNotInTableErrorCount(DTC_Link_1);
	form.vals.push_back(o.str());
	return form;
}
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatSERDESCharacterNotInTableErrorCountLink2()
{
	auto form = CreateFormatter(DTC_Register_SERDES_CharacterNotInTableErrorCount_Link2);
	form.description = "SERDES Character Not In Table Error Count Link 2";
	std::stringstream o;
	o << std::dec << ReadSERDESCharacterNotInTableErrorCount(DTC_Link_2);
	form.vals.push_back(o.str());
	return form;
}
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatSERDESCharacterNotInTableErrorCountLink3()
{
	auto form = CreateFormatter(DTC_Register_SERDES_CharacterNotInTableErrorCount_Link3);
	form.description = "SERDES Character Not In Table Error Count Link 3";
	std::stringstream o;
	o << std::dec << ReadSERDESCharacterNotInTableErrorCount(DTC_Link_3);
	form.vals.push_back(o.str());
	return form;
}
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatSERDESCharacterNotInTableErrorCountLink4()
{
	auto form = CreateFormatter(DTC_Register_SERDES_CharacterNotInTableErrorCount_Link4);
	form.description = "SERDES Character Not In Table Error Count Link 4";
	std::stringstream o;
	o << std::dec << ReadSERDESCharacterNotInTableErrorCount(DTC_Link_4);
	form.vals.push_back(o.str());
	return form;
}
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatSERDESCharacterNotInTableErrorCountLink5()
{
	auto form = CreateFormatter(DTC_Register_SERDES_CharacterNotInTableErrorCount_Link5);
	form.description = "SERDES Character Not In Table Error Count Link 5";
	std::stringstream o;
	o << std::dec << ReadSERDESCharacterNotInTableErrorCount(DTC_Link_5);
	form.vals.push_back(o.str());
	return form;
}
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatSERDESCharacterNotInTableErrorCountCFOLink()
{
	auto form = CreateFormatter(DTC_Register_SERDES_CharacterNotInTableErrorCount_CFOLink);
	form.description = "SERDES Character Not In Table Error Count CFO Link";
	std::stringstream o;
	o << std::dec << ReadSERDESCharacterNotInTableErrorCount(DTC_Link_CFO);
	form.vals.push_back(o.str());
	return form;
}

uint32_t DTCLib::DTC_Registers::ReadSERDESRXDisparityErrorCount(DTC_Link_ID const& link, std::optional<uint32_t> val)
{
	DTC_Register reg;
	switch (link)
	{
		case DTC_Link_0:
			reg = DTC_Register_SERDES_RXDisparityErrorCount_Link0;
			break;
		case DTC_Link_1:
			reg = DTC_Register_SERDES_RXDisparityErrorCount_Link1;
			break;
		case DTC_Link_2:
			reg = DTC_Register_SERDES_RXDisparityErrorCount_Link2;
			break;
		case DTC_Link_3:
			reg = DTC_Register_SERDES_RXDisparityErrorCount_Link3;
			break;
		case DTC_Link_4:
			reg = DTC_Register_SERDES_RXDisparityErrorCount_Link4;
			break;
		case DTC_Link_5:
			reg = DTC_Register_SERDES_RXDisparityErrorCount_Link5;
			break;
		case DTC_Link_CFO:
			reg = DTC_Register_SERDES_RXDisparityErrorCount_CFOLink;
			break;
		default: {
			__SS__ << "Illegal link index provided: " << link << __E__;
			__SS_THROW__;
		}
	}
	return val.has_value() ? *val : ReadRegister_(reg);
}
void DTCLib::DTC_Registers::ClearSERDESRXDisparityErrorCount(DTC_Link_ID const& link)
{
	DTC_Register reg;
	switch (link)
	{
		case DTC_Link_0:
			reg = DTC_Register_SERDES_RXDisparityErrorCount_Link0;
			break;
		case DTC_Link_1:
			reg = DTC_Register_SERDES_RXDisparityErrorCount_Link1;
			break;
		case DTC_Link_2:
			reg = DTC_Register_SERDES_RXDisparityErrorCount_Link2;
			break;
		case DTC_Link_3:
			reg = DTC_Register_SERDES_RXDisparityErrorCount_Link3;
			break;
		case DTC_Link_4:
			reg = DTC_Register_SERDES_RXDisparityErrorCount_Link4;
			break;
		case DTC_Link_5:
			reg = DTC_Register_SERDES_RXDisparityErrorCount_Link5;
			break;
		case DTC_Link_CFO:
			reg = DTC_Register_SERDES_RXDisparityErrorCount_CFOLink;
			break;
		default: {
			__SS__ << "Illegal link index provided: " << link << __E__;
			__SS_THROW__;
		}
	}
	WriteRegister_(1, reg);
}

DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatSERDESRXDisparityErrorCountLink0()
{
	auto form = CreateFormatter(DTC_Register_SERDES_RXDisparityErrorCount_Link0);
	form.description = "SERDES RX Disparity Error Count Link 0";
	std::stringstream o;
	o << std::dec << ReadSERDESRXDisparityErrorCount(DTC_Link_0, form.value);
	form.vals.push_back(o.str());
	return form;
}
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatSERDESRXDisparityErrorCountLink1()
{
	auto form = CreateFormatter(DTC_Register_SERDES_RXDisparityErrorCount_Link1);
	form.description = "SERDES RX Disparity Error Count Link 1";
	std::stringstream o;
	o << std::dec << ReadSERDESRXDisparityErrorCount(DTC_Link_1, form.value);
	form.vals.push_back(o.str());
	return form;
}
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatSERDESRXDisparityErrorCountLink2()
{
	auto form = CreateFormatter(DTC_Register_SERDES_RXDisparityErrorCount_Link2);
	form.description = "SERDES RX Disparity Error Count Link 2";
	std::stringstream o;
	o << std::dec << ReadSERDESRXDisparityErrorCount(DTC_Link_2, form.value);
	form.vals.push_back(o.str());
	return form;
}
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatSERDESRXDisparityErrorCountLink3()
{
	auto form = CreateFormatter(DTC_Register_SERDES_RXDisparityErrorCount_Link3);
	form.description = "SERDES RX Disparity Error Count Link 3";
	std::stringstream o;
	o << std::dec << ReadSERDESRXDisparityErrorCount(DTC_Link_3, form.value);
	form.vals.push_back(o.str());
	return form;
}
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatSERDESRXDisparityErrorCountLink4()
{
	auto form = CreateFormatter(DTC_Register_SERDES_RXDisparityErrorCount_Link4);
	form.description = "SERDES RX Disparity Error Count Link 4";
	std::stringstream o;
	o << std::dec << ReadSERDESRXDisparityErrorCount(DTC_Link_4, form.value);
	form.vals.push_back(o.str());
	return form;
}
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatSERDESRXDisparityErrorCountLink5()
{
	auto form = CreateFormatter(DTC_Register_SERDES_RXDisparityErrorCount_Link5);
	form.description = "SERDES RX Disparity Error Count Link 5";
	std::stringstream o;
	o << std::dec << ReadSERDESRXDisparityErrorCount(DTC_Link_5, form.value);
	form.vals.push_back(o.str());
	return form;
}
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatSERDESRXDisparityErrorCountCFOLink()
{
	auto form = CreateFormatter(DTC_Register_SERDES_RXDisparityErrorCount_CFOLink);
	form.description = "SERDES RX Disparity Error Count CFO Link";
	std::stringstream o;
	o << std::dec << ReadSERDESRXDisparityErrorCount(DTC_Link_CFO, form.value);
	form.vals.push_back(o.str());
	return form;
}

uint32_t DTCLib::DTC_Registers::ReadSERDESRXPRBSErrorCount(DTC_Link_ID const& link, std::optional<uint32_t> val)
{
	DTC_Register reg;
	switch (link)
	{
		case DTC_Link_0:
			reg = DTC_Register_SERDES_RXPRBSErrorCount_Link0;
			break;
		case DTC_Link_1:
			reg = DTC_Register_SERDES_RXPRBSErrorCount_Link1;
			break;
		case DTC_Link_2:
			reg = DTC_Register_SERDES_RXPRBSErrorCount_Link2;
			break;
		case DTC_Link_3:
			reg = DTC_Register_SERDES_RXPRBSErrorCount_Link3;
			break;
		case DTC_Link_4:
			reg = DTC_Register_SERDES_RXPRBSErrorCount_Link4;
			break;
		case DTC_Link_5:
			reg = DTC_Register_SERDES_RXPRBSErrorCount_Link5;
			break;
		case DTC_Link_CFO:
			reg = DTC_Register_SERDES_RXPRBSErrorCount_CFOLink;
			break;
		default: {
			__SS__ << "Illegal link index provided: " << link << __E__;
			__SS_THROW__;
		}
	}
	return val.has_value() ? *val : ReadRegister_(reg);
}
void DTCLib::DTC_Registers::ClearSERDESRXPRBSErrorCount(DTC_Link_ID const& link)
{
	DTC_Register reg;
	switch (link)
	{
		case DTC_Link_0:
			reg = DTC_Register_SERDES_RXPRBSErrorCount_Link0;
			break;
		case DTC_Link_1:
			reg = DTC_Register_SERDES_RXPRBSErrorCount_Link1;
			break;
		case DTC_Link_2:
			reg = DTC_Register_SERDES_RXPRBSErrorCount_Link2;
			break;
		case DTC_Link_3:
			reg = DTC_Register_SERDES_RXPRBSErrorCount_Link3;
			break;
		case DTC_Link_4:
			reg = DTC_Register_SERDES_RXPRBSErrorCount_Link4;
			break;
		case DTC_Link_5:
			reg = DTC_Register_SERDES_RXPRBSErrorCount_Link5;
			break;
		case DTC_Link_CFO:
			reg = DTC_Register_SERDES_RXPRBSErrorCount_CFOLink;
			break;
		default: {
			__SS__ << "Illegal link index provided: " << link << __E__;
			__SS_THROW__;
		}
	}
	WriteRegister_(1, reg);
}

DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatSERDESRXPRBSErrorCountLink0()
{
	auto form = CreateFormatter(DTC_Register_SERDES_RXPRBSErrorCount_Link0);
	form.description = "SERDES RX PRBS Error Count Link 0";
	std::stringstream o;
	o << std::dec << ReadSERDESRXPRBSErrorCount(DTC_Link_0, form.value);
	form.vals.push_back(o.str());
	return form;
}
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatSERDESRXPRBSErrorCountLink1()
{
	auto form = CreateFormatter(DTC_Register_SERDES_RXPRBSErrorCount_Link1);
	form.description = "SERDES RX PRBS Error Count Link 1";
	std::stringstream o;
	o << std::dec << ReadSERDESRXPRBSErrorCount(DTC_Link_1, form.value);
	form.vals.push_back(o.str());
	return form;
}
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatSERDESRXPRBSErrorCountLink2()
{
	auto form = CreateFormatter(DTC_Register_SERDES_RXPRBSErrorCount_Link2);
	form.description = "SERDES RX PRBS Error Count Link 2";
	std::stringstream o;
	o << std::dec << ReadSERDESRXPRBSErrorCount(DTC_Link_2, form.value);
	form.vals.push_back(o.str());
	return form;
}
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatSERDESRXPRBSErrorCountLink3()
{
	auto form = CreateFormatter(DTC_Register_SERDES_RXPRBSErrorCount_Link3);
	form.description = "SERDES RX PRBS Error Count Link 3";
	std::stringstream o;
	o << std::dec << ReadSERDESRXPRBSErrorCount(DTC_Link_3, form.value);
	form.vals.push_back(o.str());
	return form;
}
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatSERDESRXPRBSErrorCountLink4()
{
	auto form = CreateFormatter(DTC_Register_SERDES_RXPRBSErrorCount_Link4);
	form.description = "SERDES RX PRBS Error Count Link 4";
	std::stringstream o;
	o << std::dec << ReadSERDESRXPRBSErrorCount(DTC_Link_4, form.value);
	form.vals.push_back(o.str());
	return form;
}
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatSERDESRXPRBSErrorCountLink5()
{
	auto form = CreateFormatter(DTC_Register_SERDES_RXPRBSErrorCount_Link5);
	form.description = "SERDES RX PRBS Error Count Link 5";
	std::stringstream o;
	o << std::dec << ReadSERDESRXPRBSErrorCount(DTC_Link_5, form.value);
	form.vals.push_back(o.str());
	return form;
}
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatSERDESRXPRBSErrorCountCFOLink()
{
	auto form = CreateFormatter(DTC_Register_SERDES_RXPRBSErrorCount_CFOLink);
	form.description = "SERDES RX PRBS Error Count CFO Link";
	std::stringstream o;
	o << std::dec << ReadSERDESRXPRBSErrorCount(DTC_Link_CFO, form.value);
	form.vals.push_back(o.str());
	return form;
}

uint32_t DTCLib::DTC_Registers::ReadSERDESRXCRCErrorCount(DTC_Link_ID const& link, std::optional<uint32_t> val)
{
	DTC_Register reg;
	switch (link)
	{
		case DTC_Link_0:
			reg = DTC_Register_SERDES_RXCRCErrorCount_Link0;
			break;
		case DTC_Link_1:
			reg = DTC_Register_SERDES_RXCRCErrorCount_Link1;
			break;
		case DTC_Link_2:
			reg = DTC_Register_SERDES_RXCRCErrorCount_Link2;
			break;
		case DTC_Link_3:
			reg = DTC_Register_SERDES_RXCRCErrorCount_Link3;
			break;
		case DTC_Link_4:
			reg = DTC_Register_SERDES_RXCRCErrorCount_Link4;
			break;
		case DTC_Link_5:
			reg = DTC_Register_SERDES_RXCRCErrorCount_Link5;
			break;
		case DTC_Link_CFO:
			reg = DTC_Register_SERDES_RXCRCErrorCount_CFOLink;
			break;
		default: {
			__SS__ << "Illegal link index provided: " << link << __E__;
			__SS_THROW__;
		}
	}
	return val.has_value() ? *val : ReadRegister_(reg);
}
void DTCLib::DTC_Registers::ClearSERDESRXCRCErrorCount(DTC_Link_ID const& link)
{
	DTC_Register reg;
	switch (link)
	{
		case DTC_Link_0:
			reg = DTC_Register_SERDES_RXCRCErrorCount_Link0;
			break;
		case DTC_Link_1:
			reg = DTC_Register_SERDES_RXCRCErrorCount_Link1;
			break;
		case DTC_Link_2:
			reg = DTC_Register_SERDES_RXCRCErrorCount_Link2;
			break;
		case DTC_Link_3:
			reg = DTC_Register_SERDES_RXCRCErrorCount_Link3;
			break;
		case DTC_Link_4:
			reg = DTC_Register_SERDES_RXCRCErrorCount_Link4;
			break;
		case DTC_Link_5:
			reg = DTC_Register_SERDES_RXCRCErrorCount_Link5;
			break;
		case DTC_Link_CFO:
			reg = DTC_Register_SERDES_RXCRCErrorCount_CFOLink;
			break;
		default: {
			__SS__ << "Illegal link index provided: " << link << __E__;
			__SS_THROW__;
		}
	}
	WriteRegister_(1, reg);
}

DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatSERDESRXCRCErrorCountLink0()
{
	auto form = CreateFormatter(DTC_Register_SERDES_RXCRCErrorCount_Link0);
	form.description = "SERDES RX CRC Error Count Link 0";
	std::stringstream o;
	o << std::dec << ReadSERDESRXCRCErrorCount(DTC_Link_0, form.value);
	form.vals.push_back(o.str());
	return form;
}
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatSERDESRXCRCErrorCountLink1()
{
	auto form = CreateFormatter(DTC_Register_SERDES_RXCRCErrorCount_Link1);
	form.description = "SERDES RX CRC Error Count Link 1";
	std::stringstream o;
	o << std::dec << ReadSERDESRXCRCErrorCount(DTC_Link_1, form.value);
	form.vals.push_back(o.str());
	return form;
}
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatSERDESRXCRCErrorCountLink2()
{
	auto form = CreateFormatter(DTC_Register_SERDES_RXCRCErrorCount_Link2);
	form.description = "SERDES RX CRC Error Count Link 2";
	std::stringstream o;
	o << std::dec << ReadSERDESRXCRCErrorCount(DTC_Link_2, form.value);
	form.vals.push_back(o.str());
	return form;
}
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatSERDESRXCRCErrorCountLink3()
{
	auto form = CreateFormatter(DTC_Register_SERDES_RXCRCErrorCount_Link3);
	form.description = "SERDES RX CRC Error Count Link 3";
	std::stringstream o;
	o << std::dec << ReadSERDESRXCRCErrorCount(DTC_Link_3, form.value);
	form.vals.push_back(o.str());
	return form;
}
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatSERDESRXCRCErrorCountLink4()
{
	auto form = CreateFormatter(DTC_Register_SERDES_RXCRCErrorCount_Link4);
	form.description = "SERDES RX CRC Error Count Link 4";
	std::stringstream o;
	o << std::dec << ReadSERDESRXCRCErrorCount(DTC_Link_4, form.value);
	form.vals.push_back(o.str());
	return form;
}
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatSERDESRXCRCErrorCountLink5()
{
	auto form = CreateFormatter(DTC_Register_SERDES_RXCRCErrorCount_Link5);
	form.description = "SERDES RX CRC Error Count Link 5";
	std::stringstream o;
	o << std::dec << ReadSERDESRXCRCErrorCount(DTC_Link_5, form.value);
	form.vals.push_back(o.str());
	return form;
}
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatSERDESRXCRCErrorCountCFOLink()
{
	auto form = CreateFormatter(DTC_Register_SERDES_RXCRCErrorCount_CFOLink);
	form.description = "SERDES RX CRC Error Count CFO Link";
	std::stringstream o;
	o << std::dec << ReadSERDESRXCRCErrorCount(DTC_Link_CFO, form.value);
	form.vals.push_back(o.str());
	return form;
}

// SERDES RX CRC Error Control
bool DTCLib::DTC_Registers::ReadEnableInduceSERDESRXCRCError(DTC_Link_ID const& link, std::optional<uint32_t> val)
{
	auto dataSet = std::bitset<32>(val.has_value() ? *val : ReadRegister_(DTC_Register_SERDES_RXCRCErrorControl));
	return dataSet[link];
}
void DTCLib::DTC_Registers::EnableInduceSERDESRXCRCError(DTC_Link_ID const& link)
{
	auto dataSet = std::bitset<32>(ReadRegister_(DTC_Register_SERDES_RXCRCErrorControl));
	dataSet[link] = 1;
	WriteRegister_(dataSet.to_ulong(), DTC_Register_SERDES_RXCRCErrorControl);
}
void DTCLib::DTC_Registers::DisableInduceSERDESRXCRCError(DTC_Link_ID const& link)
{
	auto dataSet = std::bitset<32>(ReadRegister_(DTC_Register_SERDES_RXCRCErrorControl));
	dataSet[link] = 0;
	WriteRegister_(dataSet.to_ulong(), DTC_Register_SERDES_RXCRCErrorControl);
}
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatSERDESRXCRCErrorControl()
{
	auto form = CreateFormatter(DTC_Register_SERDESTXRXInvertEnable);
	form.description = "SERDES RX CRC Error Control";
	form.vals.push_back("([ x = 1 (hi) ])");  // translation
	for (auto r : DTC_ROC_Links)
		form.vals.push_back(std::string("Induce Error Link ") + std::to_string(r) + ":  [" +
							(ReadEnableInduceSERDESRXCRCError(r, form.value) ? "x" : " ") + "]");

	form.vals.push_back(std::string("Incude Error CFO Link: [") +
						(ReadEnableInduceSERDESRXCRCError(DTC_Link_CFO, form.value) ? "x" : " ") + "]");

	return form;
}

uint32_t DTCLib::DTC_Registers::ReadEVBSERDESRXPacketErrorCounter(std::optional<uint32_t> val)
{
	return val.has_value() ? *val : ReadRegister_(DTC_Register_EBVSERDES_RXPacketErrorCount);
}
void DTCLib::DTC_Registers::ClearEVBSERDESRXPacketErrorCounter()
{
	WriteRegister_(1, DTC_Register_EBVSERDES_RXPacketErrorCount);
}
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatEVBSERDESRXPacketErrorCounter()
{
	auto form = CreateFormatter(DTC_Register_EBVSERDES_RXPacketErrorCount);
	form.description = "EVB SERDES RX Packet Error Counter";
	std::stringstream o;
	o << std::dec << ReadEVBSERDESRXPacketErrorCounter(form.value);
	form.vals.push_back(o.str());
	return form;
}

// Jitter Attenuator SERDES RX Recovered Clock LOS Counter
uint32_t DTCLib::DTC_Registers::ReadJitterAttenuatorRecoveredClockLOSCount(std::optional<uint32_t> val)
{
	return val.has_value() ? *val : ReadRegister_(DTC_Register_JitterAttenuator_SERDES_RXRecoveredClockLOSCount);
}
void DTCLib::DTC_Registers::ClearJitterAttenuatorRecoeveredClockLOSCount()
{
	WriteRegister_(1, DTC_Register_JitterAttenuator_SERDES_RXRecoveredClockLOSCount);
}
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatJitterAttenuatorRecoveredClockLOSCount()
{
	auto form = CreateFormatter(DTC_Register_JitterAttenuator_SERDES_RXRecoveredClockLOSCount);
	form.description = "Jitter Attenuator SERDES RX Recovered Clock LOS Counter";
	std::stringstream o;
	o << std::dec << ReadJitterAttenuatorRecoveredClockLOSCount(form.value);
	form.vals.push_back(o.str());
	return form;
}

// Jitter Attenuator SERDES RX External Clock LOS Counter
uint32_t DTCLib::DTC_Registers::ReadJitterAttenuatorExternalClockLOSCount(std::optional<uint32_t> val)
{
	return val.has_value() ? *val : ReadRegister_(DTC_Register_JitterAttenuator_SERDES_RXExternalClockLOSCount);
}
void DTCLib::DTC_Registers::ClearJitterAttenuatorExternalClockLOSCount()
{
	WriteRegister_(1, DTC_Register_JitterAttenuator_SERDES_RXExternalClockLOSCount);
}
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatJitterAttenuatorExternalClockLOSCount()
{
	auto form = CreateFormatter(DTC_Register_JitterAttenuator_SERDES_RXExternalClockLOSCount);
	form.description = "Jitter Attenuator SERDES RX External Clock LOS Counter";
	std::stringstream o;
	o << std::dec << ReadJitterAttenuatorExternalClockLOSCount(form.value);
	form.vals.push_back(o.str());
	return form;
}

// ROC Emulator Interpacket Delay
uint32_t DTCLib::DTC_Registers::ReadROCEmulatorInterpacketDelay(DTC_Link_ID const& link, std::optional<uint32_t> val)
{
	DTC_Register reg;
	switch (link)
	{
		case DTC_Link_0:
			reg = DTC_Register_ROCEmulator_InterpacketDelay_Link0;
			break;
		// case DTC_Link_1:
		// 	reg = DTC_Register_ROCEmulator_InterpacketDelay_Link1;
		// 	break;
		// case DTC_Link_2:
		// 	reg = DTC_Register_ROCEmulator_InterpacketDelay_Link2;
		// 	break;
		// case DTC_Link_3:
		// 	reg = DTC_Register_ROCEmulator_InterpacketDelay_Link3;
		// 	break;
		// case DTC_Link_4:
		// 	reg = DTC_Register_ROCEmulator_InterpacketDelay_Link4;
		// 	break;
		// case DTC_Link_5:
		// 	reg = DTC_Register_ROCEmulator_InterpacketDelay_Link5;
		// 	break;
		default: {
			__SS__ << "Illegal link index provided: " << link << __E__;
			__SS_THROW__;
		}
	}
	return val.has_value() ? *val : ReadRegister_(reg);
}
void DTCLib::DTC_Registers::SetROCEmulatorInterpacketDelay(DTC_Link_ID const& link, uint32_t delay)
{
	DTC_Register reg;
	switch (link)
	{
		case DTC_Link_0:
			reg = DTC_Register_ROCEmulator_InterpacketDelay_Link0;
			break;
		// case DTC_Link_1:
		// 	reg = DTC_Register_ROCEmulator_InterpacketDelay_Link1;
		// 	break;
		// case DTC_Link_2:
		// 	reg = DTC_Register_ROCEmulator_InterpacketDelay_Link2;
		// 	break;
		// case DTC_Link_3:
		// 	reg = DTC_Register_ROCEmulator_InterpacketDelay_Link3;
		// 	break;
		// case DTC_Link_4:
		// 	reg = DTC_Register_ROCEmulator_InterpacketDelay_Link4;
		// 	break;
		// case DTC_Link_5:
		// 	reg = DTC_Register_ROCEmulator_InterpacketDelay_Link5;
		// 	break;
		default: {
			__SS__ << "Illegal link index provided: " << link << __E__;
			__SS_THROW__;
		}
	}
	WriteRegister_(delay, reg);
}
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatROCEmulatorInterpacketDelayLink0()
{
	auto form = CreateFormatter(DTC_Register_ROCEmulator_InterpacketDelay_Link0);
	form.description = "ROC Emulator Interpacket Delay all Links (*5ns)";
	std::stringstream o;
	o << std::dec << ReadROCEmulatorInterpacketDelay(DTC_Link_0, form.value);
	form.vals.push_back(o.str());
	return form;
}
// DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatROCEmulatorInterpacketDelayLink1()
// {
// 	auto form = CreateFormatter(DTC_Register_ROCEmulator_InterpacketDelay_Link1);
// 	form.description = "ROC Emulator Interpacket Delay Link 1 (*5ns)";
// 	std::stringstream o;
// 	o << std::dec << ReadROCEmulatorInterpacketDelay(DTC_Link_1, form.value);
// 	form.vals.push_back(o.str());
// 	return form;
// }
// DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatROCEmulatorInterpacketDelayLink2()
// {
// 	auto form = CreateFormatter(DTC_Register_ROCEmulator_InterpacketDelay_Link2);
// 	form.description = "ROC Emulator Interpacket Delay Link 2 (*5ns)";
// 	std::stringstream o;
// 	o << std::dec << ReadROCEmulatorInterpacketDelay(DTC_Link_2, form.value);
// 	form.vals.push_back(o.str());
// 	return form;
// }
// DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatROCEmulatorInterpacketDelayLink3()
// {
// 	auto form = CreateFormatter(DTC_Register_ROCEmulator_InterpacketDelay_Link3);
// 	form.description = "ROC Emulator Interpacket Delay Link 3 (*5ns)";
// 	std::stringstream o;
// 	o << std::dec << ReadROCEmulatorInterpacketDelay(DTC_Link_3, form.value);
// 	form.vals.push_back(o.str());
// 	return form;
// }
// DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatROCEmulatorInterpacketDelayLink4()
// {
// 	auto form = CreateFormatter(DTC_Register_ROCEmulator_InterpacketDelay_Link4);
// 	form.description = "ROC Emulator Interpacket Delay Link 4 (*5ns)";
// 	std::stringstream o;
// 	o << std::dec << ReadROCEmulatorInterpacketDelay(DTC_Link_4, form.value);
// 	form.vals.push_back(o.str());
// 	return form;
// }
// DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatROCEmulatorInterpacketDelayLink5()
// {
// 	auto form = CreateFormatter(DTC_Register_ROCEmulator_InterpacketDelay_Link5);
// 	form.description = "ROC Emulator Interpacket Delay Link 5 (*5ns)";
// 	std::stringstream o;
// 	o << std::dec << ReadROCEmulatorInterpacketDelay(DTC_Link_5, form.value);
// 	form.vals.push_back(o.str());
// 	return form;
// }

// TX Data Request Packet Count
uint32_t DTCLib::DTC_Registers::ReadTXEventWindowMarkerCount(DTC_Link_ID const& link, std::optional<uint32_t> val)
{
	return val.has_value() ? *val : ReadRegister_(GetTXEventWindowMarkerCountLinkRegister(link));
}  // end ReadTXEventWindowMarkerCount()

DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatTXEventWindowMarkerCountLink(DTC_Link_ID const& link)
{
	auto form = CreateFormatter(GetTXEventWindowMarkerCountLinkRegister(link));
	if (link == DTC_Link_CFO)
		form.description = "CFO Event Window Marker (EWM) Count";
	else
		form.description = "EWM TX Count on Link " +
						   std::to_string((GetTXEventWindowMarkerCountLinkRegister(link) -
										   GetTXEventWindowMarkerCountLinkRegister(DTC_Link_0)) /
										  4);
	std::stringstream o;
	o << std::dec << ReadTXEventWindowMarkerCount(link, form.value);
	form.vals.push_back(o.str());
	return form;
}  // end FormatTXEventWindowMarkerCountLink()

DTCLib::DTC_Register DTCLib::DTC_Registers::GetTXEventWindowMarkerCountLinkRegister(DTC_Link_ID const& link)
{
	DTC_Register reg;
	switch (link)
	{
		case DTC_Link_0:
			reg = DTC_Register_TXEventWindowMarkerCount_Link0;
			break;
		case DTC_Link_1:
			reg = DTC_Register_TXEventWindowMarkerCount_Link1;
			break;
		case DTC_Link_2:
			reg = DTC_Register_TXEventWindowMarkerCount_Link2;
			break;
		case DTC_Link_3:
			reg = DTC_Register_TXEventWindowMarkerCount_Link3;
			break;
		case DTC_Link_4:
			reg = DTC_Register_TXEventWindowMarkerCount_Link4;
			break;
		case DTC_Link_5:
			reg = DTC_Register_TXEventWindowMarkerCount_Link5;
			break;
		case DTC_Link_CFO:
			reg = DTC_Register_CFOTXEventWindowMarkerCount_Link6;
			break;
		default: {
			__SS__ << "Illegal link index provided: " << link << __E__;
			ss << "\n\nThe stack trace is as follows:\n"
			   << otsStyleStackTrace() << __E__;
			__SS_THROW__;
		}
	}
	return reg;
}  // end GetTXEventWindowMarkerCountLinkRegister()

// TX Data Request Packet Count
uint32_t DTCLib::DTC_Registers::ReadTXDataRequestPacketCount(DTC_Link_ID const& link, std::optional<uint32_t> val)
{
	return val.has_value() ? *val : ReadRegister_(GetTXDataRequestPacketCountLinkRegister(link));
}  // end ReadTXDataRequestPacketCount()

DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatTXDataRequestPacketCountLink(DTC_Link_ID const& link)
{
	auto form = CreateFormatter(GetTXDataRequestPacketCountLinkRegister(link));
	form.description = "DRP TX Count on Link " +
					   std::to_string((GetTXDataRequestPacketCountLinkRegister(link) -
									   GetTXDataRequestPacketCountLinkRegister(DTC_Link_0)) /
									  4);
	std::stringstream o;
	o << std::dec << ReadTXDataRequestPacketCount(link, form.value);
	form.vals.push_back(o.str());
	return form;
}  // end FormatTXEventWindowMarkerCountLink()

DTCLib::DTC_Register DTCLib::DTC_Registers::GetTXDataRequestPacketCountLinkRegister(DTC_Link_ID const& link)
{
	DTC_Register reg;
	switch (link)
	{
		case DTC_Link_0:
			reg = DTC_Register_TXDataRequestPacketCount_Link0;
			break;
		case DTC_Link_1:
			reg = DTC_Register_TXDataRequestPacketCount_Link1;
			break;
		case DTC_Link_2:
			reg = DTC_Register_TXDataRequestPacketCount_Link2;
			break;
		case DTC_Link_3:
			reg = DTC_Register_TXDataRequestPacketCount_Link3;
			break;
		case DTC_Link_4:
			reg = DTC_Register_TXDataRequestPacketCount_Link4;
			break;
		case DTC_Link_5:
			reg = DTC_Register_TXDataRequestPacketCount_Link5;
			break;
		default: {
			__SS__ << "Illegal link index provided: " << link << __E__;
			ss << "\n\nThe stack trace is as follows:\n"
			   << otsStyleStackTrace() << __E__;
			__SS_THROW__;
		}
	}
	return reg;
}  // end GetTXDataRequestPacketCountLinkRegister()

DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatCFOTXClockMarkerCountLink6()
{
	auto form = CreateFormatter(DTC_Register_CFOTXClockMarkerCount_Link6);
	form.description = "CFO Clock Marker Count";
	std::stringstream o;
	o << std::dec << ReadCFOTXClockMarkerCountLink6(form.value);
	form.vals.push_back(o.str());
	return form;
}  // end FormatCFOTXClockMarkerCountLink6()

uint32_t DTCLib::DTC_Registers::ReadCFOTXClockMarkerCountLink6(std::optional<uint32_t> val)
{
	return val.has_value() ? *val : ReadRegister_(DTC_Register_CFOTXClockMarkerCount_Link6);
}  // end ReadCFOTXClockMarkerCountLink6()

// TX Heartbeat Packet Count
uint32_t DTCLib::DTC_Registers::ReadTXHeartbeatPacketCount(DTC_Link_ID const& link, std::optional<uint32_t> val)
{
	return val.has_value() ? *val : ReadRegister_(GetTXHeartbeatPacketCountLinkRegister(link));
}  // end ReadTXHeartbeatPacketCount()
DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatTXHeartbeatPacketCountLink(DTC_Link_ID const& link)
{
	auto form = CreateFormatter(GetTXHeartbeatPacketCountLinkRegister(link));
	if (link == DTC_Link_CFO)
		form.description = "CFO Heartbeat Packet (HBP) Count";
	else
		form.description = "HBP TX Counter Link " +
						   std::to_string((GetTXHeartbeatPacketCountLinkRegister(link) -
										   GetTXHeartbeatPacketCountLinkRegister(DTC_Link_0)) /
										  4);
	std::stringstream o;
	o << std::dec << ReadTXHeartbeatPacketCount(DTC_Link_0, form.value);
	form.vals.push_back(o.str());
	return form;
}  // end FormatTXHeartbeatPacketCountLink()

DTCLib::DTC_Register DTCLib::DTC_Registers::GetTXHeartbeatPacketCountLinkRegister(DTC_Link_ID const& link)
{
	DTC_Register reg;
	switch (link)
	{
		case DTC_Link_0:
			reg = DTC_Register_TXHeartbeatPacketCount_Link0;
			break;
		case DTC_Link_1:
			reg = DTC_Register_TXHeartbeatPacketCount_Link1;
			break;
		case DTC_Link_2:
			reg = DTC_Register_TXHeartbeatPacketCount_Link2;
			break;
		case DTC_Link_3:
			reg = DTC_Register_TXHeartbeatPacketCount_Link3;
			break;
		case DTC_Link_4:
			reg = DTC_Register_TXHeartbeatPacketCount_Link4;
			break;
		case DTC_Link_5:
			reg = DTC_Register_TXHeartbeatPacketCount_Link5;
			break;
		case DTC_Link_CFO:
			reg = DTC_Register_CFOTXHeartbeatPacketCount_Link5;
			break;
		default: {
			__SS__ << "Illegal link index provided: " << link << __E__;
			__SS_THROW__;
		}
	}
	return reg;
}  // end GetTXHeartbeatPacketCountLinkRegister()

// RX Data Header Packet Count
uint32_t DTCLib::DTC_Registers::ReadRXDataHeaderPacketCount(DTC_Link_ID const& link, std::optional<uint32_t> val)
{
	return val.has_value() ? *val : ReadRegister_(GetRXDataHeaderPacketCountLinkRegister(link));
}  // end ReadRXDataHeaderPacketCount()

DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatRXDataHeaderPacketCountLink(DTC_Link_ID const& link)
{
	auto form = CreateFormatter(GetRXDataHeaderPacketCountLinkRegister(link));
	form.description = "ROC DH RX Count on Link " +
					   std::to_string((GetRXDataHeaderPacketCountLinkRegister(link) -
									   GetRXDataHeaderPacketCountLinkRegister(DTC_Link_0)) /
									  4);
	std::stringstream o;
	o << std::dec << ReadRXDataHeaderPacketCount(link, form.value);
	form.vals.push_back(o.str());
	return form;
}  // end FormatRXDataHeaderPacketCountLink()

DTCLib::DTC_Register DTCLib::DTC_Registers::GetRXDataHeaderPacketCountLinkRegister(DTC_Link_ID const& link)
{
	DTC_Register reg;
	switch (link)
	{
		case DTC_Link_0:
			reg = DTC_Register_RXDataHeaderPacketCount_Link0;
			break;
		case DTC_Link_1:
			reg = DTC_Register_RXDataHeaderPacketCount_Link1;
			break;
		case DTC_Link_2:
			reg = DTC_Register_RXDataHeaderPacketCount_Link2;
			break;
		case DTC_Link_3:
			reg = DTC_Register_RXDataHeaderPacketCount_Link3;
			break;
		case DTC_Link_4:
			reg = DTC_Register_RXDataHeaderPacketCount_Link4;
			break;
		case DTC_Link_5:
			reg = DTC_Register_RXDataHeaderPacketCount_Link5;
			break;
		default: {
			__SS__ << "Illegal link index provided: " << link << __E__;
			__SS_THROW__;
		}
	}
	return reg;
}  // end GetRXDataHeaderPacketCountLinkRegister()

// RX Data Packet Count
uint32_t DTCLib::DTC_Registers::ReadRXDataPacketCount(DTC_Link_ID const& link, std::optional<uint32_t> val)
{
	return val.has_value() ? *val : ReadRegister_(GetRXDataPacketCountLinkRegister(link));
}  // end ReadRXDataPacketCount()

DTCLib::RegisterFormatter DTCLib::DTC_Registers::FormatRXDataPacketCountLink(DTC_Link_ID const& link)
{
	auto form = CreateFormatter(GetRXDataPacketCountLinkRegister(link));
	form.description = "ROC DP RX Count on Link " +
					   std::to_string((GetRXDataPacketCountLinkRegister(link) -
									   GetRXDataPacketCountLinkRegister(DTC_Link_0)) /
									  4);
	std::stringstream o;
	o << std::dec << ReadRXDataHeaderPacketCount(link, form.value);
	form.vals.push_back(o.str());
	return form;
}  // end FormatRXDataPacketCountLink()

DTCLib::DTC_Register DTCLib::DTC_Registers::GetRXDataPacketCountLinkRegister(DTC_Link_ID const& link)
{
	DTC_Register reg;
	switch (link)
	{
		case DTC_Link_0:
			reg = DTC_Register_RXDataPacketCount_Link0;
			break;
		case DTC_Link_1:
			reg = DTC_Register_RXDataPacketCount_Link1;
			break;
		case DTC_Link_2:
			reg = DTC_Register_RXDataPacketCount_Link2;
			break;
		case DTC_Link_3:
			reg = DTC_Register_RXDataPacketCount_Link3;
			break;
		case DTC_Link_4:
			reg = DTC_Register_RXDataPacketCount_Link4;
			break;
		case DTC_Link_5:
			reg = DTC_Register_RXDataPacketCount_Link5;
			break;
		default: {
			__SS__ << "Illegal link index provided: " << link << __E__;
			__SS_THROW__;
		}
	}
	return reg;
}  // end GetRXDataPacketCountLinkRegister()

// EVB Diagnostic RX Packet FIFO
uint64_t DTCLib::DTC_Registers::ReadEVBDiagnosticFIFO(std::optional<uint32_t> val)
{
	uint64_t ret = val.has_value() ? *val : ReadRegister_(DTC_Register_EVBDiagnosticRXPacket_High);
	ret = (ret << 32) + ReadRegister_(DTC_Register_EVBDiagnosticRXPacket_Low);
	return ret;
}
void DTCLib::DTC_Registers::ClearEVBDiagnosticFIFO()
{
	WriteRegister_(0, DTC_Register_EVBDiagnosticRXPacket_Low);
}

// Event Mode Lookup Table
/// <summary>
/// Set all event mode words to the given value
/// </summary>
/// <param name="data">Value for all event mode words</param>
void DTCLib::DTC_Registers::SetAllEventModeWords(uint32_t data)
{
	for (uint16_t address = DTC_Register_EventModeLookupTableStart; address <= DTC_Register_EventModeLookupTableEnd;
		 address += 4)
	{
		auto retry = 3;
		int errorCode;
		do
		{
			errorCode = device_.write_register(address, 100, data);
			--retry;
		} while (retry > 0 && errorCode != 0);
		if (errorCode != 0)
		{
			__SS__ << "Error writing register " << address;
			__SS_THROW__;
			// throw DTC_IOErrorException(errorCode);
		}
	}
}

/// <summary>
/// Set a given event mode word
/// </summary>
/// <param name="which">Word index to write</param>
/// <param name="data">Data for word</param>
void DTCLib::DTC_Registers::SetEventModeWord(uint8_t which, uint32_t data)
{
	uint16_t address = DTC_Register_EventModeLookupTableStart + (which * 4);
	if (address <= DTC_Register_EventModeLookupTableEnd)
	{
		auto retry = 3;
		int errorCode;
		do
		{
			errorCode = device_.write_register(address, 100, data);
			--retry;
		} while (retry > 0 && errorCode != 0);
		if (errorCode != 0)
		{
			__SS__ << "Error writing register " << address;
			__SS_THROW__;
			// throw DTC_IOErrorException(errorCode);
		}
	}
}

/// <summary>
/// Read an event mode word from the Event Mode lookup table
/// </summary>
/// <param name="which">Word index to read</param>
/// <returns>Value of the given event mode word</returns>
uint32_t DTCLib::DTC_Registers::ReadEventModeWord(uint8_t which)
{
	uint16_t address = DTC_Register_EventModeLookupTableStart + (which * 4);
	if (address <= DTC_Register_EventModeLookupTableEnd)
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
			__SS__ << "Error writing register " << address;
			__SS_THROW__;
			// throw DTC_IOErrorException(errorCode);
		}

		return data;
	}
	return 0;
}

// Oscillator Programming (DDR and SERDES)
/// <summary>
/// Set the given oscillator to the given frequency, calculating a new program in the process.
/// </summary>
/// <param name="oscillator">Oscillator to program, either DDR or SERDES</param>
/// <param name="targetFrequency">New frequency to program, in Hz</param>
/// <returns>Whether the oscillator was changed (Will not reset if already set to desired frequency)</returns>
bool DTCLib::DTC_Registers::SetNewOscillatorFrequency(DTC_OscillatorType oscillator, double targetFrequency)
{
	auto currentFrequency = 0;  // As of 2023 DTCs, can not read frequency back (it was doing nothing, just a scratch register) // ReadCurrentFrequency(oscillator);
	auto currentProgram = ReadCurrentProgram(oscillator);
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
	WriteCurrentProgram(newParameters, oscillator);
	// WriteCurrentFrequency(targetFrequency, oscillator); //As of 2023 DTCs, can not write frequency back (it was doing nothing, just a scratch register)
	return true;
}

/// <summary>
/// Read the current RFREQ and dividers of the given oscillator clock
/// </summary>
/// <param name="oscillator">Oscillator to program, either DDR or SERDES</param>
/// <returns>64-bit integer contianing current oscillator program</returns>
uint64_t DTCLib::DTC_Registers::ReadCurrentProgram(DTC_OscillatorType oscillator)
{
	switch (oscillator)
	{
		case DTC_OscillatorType_SERDES:
			return ReadSERDESOscillatorParameters_();
		case DTC_OscillatorType_Timing:
			return ReadTimingOscillatorParameters_();
		// case DTC_OscillatorType_DDR:
		// 	return ReadDDROscillatorParameters_();
		default:
			__SS__ << "Invalid DTC_OscillatorType to read: " + std::to_string(oscillator);
			__SS_THROW__;
	}
	return 0;
}

/// <summary>
/// Writes a program for the given oscillator crystal. This function should be paired with a call to
/// WriteCurrentFrequency so that subsequent programming attempts work as expected.
/// </summary>
/// <param name="program">64-bit integer with new RFREQ and dividers</param>
/// <param name="oscillator">Oscillator to program, either DDR or SERDES</param>
void DTCLib::DTC_Registers::WriteCurrentProgram(uint64_t program, DTC_OscillatorType oscillator)
{
	switch (oscillator)
	{
		case DTC_OscillatorType_SERDES:
			SetSERDESOscillatorParameters_(program);
			break;
		case DTC_OscillatorType_Timing:
			SetTimingOscillatorParameters_(program);
			break;
		// case DTC_OscillatorType_DDR:
		// 	SetDDROscillatorParameters_(program);
		// 	break;
		default:
			__SS__ << "Invalid DTC_OscillatorType to program: " + std::to_string(oscillator);
			__SS_THROW__;
	}
}

// Private Functions
void DTCLib::DTC_Registers::VerifyRegisterWrite_(const CFOandDTC_Register& address, uint32_t readbackValue, uint32_t dataToWrite)
{
	// verify register readback
	if (1)
	{
		// uint32_t readbackReturnValue = ReadRegister_(address);
		// uint32_t readbackValue = readbackReturnValue;
		// uint32_t readbackValue = ReadRegister_(address);

		switch (address)  // handle special register checks by masking of DONT-CARE bits, or else check full 32 bits
		{
			//---------- DTC only registers
			case DTC_Register_CFOLinkErrorFlags:  // CFO Sample Permanent Offset 2:0
				dataToWrite &= 0x03;
				readbackValue &= 0x03;
				break;
			case DTC_Register_CFOMarkerEnables:  // CFO emulator marker enables: 5:0 enables clock marker, 13:8 is event
												 // marker per ROC link for some reason, now event marker is not returned
												 // (FIXME?)
				dataToWrite &= 0x03f;
				readbackValue &= 0x03f;
				break;
			case DTC_Register_RXCDRUnlockCount_CFOLink:  // write clears 32-bit CDR unlock counter, but can read back errors
														 // immediately, so don't check
			case DTC_Register_JitterAttenuatorLossOfLockCount:
				return;
			case DTC_Register_JitterAttenuatorCSR:  // 0x9308 bit-0 is reset, input select bit-5:4, bit-8 is LOL, bit-11:9
													// (input LOS).. only check input select bits
				dataToWrite &= (3 << 4);
				readbackValue &= (3 << 4);
				break;
			case DTC_Register_CFOEmulation_EventMode2:  // only lower 16-bits are R/W
				dataToWrite &= 0x0000ffff;
				readbackValue &= 0x0000ffff;
				break;
			case DTC_Register_DetEmulation_Control1:  // self clearing bit-1, so return immediately
				return;
			case DTC_Register_EVBStats:  // addrs for write, data on read
				return;

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
		// return readbackReturnValue;
	}  // end verify register readback
}  // end VerifyRegisterWrite_()

int DTCLib::DTC_Registers::DecodeHighSpeedDivider_(int input)
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

int DTCLib::DTC_Registers::EncodeHighSpeedDivider_(int input)
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

int DTCLib::DTC_Registers::EncodeOutputDivider_(int input)
{
	if (input == 1) return 0;
	int temp = input / 2;
	return (temp * 2) - 1;
}

uint64_t DTCLib::DTC_Registers::CalculateFrequencyForProgramming_(double targetFrequency, double currentFrequency,
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

uint64_t DTCLib::DTC_Registers::ReadSERDESOscillatorParameters_()
{
	uint64_t data = (static_cast<uint64_t>(ReadSERDESIICInterface(DTC_IICSERDESBusAddress_EVB, 7)) << 40) +
					(static_cast<uint64_t>(ReadSERDESIICInterface(DTC_IICSERDESBusAddress_EVB, 8)) << 32) +
					(static_cast<uint64_t>(ReadSERDESIICInterface(DTC_IICSERDESBusAddress_EVB, 9)) << 24) +
					(static_cast<uint64_t>(ReadSERDESIICInterface(DTC_IICSERDESBusAddress_EVB, 10)) << 16) +
					(static_cast<uint64_t>(ReadSERDESIICInterface(DTC_IICSERDESBusAddress_EVB, 11)) << 8) +
					static_cast<uint64_t>(ReadSERDESIICInterface(DTC_IICSERDESBusAddress_EVB, 12));
	return data;
}

uint64_t DTCLib::DTC_Registers::ReadTimingOscillatorParameters_()
{
	uint64_t data = (static_cast<uint64_t>(ReadSERDESIICInterface(DTC_IICSERDESBusAddress_CFO, 7)) << 40) +
					(static_cast<uint64_t>(ReadSERDESIICInterface(DTC_IICSERDESBusAddress_CFO, 8)) << 32) +
					(static_cast<uint64_t>(ReadSERDESIICInterface(DTC_IICSERDESBusAddress_CFO, 9)) << 24) +
					(static_cast<uint64_t>(ReadSERDESIICInterface(DTC_IICSERDESBusAddress_CFO, 10)) << 16) +
					(static_cast<uint64_t>(ReadSERDESIICInterface(DTC_IICSERDESBusAddress_CFO, 11)) << 8) +
					static_cast<uint64_t>(ReadSERDESIICInterface(DTC_IICSERDESBusAddress_CFO, 12));
	return data;
}

// uint64_t DTCLib::DTC_Registers::ReadDDROscillatorParameters_()
// {
// 	uint64_t data = (static_cast<uint64_t>(ReadDDRIICInterface(DTC_IICDDRBusAddress_DDROscillator, 7)) << 40) +
// 					(static_cast<uint64_t>(ReadDDRIICInterface(DTC_IICDDRBusAddress_DDROscillator, 8)) << 32) +
// 					(static_cast<uint64_t>(ReadDDRIICInterface(DTC_IICDDRBusAddress_DDROscillator, 9)) << 24) +
// 					(static_cast<uint64_t>(ReadDDRIICInterface(DTC_IICDDRBusAddress_DDROscillator, 10)) << 16) +
// 					(static_cast<uint64_t>(ReadDDRIICInterface(DTC_IICDDRBusAddress_DDROscillator, 11)) << 8) +
// 					static_cast<uint64_t>(ReadDDRIICInterface(DTC_IICDDRBusAddress_DDROscillator, 12));
// 	return data;
// }

void DTCLib::DTC_Registers::SetSERDESOscillatorParameters_(uint64_t program)
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

void DTCLib::DTC_Registers::SetTimingOscillatorParameters_(uint64_t program)
{
	WriteSERDESIICInterface(DTC_IICSERDESBusAddress_CFO, 0x89, 0x10);

	WriteSERDESIICInterface(DTC_IICSERDESBusAddress_CFO, 7, static_cast<uint8_t>(program >> 40));
	WriteSERDESIICInterface(DTC_IICSERDESBusAddress_CFO, 8, static_cast<uint8_t>(program >> 32));
	WriteSERDESIICInterface(DTC_IICSERDESBusAddress_CFO, 9, static_cast<uint8_t>(program >> 24));
	WriteSERDESIICInterface(DTC_IICSERDESBusAddress_CFO, 10, static_cast<uint8_t>(program >> 16));
	WriteSERDESIICInterface(DTC_IICSERDESBusAddress_CFO, 11, static_cast<uint8_t>(program >> 8));
	WriteSERDESIICInterface(DTC_IICSERDESBusAddress_CFO, 12, static_cast<uint8_t>(program));

	WriteSERDESIICInterface(DTC_IICSERDESBusAddress_CFO, 0x89, 0);
	WriteSERDESIICInterface(DTC_IICSERDESBusAddress_CFO, 0x87, 0x40);
}

// void DTCLib::DTC_Registers::SetDDROscillatorParameters_(uint64_t program)
// {
// 	WriteDDRIICInterface(DTC_IICDDRBusAddress_DDROscillator, 0x89, 0x10);

// 	WriteDDRIICInterface(DTC_IICDDRBusAddress_DDROscillator, 7, static_cast<uint8_t>(program >> 40));
// 	WriteDDRIICInterface(DTC_IICDDRBusAddress_DDROscillator, 8, static_cast<uint8_t>(program >> 32));
// 	WriteDDRIICInterface(DTC_IICDDRBusAddress_DDROscillator, 9, static_cast<uint8_t>(program >> 24));
// 	WriteDDRIICInterface(DTC_IICDDRBusAddress_DDROscillator, 10, static_cast<uint8_t>(program >> 16));
// 	WriteDDRIICInterface(DTC_IICDDRBusAddress_DDROscillator, 11, static_cast<uint8_t>(program >> 8));
// 	WriteDDRIICInterface(DTC_IICDDRBusAddress_DDROscillator, 12, static_cast<uint8_t>(program));

// 	WriteDDRIICInterface(DTC_IICDDRBusAddress_DDROscillator, 0x89, 0);
// 	WriteDDRIICInterface(DTC_IICDDRBusAddress_DDROscillator, 0x87, 0x40);
// }

bool DTCLib::DTC_Registers::WaitForLinkReady_(DTC_Link_ID const& link, size_t interval, double timeout /*seconds*/)
{
	auto start = std::chrono::steady_clock::now();
	auto last_print = start;
	bool ready = ReadSERDESPLLLocked(link) && ReadResetRXSERDESDone(link) && ReadResetTXSERDESDone(link) && ReadSERDESRXCDRLock(link);

	while (!ready)
	{
		usleep(interval);
		ready = ReadSERDESPLLLocked(link) && ReadResetRXSERDESDone(link) && ReadResetTXSERDESDone(link) && ReadSERDESRXCDRLock(link);
		if (std::chrono::duration_cast<std::chrono::duration<double, std::ratio<1>>>(std::chrono::steady_clock::now() - last_print).count() > 5.0)
		{
			__COUT__ << "DTC_Registers: WaitForLinkReady_: ROC Link " << link << ": PLL Locked: " << std::boolalpha << ReadSERDESPLLLocked(link) << ", RX Reset Done: " << ReadResetRXSERDESDone(link) << ", TX Reset Done: " << ReadResetTXSERDESDone(link) << ", CDR Lock: " << ReadSERDESRXCDRLock(link);
			last_print = std::chrono::steady_clock::now();
		}
		if (std::chrono::duration_cast<std::chrono::duration<double, std::ratio<1>>>(std::chrono::steady_clock::now() - start).count() > timeout)
		{
			__COUT_ERR__ << "DTC_Registers: WaitForLinkReady_ ABORTING: ROC Link " << link << ": PLL Locked: " << std::boolalpha << ReadSERDESPLLLocked(link) << ", RX Reset Done: " << ReadResetRXSERDESDone(link) << ", TX Reset Done: " << ReadResetTXSERDESDone(link) << ", CDR Lock: " << ReadSERDESRXCDRLock(link);
			return false;
		}
	}
	return true;
}
