#ifndef CFO_REGISTERS_H
#define CFO_REGISTERS_H

#include <functional>  // std::bind, std::function
#include <vector>      // std::vector

#include "dtcInterfaceLib/CFOandDTC_Registers.h"

using namespace DTCLib;

namespace CFOLib {

/// <summary>
/// Register address map
/// </summary>
enum CFO_Register : uint16_t
{
	DTCLIB_COMMON_REGISTERS,  // Moved here all registers in common with DTC

	CFO_Register_SFPSERDESStatus = 0x9140,
	CFO_Register_BeamOnTimerPreset = 0x9144,
	CFO_Register_EnableBeamOnMode = 0x9148,
	CFO_Register_EnableBeamOffMode = 0x914C,
	CFO_Register_ClockMarkerIntervalCount = 0x9154,
	CFO_Register_SERDESOscillatorFrequency = 0x9160,
	CFO_Register_SERDESClock_IICBusControl = 0x9164,
	CFO_Register_TimestampPreset0 = 0x9180,
	CFO_Register_TimestampPreset1 = 0x9184,
	CFO_Register_NUMDTCs = 0x918C,
	CFO_Register_FIFOFullErrorFlag0 = 0x9190,
	CFO_Register_ReceivePacketError = 0x919C,
	// CFO_Register_EventWindowEmulatorIntervalTime = 0x91A0, //register deleted in Firmware version: Nov/09/2023 11:00   raw-data: 0x23110911
	CFO_Register_EventWindowHoldoffTime = 0x91A4,
	CFO_Register_EventWindowTimeoutError = 0x91A8,
	CFO_Register_EventWindowTimeoutValue = 0x91AC,
	CFO_Register_ReceiveByteCountDataLink0 = 0x9200,
	CFO_Register_ReceiveByteCountDataLink1 = 0x9204,
	CFO_Register_ReceiveByteCountDataLink2 = 0x9208,
	CFO_Register_ReceiveByteCountDataLink3 = 0x920C,
	CFO_Register_ReceiveByteCountDataLink4 = 0x9210,
	CFO_Register_ReceiveByteCountDataLink5 = 0x9214,
	CFO_Register_ReceiveByteCountDataLink6 = 0x9218,
	CFO_Register_ReceiveByteCountDataLink7 = 0x921C,
	CFO_Register_ReceivePacketCountDataLink0 = 0x9220,
	CFO_Register_ReceivePacketCountDataLink1 = 0x9224,
	CFO_Register_ReceivePacketCountDataLink2 = 0x9228,
	CFO_Register_ReceivePacketCountDataLink3 = 0x922C,
	CFO_Register_ReceivePacketCountDataLink4 = 0x9230,
	CFO_Register_ReceivePacketCountDataLink5 = 0x9234,
	CFO_Register_ReceivePacketCountDataLink6 = 0x9238,
	CFO_Register_ReceivePacketCountDataLink7 = 0x923C,
	CFO_Register_TransmitByteCountDataLink0 = 0x9240,
	CFO_Register_TransmitByteCountDataLink1 = 0x9244,
	CFO_Register_TransmitByteCountDataLink2 = 0x9248,
	CFO_Register_TransmitByteCountDataLink3 = 0x924C,
	CFO_Register_TransmitByteCountDataLink4 = 0x9250,
	CFO_Register_TransmitByteCountDataLink5 = 0x9254,
	CFO_Register_TransmitByteCountDataLink6 = 0x9258,
	CFO_Register_TransmitByteCountDataLink7 = 0x925C,
	CFO_Register_TransmitPacketCountDataLink0 = 0x9260,
	CFO_Register_TransmitPacketCountDataLink1 = 0x9264,
	CFO_Register_TransmitPacketCountDataLink2 = 0x9268,
	CFO_Register_TransmitPacketCountDataLink3 = 0x926C,
	CFO_Register_TransmitPacketCountDataLink4 = 0x9270,
	CFO_Register_TransmitPacketCountDataLink5 = 0x9274,
	CFO_Register_TransmitPacketCountDataLink6 = 0x9278,
	CFO_Register_TransmitPacketCountDataLink7 = 0x927C,
	CFO_Register_DDRMemoryDMAWriteStartAddress = 0x9300,
	CFO_Register_DDRMemoryDMAReadStartAddress = 0x9304,
	CFO_Register_DDRMemoryDMAReadByteCount = 0x9308,
	CFO_Register_RunPlanBeamOnBaseAddress = 0x930C,
	CFO_Register_RunPlanBeamOffBaseAddress = 0x9310,

	CFO_Register_RunPlan_Address = 0x9314,
	CFO_Register_RunPlan_Data = 0x9318,
	CFO_Register_RunPlan_EventMode0 = 0x931C,
	CFO_Register_RunPlan_EventMode1 = 0x9320,
	CFO_Register_RunPlan_EventTag0 = 0x9324,
	CFO_Register_RunPlan_EventTag1 = 0x9328,

	CFO_Register_SERDESPRBSControlLink0 = 0x9330,
	CFO_Register_SERDESPRBSControlLink1 = 0x9334,
	CFO_Register_SERDESPRBSControlLink2 = 0x9338,
	CFO_Register_SERDESPRBSControlLink3 = 0x933C,
	CFO_Register_SERDESPRBSControlLink4 = 0x9340,
	CFO_Register_SERDESPRBSControlLink5 = 0x9344,
	CFO_Register_SERDESPRBSControlLink6 = 0x9348,
	CFO_Register_SERDESPRBSControlLink7 = 0x934C,

	CFO_Register_CableDelayValue_offset = 0x9360,
	// CFO_Register_CableDelayValueROC1_offset = 0x9460,
	// CFO_Register_CableDelayValueROC2_offset = 0x9560,
	// CFO_Register_CableDelayValueROC3_offset = 0x9660,
	// CFO_Register_CableDelayValueROC4_offset = 0x9760,
	// CFO_Register_CableDelayValueROC5_offset = 0x9860,
	// CFO_Register_CableDelayValueLink0 = 0x9360,
	// CFO_Register_CableDelayValueLink1 = 0x9364,
	// CFO_Register_CableDelayValueLink2 = 0x9368,
	// CFO_Register_CableDelayValueLink3 = 0x936C,
	// CFO_Register_CableDelayValueLink4 = 0x9370,
	// CFO_Register_CableDelayValueLink5 = 0x9374,
	// CFO_Register_CableDelayValueLink6 = 0x9378,
	// CFO_Register_CableDelayValueLink7 = 0x937C,

	CFO_Register_CableDelayControlStatus = 0x9380,

	CFO_Register_FireflyCSRRegister = 0x93A0,

	CFO_Register_FPGAProgramData = 0x9400,
	CFO_Register_FPGAPROMProgramStatus = 0x9404,
	CFO_Register_FPGACoreAccess = 0x9408,
	CFO_Register_JitterAttenuatorCSR = 0x9500,
	CFO_Register_Invalid,
	// };
};  // end CFO_Register enum

/// <summary>
/// The links of the CFO
/// </summary>
enum CFO_Link_ID : uint8_t
{
	CFO_Link_0 = 0,
	CFO_Link_1 = 1,
	CFO_Link_2 = 2,
	CFO_Link_3 = 3,
	CFO_Link_4 = 4,
	CFO_Link_5 = 5,
	CFO_Link_6 = 6,
	CFO_Link_7 = 7,
	CFO_Link_Unused,
	CFO_Link_ALL = 255
};

/// <summary>
/// Vector of the links, for iterating
/// </summary>
static const std::vector<CFO_Link_ID> CFO_Links{CFO_Link_0, CFO_Link_1, CFO_Link_2, CFO_Link_3,
												CFO_Link_4, CFO_Link_5, CFO_Link_6, CFO_Link_7};

/// <summary>
/// The CFO_Registers class represents the CFO Register space, and all the methods necessary to read and write those
/// registers. Each register has, at the very least, a read method, a write method, and a RegisterFormatter method
/// which formats the register value in a human-readable way.
/// </summary>
class CFO_Registers : public DTCLib::CFOandDTC_Registers
{
public:
	/// <summary>
	/// Construct an instance of the CFO register map
	/// </summary>
	/// <param name="mode">Default: CFO_SimMode_Disabled; The simulation mode of the CFO</param>
	/// <param name="CFO">CFO/DTC card instance to use</param>
	/// <param name="skipInit">Default: false; Whether to skip initializing the CFO using the SimMode.
	/// Used to read state.</param> <param name="expectedDesignVersion">Expected CFO Firmware Design Version. If set, will
	/// throw an exception if the CFO firmware does not match (Default: "")</param>
	explicit CFO_Registers(DTC_SimMode mode, int cfo, std::string expectedDesignVersion = "",
						   bool skipInit = false, const std::string& uid = "");
	/// <summary>
	/// CFO_Registers destructor
	/// </summary>
	virtual ~CFO_Registers();

	//
	// CFO Sim Mode Virtual Register
	//
	/// <summary>
	/// Get the current CFO_SimMode of this CFO_Registers object
	/// </summary>
	/// <returns></returns>
	DTC_SimMode GetSimMode() const { return simMode_; }

	/// <summary>
	/// Initialize the CFO in the given SimMode.
	/// </summary>
	/// <param name="expectedDesignVersion">Expected CFO Firmware Design Version. If set, will throw an exception if the
	/// CFO firmware does not match</param> <param name="mode">Mode to set</param> <param name="CFO">CFO/DTC card instance
	/// to use</param> <param name="skipInit">Whether to skip initializing the CFO using the SimMode. Used to read
	/// state.</param> <returns></returns>
	DTC_SimMode SetSimMode(std::string expectedDesignVersion, DTC_SimMode mode, int CFO,
						   bool skipInit = false, const std::string& uid = "");

	virtual void ResetPCIe() override { throw std::runtime_error("CFO-TODO!"); };
	virtual void FlashLEDs() override { throw std::runtime_error("CFO-TODO!"); };

	// Design Status Register
	/// <summary>
	/// Determine if the DDR FIFO is empty
	/// </summary>
	/// <returns>True if the DDR FIFO Empty bit is set</returns>
	bool ReadDDRFIFOEmpty(std::optional<uint32_t> val = std::nullopt);
	/// <summary>
	/// Read the DDR Clock Calibration Done bit
	/// </summary>
	/// <returns>True if the DDR Clock Calibration Done bit is set</returns>
	bool ReadDDRClockCalibrationDone(std::optional<uint32_t> val = std::nullopt);
	/// <summary>
	/// Formats the register's current value for register dumps
	/// </summary>
	/// <returns>RegisterFormatter object containing register information</returns>
	RegisterFormatter FormatDesignStatus();

	// CFO Control Register
	// void ResetCFORunPlan();
	// bool ReadResetCFORunPlan(std::optional<uint32_t> val = std::nullopt);
	void EnableLED7();
	void DisableReadLED7();
	bool ReadLED7State(std::optional<uint32_t> val = std::nullopt);
	void EnableAcceleratorRF0();
	void DisableAcceleratorRF0();
	bool ReadAcceleratorRF0Enable(std::optional<uint32_t> val = std::nullopt);
	void EnableEmbeddedClockMarker();
	void DisableEmbeddedClockMarker();
	bool ReadEmbeddedClockMarkerEnable(std::optional<uint32_t> val = std::nullopt);
	RegisterFormatter FormatCFOControl();

	// DMA Transfer Length Register
	/// <summary>
	/// Set the DMA buffer size which will automatically trigger a DMA
	/// </summary>
	/// <param name="length">Size, in bytes of buffer that will trigger a DMA</param>
	void SetTriggerDMATransferLength(uint16_t length);
	/// <summary>
	/// Read the DMA buffer size which will automatically trigger a DMA
	/// </summary>
	/// <returns>The DMA buffer size which will automatically trigger a DMA, in bytes</returns>
	uint16_t ReadTriggerDMATransferLength(std::optional<uint32_t> val = std::nullopt);
	/// <summary>
	/// Set the minimum DMA transfer size. Absolute minimum is 64 bytes.
	/// Buffers smaller than this size will be padded to the minimum.
	/// </summary>
	/// <param name="length">Size, in bytes, of the minimum DMA transfer buffer</param>
	void SetMinDMATransferLength(uint16_t length);
	/// <summary>
	/// Read the minimum DMA transfer size.
	/// </summary>
	/// <returns>The minimum DMA size, in bytes</returns>
	uint16_t ReadMinDMATransferLength(std::optional<uint32_t> val = std::nullopt);
	/// <summary>
	/// Formats the register's current value for register dumps
	/// </summary>
	/// <returns>RegisterFormatter object containing register information</returns>
	RegisterFormatter FormatDMATransferLength();

	// SERDES Loopback Enable Register
	/// <summary>
	/// Set the SERDES Loopback mode for the given Link
	/// </summary>
	/// <param name="link">Link to set for</param>
	/// <param name="mode">CFO_SERDESLoopbackMode to set</param>
	void SetSERDESLoopbackMode(const CFO_Link_ID& link, const DTC_SERDESLoopbackMode& mode);
	/// <summary>
	/// Read the SERDES Loopback mode for the given Link
	/// </summary>
	/// <param name="link">Link to read</param>
	/// <returns>CFO_SERDESLoopbackMode of the Link</returns>
	DTC_SERDESLoopbackMode ReadSERDESLoopback(const CFO_Link_ID& link, std::optional<uint32_t> val = std::nullopt);
	/// <summary>
	/// Formats the register's current value for register dumps
	/// </summary>
	/// <returns>RegisterFormatter object containing register information</returns>
	RegisterFormatter FormatSERDESLoopbackEnable();

	// Clock Status Register
	/// <summary>
	/// Read the SERDES Oscillator IIC Error Bit
	/// </summary>
	/// <returns>True if the SERDES Oscillator IIC Error is set</returns>
	bool ReadSERDESOscillatorIICError(std::optional<uint32_t> val = std::nullopt);
	/// <summary>
	/// Read the SERDES Oscillator Initalization Complete flag
	/// </summary>
	/// <returns>Whether the SERDES Oscillator has completed initialization</returns>
	bool ReadSERDESOscillatorInitializationComplete(std::optional<uint32_t> val = std::nullopt);
	/// <summary>
	/// Wait for the SERDES Oscillator to initialize, up to max_wait seconds
	/// </summary>
	/// <param name="max_wait">Seconds to wait</param>
	/// <returns>Whether the SERDES OScillator Completed initialization in the timeout</returns>
	bool WaitForSERDESOscillatorInitializationComplete(double max_wait = 1.0);
	/// <summary>
	/// Read the Timing Clock PLL Locked bit
	/// </summary>
	/// <returns>The current value of the Timing Clock PLL Locked bit</returns>
	bool ReadTimingClockPLLLocked(std::optional<uint32_t> val = std::nullopt);
	/// <summary>
	/// Formats the register's current value for register dumps
	/// </summary>
	/// <returns>RegisterFormatter object containing register information</returns>
	RegisterFormatter FormatClockOscillatorStatus();

	// Link Enable Register
	/// <summary>
	/// Enable a SERDES Link
	/// </summary>
	/// <param name="link">Link to enable</param>
	/// <param name="mode">Link enable bits to set (Default: All)</param>
	/// <param name="dtcCount">Number of DTCs in the Link (Default: 0)</param>
	void EnableLink(const CFO_Link_ID& link, const DTC_LinkEnableMode& mode = DTC_LinkEnableMode(),
					const uint8_t& dtcCount = 0);

	/// <summary>
	/// Disable a SERDES Link
	/// The given mode bits will be UNSET
	/// </summary>
	/// <param name="link">Link to disable</param>
	/// <param name="mode">Link enable bits to unset (Default: All)</param>
	void DisableLink(const CFO_Link_ID& link, const DTC_LinkEnableMode& mode = DTC_LinkEnableMode());
	/// <summary>
	/// Read the Link Enable bits for a given SERDES Link
	/// </summary>
	/// <param name="link">Link to read</param>
	/// <returns>DTC_LinkEnableMode containing TX, RX, and CFO bits</returns>
	DTC_LinkEnableMode ReadLinkEnabled(const CFO_Link_ID& link, std::optional<uint32_t> val = std::nullopt);
	/// <summary>
	/// Formats the register's current value for register dumps
	/// </summary>
	/// <returns>RegisterFormatter object containing register information</returns>
	RegisterFormatter FormatLinkEnable();

	// SERDES Reset Register
	/// <summary>
	/// Reset the SERDES
	/// Will poll the Reset SERDES Done flag until the SERDES reset is complete
	/// </summary>
	/// <param name="link">Link to reset</param>
	/// <param name="interval">Pollint interval, in microseconds</param>
	void ResetSERDES(const CFO_Link_ID& link, int interval = 100000);
	/// <summary>
	/// Reset all SERDES PLLs
	/// </summary>
	void ResetAllSERDESPlls();
	/// <summary>
	/// Reset all SERDES Tx
	/// </summary>
	void ResetAllSERDESTx();
	/// <summary>
	/// Read if a SERDES reset is currently in progress
	/// </summary>
	/// <param name="link">Link to read</param>
	/// <returns>True if a SERDES reset is in progress</returns>
	bool ReadResetSERDES(const CFO_Link_ID& link, std::optional<uint32_t> val = std::nullopt);
	/// <summary>
	/// Formats the register's current value for register dumps
	/// </summary>
	/// <returns>RegisterFormatter object containing register information</returns>
	RegisterFormatter FormatSERDESReset();

	// SERDES RX Disparity Error Register
	/// <summary>
	/// Read the SERDES RX Dispatity Error bits
	/// </summary>
	/// <param name="link">Link to read</param>
	/// <returns>CFO_SERDESRXDisparityError object with error bits</returns>
	DTC_SERDESRXDisparityError ReadSERDESRXDisparityError(const CFO_Link_ID& link, std::optional<uint32_t> val = std::nullopt);
	/// <summary>
	/// Formats the register's current value for register dumps
	/// </summary>
	/// <returns>RegisterFormatter object containing register information</returns>
	RegisterFormatter FormatSERDESRXDisparityError();

	// SERDES Character Not In Table Error Register
	/// <summary>
	/// Read the SERDES Character Not In Table Error bits
	/// </summary>
	/// <param name="link">Link to read</param>
	/// <returns>CFO_CharacterNotInTableError object with error bits</returns>
	DTC_CharacterNotInTableError ReadSERDESRXCharacterNotInTableError(const CFO_Link_ID& link, std::optional<uint32_t> val = std::nullopt);
	/// <summary>
	/// Formats the register's current value for register dumps
	/// </summary>
	/// <returns>RegisterFormatter object containing register information</returns>
	RegisterFormatter FormatSERDESRXCharacterNotInTableError();

	// SERDES Unlock Error Register
	/// <summary>
	/// Read whether the SERDES Unlock Error bit is set
	/// </summary>
	/// <param name="link">Link to check</param>
	/// <returns>True if the SERDES Unlock Error bit is set on the given Link</returns>
	bool ReadSERDESUnlockError(const CFO_Link_ID& link, std::optional<uint32_t> val = std::nullopt);
	/// <summary>
	/// Formats the register's current value for register dumps
	/// </summary>
	/// <returns>RegisterFormatter object containing register information</returns>
	RegisterFormatter FormatSERDESUnlockError();

	// SERDES PLL Locked Register
	/// <summary>
	/// Read if the SERDES PLL is locked for the given SERDES Link
	/// </summary>
	/// <param name="link">Link to read</param>
	/// <returns>True if the PLL is locked, false otherwise</returns>
	bool ReadSERDESPLLLocked(const CFO_Link_ID& link, std::optional<uint32_t> val = std::nullopt);
	/// <summary>
	/// Formats the register's current value for register dumps
	/// </summary>
	/// <returns>RegisterFormatter object containing register information</returns>
	RegisterFormatter FormatSERDESPLLLocked();

	// SERDES RX Status Register
	/// <summary>
	/// Read the SERDES RX Status for the given SERDES Link
	/// </summary>
	/// <param name="link">Link to read</param>
	/// <returns>CFO_RXStatus object</returns>
	DTC_RXStatus ReadSERDESRXStatus(const CFO_Link_ID& link, std::optional<uint32_t> val = std::nullopt);
	/// <summary>
	/// Formats the register's current value for register dumps
	/// </summary>
	/// <returns>RegisterFormatter object containing register information</returns>
	RegisterFormatter FormatSERDESRXStatus();

	// SERDES Reset Done Register
	/// <summary>
	/// Read if the SERDES reset is complete on the given SERDES Link
	/// </summary>
	/// <param name="link">Link to read</param>
	/// <returns>True if the SERDES Reset is done, false otherwise</returns>
	bool ReadResetSERDESDone(const CFO_Link_ID& link, std::optional<uint32_t> val = std::nullopt);
	/// <summary>
	/// Formats the register's current value for register dumps
	/// </summary>
	/// <returns>RegisterFormatter object containing register information</returns>
	RegisterFormatter FormatSERDESResetDone();

	// SERDES RX CDR Lock Register
	/// <summary>
	/// Read the SERDES CDR Lock bit for the given SERDES Link
	/// </summary>
	/// <param name="link">Link to read</param>
	/// <returns>True if the SERDES CDR Lock bit is set</returns>
	bool ReadSERDESRXCDRLock(const CFO_Link_ID& link, std::optional<uint32_t> val = std::nullopt);
	/// <summary>
	/// Formats the register's current value for register dumps
	/// </summary>
	/// <returns>RegisterFormatter object containing register information</returns>
	RegisterFormatter FormatSERDESRXCDRLock();

	// Beam On Timer Preset Regsiter
	/// <summary>
	/// Set the maximum time a DMA buffer may be active before it is sent, in 4ns ticks.
	/// The default value is 0x800
	/// </summary>
	/// <param name="preset">Maximum active time for DMA buffers</param>
	void SetBeamOnTimerPreset(uint32_t preset);
	/// <summary>
	/// Read the maximum time a DMA buffer may be active before it is sent, in 4ns ticks.
	/// The default value is 0x800
	/// </summary>
	/// <returns>Maximum active time for DMA buffers</returns>
	uint32_t ReadBeamOnTimerPreset(std::optional<uint32_t> val = std::nullopt);
	/// <summary>
	/// Formats the register's current value for register dumps
	/// </summary>
	/// <returns>RegisterFormatter object containing register information</returns>
	RegisterFormatter FormatBeamOnTimerPreset();

	// Enable Beam On Mode Register
	/// <summary>
	/// Set the Beam On Mode Enable bit for the given link
	/// </summary>
	/// <param name="link">Link to enable</param>
	void EnableBeamOnMode(const CFO_Link_ID& link);
	/// <summary>
	/// Clear the Beam On Mode Enable bit for the given link
	/// </summary>
	/// <param name="link">Link to disable</param>
	void DisableBeamOnMode(const CFO_Link_ID& link);
	/// <summary>
	/// Read the global Beam On Mode Enable bit
	/// </summary>
	/// <param name="val">Optional register value to use instead of reading from hardware</param>
	/// <returns>Value of the Beam On Mode Enable bit</returns>
	bool ReadBeamOnMode(std::optional<uint32_t> val = std::nullopt);
	/// <summary>
	/// Formats the register's current value for register dumps
	/// </summary>
	/// <returns>RegisterFormatter object containing register information</returns>
	RegisterFormatter FormatBeamOnMode();

	// Enable Beam Off Mode Register
	/// <summary>
	/// Set the Beam Off Mode Enable bit for the given link
	/// </summary>
	/// <param name="link">Link to enable</param>
	void EnableBeamOffMode(const CFO_Link_ID& link);
	/// <summary>
	/// Clear the Beam Off Mode Enable bit for the given link
	/// </summary>
	/// <param name="link">Link to disable</param>
	void DisableBeamOffMode(const CFO_Link_ID& link);
	/// <summary>
	/// Read the global Beam Off Mode Enable bit
	/// </summary>
	/// <returns>Value of the global Beam Off Mode Enable bit</returns>
	bool ReadBeamOffMode(std::optional<uint32_t> val = std::nullopt);
	/// <summary>
	/// Formats the register's current value for register dumps
	/// </summary>
	/// <returns>RegisterFormatter object containing register information</returns>
	RegisterFormatter FormatBeamOffMode();

	// 40 MHz Clock Marker Interval Count Register
	/// <summary>
	/// Set the Clock Marker Interval Count for synchronizing the 240 MHz and 40 MHz clocks
	/// </summary>
	/// <param name="data">Interval to set</param>
	void SetClockMarkerIntervalCount(uint32_t data);
	/// <summary>
	/// Read the Clock Marker Interval Count used for synchronizing the 240 MHz and 40 MHz clocks
	/// </summary>
	/// <returns>The Clock Marker Interval Count used for synchronizing the 240 MHz and 40 MHz clocks</returns>
	uint32_t ReadClockMarkerIntervalCount(std::optional<uint32_t> val = std::nullopt);
	/// <summary>
	/// Formats the register's current value for register dumps
	/// </summary>
	/// <returns>RegisterFormatter object containing register information</returns>
	RegisterFormatter FormatClockMarkerIntervalCount();

	// SERDES Oscillator Registers
	/// <summary>
	/// Read the current SERDES Oscillator frequency, in Hz
	/// </summary>
	/// <returns>Current SERDES Oscillator frequency, in Hz</returns>
	uint32_t ReadSERDESOscillatorFrequency(std::optional<uint32_t> val = std::nullopt);
	/// <summary>
	/// Set the SERDES Oscillator frequency
	/// </summary>
	/// <param name="freq">New frequency, in Hz</param>
	void SetSERDESOscillatorFrequency(uint32_t freq);
	/// <summary>
	/// Read the Reset bit of the SERDES IIC Bus
	/// </summary>
	/// <returns>Reset bit value</returns>
	bool ReadSERDESOscillatorIICInterfaceReset(std::optional<uint32_t> val = std::nullopt);
	/// <summary>
	/// Reset the SERDES IIC Bus
	/// </summary>
	void ResetSERDESOscillatorIICInterface();

	// Jitter Attenuator CSR Register
	virtual std::bitset<2> ReadJitterAttenuatorSelect(std::optional<uint32_t> val = std::nullopt) override;
	virtual void SetJitterAttenuatorSelect(std::bitset<2> data, bool alsoResetJA = false) override;
	virtual bool ReadJitterAttenuatorReset(std::optional<uint32_t> val = std::nullopt) override;
	virtual bool ReadJitterAttenuatorLocked(std::optional<uint32_t> val = std::nullopt) override;
	virtual void ResetJitterAttenuator() override;
	virtual RegisterFormatter FormatJitterAttenuatorCSR() override;

	/// <summary>
	/// Read the current Oscillator program for the SERDES Oscillator
	/// </summary>
	/// <returns>SERDES Oscillator Program</returns>
	uint64_t ReadSERDESOscillatorParameters(std::optional<uint32_t> val = std::nullopt);
	/// <summary>
	/// Set the SERDES Oscillator program
	/// </summary>
	/// <param name="program">New program for the SERDES Oscillator</param>
	void SetSERDESOscillatorParameters(uint64_t program);
	/// <summary>
	/// Read the current SERDES Oscillator clock speed
	/// </summary>
	/// <returns>Current SERDES clock speed</returns>
	DTC_SerdesClockSpeed ReadSERDESOscillatorClock(std::optional<uint32_t> val = std::nullopt);
	/// <summary>
	/// Set the SERDES Oscillator clock speed for the given SERDES transfer rate
	/// </summary>
	/// <param name="speed">Clock speed to set</param>
	void SetSERDESOscillatorClock(DTC_SerdesClockSpeed speed);
	/// <summary>
	/// Formats the register's current value for register dumps
	/// </summary>
	/// <returns>RegisterFormatter object containing register information</returns>
	RegisterFormatter FormatSERDESOscillatorFrequency();
	/// <summary>
	/// Formats the register's current value for register dumps
	/// </summary>
	/// <returns>RegisterFormatter object containing register information</returns>
	RegisterFormatter FormatSERDESOscillatorControl();

	// Timestamp Preset Registers
	/// <summary>
	/// Set the Timestamp preset for Timing system emulation mode
	/// </summary>
	/// <param name="preset">Timestamp for Timing emulation</param>
	void SetEventWindowTagPreset(const DTC_EventWindowTag& preset);
	/// <summary>
	/// Read the Timestamp preset for Timing system emulation mode
	/// </summary>
	/// <returns>Timestamp preset</returns>
	DTC_EventWindowTag ReadTimestampPreset(std::optional<uint32_t> val = std::nullopt);
	/// <summary>
	/// Formats the register's current value for register dumps
	/// </summary>
	/// <returns>RegisterFormatter object containing register information</returns>
	RegisterFormatter FormatTimestampPreset0();
	/// <summary>
	/// Formats the register's current value for register dumps
	/// </summary>
	/// <returns>RegisterFormatter object containing register information</returns>
	RegisterFormatter FormatTimestampPreset1();

	// NUMDTCs Register
	/// <summary>
	/// Set the maximum ROC ID for the given Link
	/// </summary>
	/// <param name="link">Link to set</param>
	/// <param name="dtcCount">Number of DTCs on the link</param>
	void SetMaxDTCNumber(const CFO_Link_ID& link, const uint8_t& dtcCount);
	/// <summary>
	/// Read the number of ROCs configured on the given Link
	/// </summary>
	/// <param name="link">Link to read</param>
	/// <param name="local">Whether to use the NUMDTCs virtual register or perform a register access (Default: true, use
	/// virtual register)</param> <returns>Number of DTCs on the link</returns>
	uint8_t ReadLinkDTCCount(const CFO_Link_ID& link, bool local = true, std::optional<uint32_t> val = std::nullopt);
	/// <summary>
	/// Formats the register's current value for register dumps
	/// </summary>
	/// <returns>RegisterFormatter object containing register information</returns>
	RegisterFormatter FormatNUMDTCs();

	// FIFO Full Error Flags Registers
	/// <summary>
	/// Clear all FIFO Full Error Flags for the given Link
	/// </summary>
	/// <param name="link">Link to clear</param>
	void ClearFIFOFullErrorFlags(const CFO_Link_ID& link);
	/// <summary>
	/// Read the FIFO Full Error/Status Flags for the given Link
	/// </summary>
	/// <param name="link">Link to read</param>
	/// <returns>CFO_FIFOFullErrorFlags object</returns>
	DTC_FIFOFullErrorFlags ReadFIFOFullErrorFlags(const CFO_Link_ID& link, std::optional<uint32_t> val = std::nullopt);
	/// <summary>
	/// Formats the register's current value for register dumps
	/// </summary>
	/// <returns>RegisterFormatter object containing register information</returns>
	RegisterFormatter FormatFIFOFullErrorFlag0();

	// Receive Packet Error Register
	/// <summary>
	/// Clear the RX Elastic Buffer Underrun Error Flag for the given Link
	/// </summary>
	/// <param name="link">Link to clear</param>
	void ClearRXElasticBufferUnderrun(const CFO_Link_ID& link);
	/// <summary>
	/// Read the RX Elastic Buffer Underrun Error Flag for the given Link
	/// </summary>
	/// <param name="link">Link to read</param>
	/// <returns>True if the RX Elastic Buffer Underrun Error Flag is set</returns>
	bool ReadRXElasticBufferUnderrun(const CFO_Link_ID& link, std::optional<uint32_t> val = std::nullopt);
	/// <summary>
	/// Clear the RX Elastic Buffer Overrun Error Flag for the given Link
	/// </summary>
	/// <param name="link">Link to clear</param>
	void ClearRXElasticBufferOverrun(const CFO_Link_ID& link);
	/// <summary>
	/// Read the RX Elastic Buffer Overrun Error Flag for the given Link
	/// </summary>
	/// <param name="link">Link to read</param>
	/// <returns>True if the RX Elastic Buffer Overrun Error Flag is set</returns>
	bool ReadRXElasticBufferOverrun(const CFO_Link_ID& link, std::optional<uint32_t> val = std::nullopt);
	/// <summary>
	/// Clear the Packet Error Flag for the given Link
	/// </summary>
	/// <param name="link">Link to clear</param>
	void ClearPacketError(const CFO_Link_ID& link);
	/// <summary>
	/// Read the Packet Error Flag for the given Link
	/// </summary>
	/// <param name="link">Link to read</param>
	/// <returns>True if the Packet Error Flag is set</returns>
	bool ReadPacketError(const CFO_Link_ID& link, std::optional<uint32_t> val = std::nullopt);
	/// <summary>
	/// Clear the Packet CRC Error Flag for the given Link
	/// </summary>
	/// <param name="link">Link to clear</param>
	void ClearPacketCRCError(const CFO_Link_ID& link);
	/// <summary>
	/// Read the Packet CRC Error Flag for the given Link
	/// </summary>
	/// <param name="link">Link to read</param>
	/// <returns>True if the Packet CRC Error Flag is set</returns>
	bool ReadPacketCRCError(const CFO_Link_ID& link, std::optional<uint32_t> val = std::nullopt);
	/// <summary>
	/// Formats the register's current value for register dumps
	/// </summary>
	/// <returns>RegisterFormatter object containing register information</returns>
	RegisterFormatter FormatReceivePacketError();

	// Event Window Emulator (Beam Off) Interval Time Register
	/// <summary>
	/// Set the Event Window Emulator (Beam Off) Interval
	/// </summary>
	/// <param name="data">Interval to set</param>
	void SetEventWindowEmulatorInterval(const uint32_t& data);
	/// <summary>
	/// Read the Event Window Emulator (Beam Off) Interval
	/// </summary>
	/// <returns>The Event Window Emulator (Beam Off) Interval</returns>
	uint32_t ReadEventWindowEmulatorInterval(std::optional<uint32_t> val = std::nullopt);
	/// <summary>
	/// Formats the register's current value for register dumps
	/// </summary>
	/// <returns>RegisterFormatter object containing register information</returns>
	RegisterFormatter FormatEventWindowEmulatorIntervalTime();

	// Event Window Holdoff Time Register
	/// <summary>
	/// Set the holdoff time (minimum dead time before Beam On)
	/// </summary>
	/// <param name="data">New value of the holdoff time parameter</param>
	void SetEventWindowHoldoffTime(const uint32_t& data);
	/// <summary>
	/// Read the current Event Window Holdoff time (minimum dead time before Beam On)
	/// </summary>
	/// <returns>The current Event Window Holdoff time (minimum dead time before Beam On)</returns>
	uint32_t ReadEventWindowHoldoffTime(std::optional<uint32_t> val = std::nullopt);
	/// <summary>
	/// Formats the register's current value for register dumps
	/// </summary>
	/// <returns>RegisterFormatter object containing register information</returns>
	RegisterFormatter FormatEventWindowHoldoffTime();

	// Event Window Timeout Error Register
	/// <summary>
	/// Read the Event Window Timeout Error bit for the given link
	/// </summary>
	/// <param name="link">Link to read</param>
	/// <returns>Value of the Event Window Timeout Error bit for the given link</returns>
	bool ReadEventWindowTimeoutError(const CFO_Link_ID& link, std::optional<uint32_t> val = std::nullopt);
	/// <summary>
	/// Clear the Event Window Timeout Error bit for the given link
	/// </summary>
	/// <param name="link">Link to clear</param>
	void ClearEventWindowTimeoutError(const CFO_Link_ID& link);
	/// <summary>
	/// Formats the register's current value for register dumps
	/// </summary>
	/// <returns>RegisterFormatter object containing register information</returns>
	RegisterFormatter FormatEventWindowTimeoutError();

	// Event Window Timeout Interval Register
	/// <summary>
	/// Set the Event Window Timeout Interval
	/// </summary>
	/// <param name="data">New Event Window Timeout Interval</param>
	void SetEventWindowTimeoutInterval(const uint32_t& data);
	/// <summary>
	/// Read the Event Window Timeout Interval
	/// </summary>
	/// <returns>The Event Window Timeout Interval</returns>
	uint32_t ReadEventWindowTimeoutInterval(std::optional<uint32_t> val = std::nullopt);
	/// <summary>
	/// Formats the register's current value for register dumps
	/// </summary>
	/// <returns>RegisterFormatter object containing register information</returns>
	RegisterFormatter FormatEventWindowTimeoutInterval();

	/// <summary>
	/// Clear the value of the Receive byte counter
	/// </summary>
	/// <param name="link">Link to clear counter for</param>
	void ClearReceiveByteCount(const CFO_Link_ID& link);
	/// <summary>
	/// Read the value of the Receive byte counter
	/// </summary>
	/// <param name="link">Link to read counter for</param>
	/// <returns>Current value of the Receive byte counter on the given Link</returns>
	uint32_t ReadReceiveByteCount(const CFO_Link_ID& link, std::optional<uint32_t> val = std::nullopt);
	/// <summary>
	/// Clear the value of the Receive Packet counter
	/// </summary>
	/// <param name="link">Link to clear counter for</param>
	void ClearReceivePacketCount(const CFO_Link_ID& link);
	/// <summary>
	/// Read the value of the Receive Packet counter
	/// </summary>
	/// <param name="link">Link to read counter for</param>
	/// <returns>Current value of the Receive Packet counter on the given Link</returns>
	uint32_t ReadReceivePacketCount(const CFO_Link_ID& link, std::optional<uint32_t> val = std::nullopt);
	/// <summary>
	/// Clear the value of the Transmit byte counter
	/// </summary>
	/// <param name="link">Link to clear counter for</param>
	void ClearTransmitByteCount(const CFO_Link_ID& link);
	/// <summary>
	/// Read the value of the Transmit byye counter
	/// </summary>
	/// <param name="link">Link to read counter for</param>
	/// <returns>Current value of the Transmit byte counter on the given Link</returns>
	uint32_t ReadTransmitByteCount(const CFO_Link_ID& link, std::optional<uint32_t> val = std::nullopt);
	/// <summary>
	/// Clear the value of the Transmit Packet counter
	/// </summary>
	/// <param name="link">Link to clear counter for</param>
	void ClearTransmitPacketCount(const CFO_Link_ID& link);
	/// <summary>
	/// Read the value of the Transmit Packet counter
	/// </summary>
	/// <param name="link">Link to read counter for</param>
	/// <returns>Current value of the Transmit Packet counter on the given Link</returns>
	uint32_t ReadTransmitPacketCount(const CFO_Link_ID& link, std::optional<uint32_t> val = std::nullopt);
	/// <summary>
	/// Formats the register's current value for register dumps
	/// </summary>
	/// <returns>RegisterFormatter object containing register information</returns>
	RegisterFormatter FormatReceiveByteCountLink0();
	/// <summary>
	/// Formats the register's current value for register dumps
	/// </summary>
	/// <returns>RegisterFormatter object containing register information</returns>
	RegisterFormatter FormatReceiveByteCountLink1();
	/// <summary>
	/// Formats the register's current value for register dumps
	/// </summary>
	/// <returns>RegisterFormatter object containing register information</returns>
	RegisterFormatter FormatReceiveByteCountLink2();
	/// <summary>
	/// Formats the register's current value for register dumps
	/// </summary>
	/// <returns>RegisterFormatter object containing register information</returns>
	RegisterFormatter FormatReceiveByteCountLink3();
	/// <summary>
	/// Formats the register's current value for register dumps
	/// </summary>
	/// <returns>RegisterFormatter object containing register information</returns>
	RegisterFormatter FormatReceiveByteCountLink4();
	/// <summary>
	/// Formats the register's current value for register dumps
	/// </summary>
	/// <returns>RegisterFormatter object containing register information</returns>
	RegisterFormatter FormatReceiveByteCountLink5();
	/// <summary>
	/// Formats the register's current value for register dumps
	/// </summary>
	/// <returns>RegisterFormatter object containing register information</returns>
	RegisterFormatter FormatReceiveByteCountLink6();
	/// <summary>
	/// Formats the register's current value for register dumps
	/// </summary>
	/// <returns>RegisterFormatter object containing register information</returns>
	RegisterFormatter FormatReceiveByteCountLink7();
	/// <summary>
	/// Formats the register's current value for register dumps
	/// </summary>
	/// <returns>RegisterFormatter object containing register information</returns>
	RegisterFormatter FormatReceivePacketCountLink0();
	/// <summary>
	/// Formats the register's current value for register dumps
	/// </summary>
	/// <returns>RegisterFormatter object containing register information</returns>
	RegisterFormatter FormatReceivePacketCountLink1();
	/// <summary>
	/// Formats the register's current value for register dumps
	/// </summary>
	/// <returns>RegisterFormatter object containing register information</returns>
	RegisterFormatter FormatReceivePacketCountLink2();
	/// <summary>
	/// Formats the register's current value for register dumps
	/// </summary>
	/// <returns>RegisterFormatter object containing register information</returns>
	RegisterFormatter FormatReceivePacketCountLink3();
	/// <summary>
	/// Formats the register's current value for register dumps
	/// </summary>
	/// <returns>RegisterFormatter object containing register information</returns>
	RegisterFormatter FormatReceivePacketCountLink4();
	/// <summary>
	/// Formats the register's current value for register dumps
	/// </summary>
	/// <returns>RegisterFormatter object containing register information</returns>
	RegisterFormatter FormatReceivePacketCountLink5();
	/// <summary>
	/// Formats the register's current value for register dumps
	/// </summary>
	/// <returns>RegisterFormatter object containing register information</returns>
	RegisterFormatter FormatReceivePacketCountLink6();
	/// <summary>
	/// Formats the register's current value for register dumps
	/// </summary>
	/// <returns>RegisterFormatter object containing register information</returns>
	RegisterFormatter FormatReceivePacketCountLink7();
	/// <summary>
	/// Formats the register's current value for register dumps
	/// </summary>
	/// <returns>RegisterFormatter object containing register information</returns>
	RegisterFormatter FormatTramsitByteCountLink0();
	/// <summary>
	/// Formats the register's current value for register dumps
	/// </summary>
	/// <returns>RegisterFormatter object containing register information</returns>
	RegisterFormatter FormatTramsitByteCountLink1();
	/// <summary>
	/// Formats the register's current value for register dumps
	/// </summary>
	/// <returns>RegisterFormatter object containing register information</returns>
	RegisterFormatter FormatTramsitByteCountLink2();
	/// <summary>
	/// Formats the register's current value for register dumps
	/// </summary>
	/// <returns>RegisterFormatter object containing register information</returns>
	RegisterFormatter FormatTramsitByteCountLink3();
	/// <summary>
	/// Formats the register's current value for register dumps
	/// </summary>
	/// <returns>RegisterFormatter object containing register information</returns>
	RegisterFormatter FormatTramsitByteCountLink4();
	/// <summary>
	/// Formats the register's current value for register dumps
	/// </summary>
	/// <returns>RegisterFormatter object containing register information</returns>
	RegisterFormatter FormatTramsitByteCountLink5();
	/// <summary>
	/// Formats the register's current value for register dumps
	/// </summary>
	/// <returns>RegisterFormatter object containing register information</returns>
	RegisterFormatter FormatTramsitByteCountLink6();
	/// <summary>
	/// Formats the register's current value for register dumps
	/// </summary>
	/// <returns>RegisterFormatter object containing register information</returns>
	RegisterFormatter FormatTramsitByteCountLink7();
	/// <summary>
	/// Formats the register's current value for register dumps
	/// </summary>
	/// <returns>RegisterFormatter object containing register information</returns>
	RegisterFormatter FormatTransmitPacketCountLink0();
	/// <summary>
	/// Formats the register's current value for register dumps
	/// </summary>
	/// <returns>RegisterFormatter object containing register information</returns>
	RegisterFormatter FormatTransmitPacketCountLink1();
	/// <summary>
	/// Formats the register's current value for register dumps
	/// </summary>
	/// <returns>RegisterFormatter object containing register information</returns>
	RegisterFormatter FormatTransmitPacketCountLink2();
	/// <summary>
	/// Formats the register's current value for register dumps
	/// </summary>
	/// <returns>RegisterFormatter object containing register information</returns>
	RegisterFormatter FormatTransmitPacketCountLink3();
	/// <summary>
	/// Formats the register's current value for register dumps
	/// </summary>
	/// <returns>RegisterFormatter object containing register information</returns>
	RegisterFormatter FormatTransmitPacketCountLink4();
	/// <summary>
	/// Formats the register's current value for register dumps
	/// </summary>
	/// <returns>RegisterFormatter object containing register information</returns>
	RegisterFormatter FormatTransmitPacketCountLink5();
	/// <summary>
	/// Formats the register's current value for register dumps
	/// </summary>
	/// <returns>RegisterFormatter object containing register information</returns>
	RegisterFormatter FormatTransmitPacketCountLink6();
	/// <summary>
	/// Formats the register's current value for register dumps
	/// </summary>
	/// <returns>RegisterFormatter object containing register information</returns>
	RegisterFormatter FormatTransmitPacketCountLink7();

	// DDR3 Memory DMA Write Start Address Register
	/// <summary>
	/// Set the Write Start Address for DMAs into the DDR memory
	/// </summary>
	/// <param name="address">Start address</param>
	void SetDMAWriteStartAddress(const uint32_t& address);
	/// <summary>
	/// Read the Write Start Address for DMAs into the DDR memory
	/// </summary>
	/// <returns>Start Address</returns>
	uint32_t ReadDMAWriteStartAddress(std::optional<uint32_t> val = std::nullopt);
	/// <summary>
	/// Formats the register's current value for register dumps
	/// </summary>
	/// <returns>RegisterFormatter object containing register information</returns>
	RegisterFormatter FormatDMAWriteStartAddress();

	// DDR3 Memory DMA Read Start Address Register
	/// <summary>
	/// Set the Read Start Address for DMAs into the DDR memory
	/// </summary>
	/// <param name="address">Start address</param>
	void SetDMAReadStartAddress(const uint32_t& address);
	/// <summary>
	/// Read the Read Start Address for DMAs into the DDR memory
	/// </summary>
	/// <returns>Start Address</returns>
	uint32_t ReadDMAReadStartAddress(std::optional<uint32_t> val = std::nullopt);
	/// <summary>
	/// Formats the register's current value for register dumps
	/// </summary>
	/// <returns>RegisterFormatter object containing register information</returns>
	RegisterFormatter FormatDMAReadStartAddress();

	// DDR3 Memory DMA Read Byte Count / Start Read DMA Register
	/// <summary>
	/// Set the number of bytes to read for a DMA. Setting this register also initiates the DMA
	/// </summary>
	/// <param name="bytes">Bytes to read</param>
	void SetDMAReadByteCount(const uint32_t& bytes);
	/// <summary>
	/// Read the current value of the DMA Read Byte Count
	/// </summary>
	/// <returns>Bytes to read</returns>
	uint32_t ReadDMAReadByteCount(std::optional<uint32_t> val = std::nullopt);
	/// <summary>
	/// Start a DMA read using the byte count already present in the register
	/// </summary>
	void StartDMARead() { SetDMAReadByteCount(ReadDMAReadByteCount()); }
	/// <summary>
	/// Formats the register's current value for register dumps
	/// </summary>
	/// <returns>RegisterFormatter object containing register information</returns>
	RegisterFormatter FormatDMAReadByteCount();

	// DDR3 Beam On Base Address Register
	/// <summary>
	/// Set the Beam On Base Address in the DDR memory
	/// </summary>
	/// <param name="address">Base address</param>
	void SetRunPlanBeamOnBaseAddress(const uint32_t& address);
	/// <summary>
	/// Read the Beam On Base Address for DMAs into the DDR memory
	/// </summary>
	/// <returns>Base Address</returns>
	uint32_t ReadRunPlanBeamOnBaseAddress(std::optional<uint32_t> val = std::nullopt);
	/// <summary>
	/// Formats the register's current value for register dumps
	/// </summary>
	/// <returns>RegisterFormatter object containing register information</returns>
	RegisterFormatter FormatRunPlanBeamOnBaseAddress();

	// DDR3 Beam Off Base Address Register
	/// <summary>
	/// Set the Beam Off Base Address in the DDR memory
	/// </summary>
	/// <param name="address">Base address</param>
	void SetRunPlanBeamOffBaseAddress(const uint32_t& address);
	/// <summary>
	/// Read the Beam Off Base Address for DMAs into the DDR memory
	/// </summary>
	/// <returns>Base Address</returns>
	uint32_t ReadRunPlanBeamOffBaseAddress(std::optional<uint32_t> val = std::nullopt);
	/// <summary>
	/// Formats the register's current value for register dumps
	/// </summary>
	/// <returns>RegisterFormatter object containing register information</returns>
	RegisterFormatter FormatRunPlanBeamOffBaseAddress();
	/// <summary>
	/// Write run plan data to the specified DDR memory address.
	/// </summary>
	/// <param name="inputData">Serialized run plan data to be written.</param>
	/// <param name="address">DDR memory address where the run plan data should be stored.</param>
	void SetRunPlanData(const std::string& inputData, const uint32_t& address);
	/// <summary>
	/// Compare the provided run plan data with the data stored at the specified DDR memory address.
	/// </summary>
	/// <param name="inputData">Serialized run plan data expected at the given address.</param>
	/// <param name="address">DDR memory address from which the current run plan data is read.</param>
	/// <param name="mismatches">
	/// Optional pointer to a map that will be populated with any mismatching words,
	/// keyed by address and containing expected/actual value pairs.
	/// </param>
	void CompareRunPlanData(const std::string& inputData, const uint32_t& address,
							std::map<uint32_t /* address */,
									 std::pair<uint32_t /* expected */,
											   uint32_t /* actual */>>* mismatches = nullptr);
	/// <summary>
	/// Read the current run plan mode as reported by the firmware.
	/// </summary>
	/// <returns>The current run plan mode value.</returns>
	uint64_t ReadRunPlanCurrentMode();
	/// <summary>
	/// Format the current run plan mode value for inclusion in register dumps.
	/// </summary>
	/// <returns>RegisterFormatter object containing the current mode information.</returns>
	RegisterFormatter FormatRunPlanCurrentMode();
	/// <summary>
	/// Read the current run plan tag as reported by the firmware.
	/// </summary>
	/// <returns>The current run plan tag value.</returns>
	uint64_t ReadRunPlanCurrentTag();
	/// <summary>
	/// Format the current run plan tag value for inclusion in register dumps.
	/// </summary>
	/// <returns>RegisterFormatter object containing the current tag information.</returns>
	RegisterFormatter FormatRunPlanCurrentTag();

	// Firefly CSR Register
	/// <summary>
	/// Read the present bit for the TX/RX Firefly
	/// </summary>
	/// <returns>Whether the Firefly is present</returns>
	bool ReadFireflyTXRXPresent(std::optional<uint32_t> val = std::nullopt);
	/// <summary>
	/// Read the present bit for the RX Firefly
	/// </summary>
	/// <returns>Whether the Firefly is present</returns>
	bool ReadFireflyRXPresent(std::optional<uint32_t> val = std::nullopt);
	/// <summary>
	/// Read the present bit for the TX Firefly
	/// </summary>
	/// <returns>Whether the Firefly is present</returns>
	bool ReadFireflyTXPresent(std::optional<uint32_t> val = std::nullopt);
	/// <summary>
	/// Read the interrupt bit for the TX/RX Firefly
	/// </summary>
	/// <returns>Whether the Firefly is in interrupt state</returns>
	bool ReadFireflyTXRXInterrupt(std::optional<uint32_t> val = std::nullopt);
	/// <summary>
	/// Read the interrupt bit for the RX Firefly
	/// </summary>
	/// <returns>Whether the Firefly is in interrupt state</returns>
	bool ReadFireflyRXInterrupt(std::optional<uint32_t> val = std::nullopt);
	/// <summary>
	/// Read the interrupt bit for the TX Firefly
	/// </summary>
	/// <returns>Whether the Firefly is in interrupt state</returns>
	bool ReadFireflyTXInterrupt(std::optional<uint32_t> val = std::nullopt);
	/// <summary>
	/// Set the Select bit of the TX/RX Firefly
	/// </summary>
	/// <param name="select">Value to set</param>
	void SetFireflyTXRXSelect(bool select);
	/// <summary>
	/// Set the Select bit of the RX Firefly
	/// </summary>
	/// <param name="select">Value to set</param>
	void SetFireflyRXSelect(bool select);
	/// <summary>
	/// Set the Select bit of the TX Firefly
	/// </summary>
	/// <param name="select">Value to set</param>
	void SetFireflyTXSelect(bool select);
	/// <summary>
	/// Read the select bit for the TX/RX Firefly
	/// </summary>
	/// <returns>Whether the Firefly is selected</returns>
	bool ReadFireflyTXRXSelect(std::optional<uint32_t> val = std::nullopt);
	/// <summary>
	/// Read the select bit for the RX Firefly
	/// </summary>
	/// <returns>Whether the Firefly is selected</returns>
	bool ReadFireflyRXSelect(std::optional<uint32_t> val = std::nullopt);
	/// <summary>
	/// Read the select bit for the TX Firefly
	/// </summary>
	/// <returns>Whether the Firefly is selected</returns>
	bool ReadFireflyTXSelect(std::optional<uint32_t> val = std::nullopt);
	/// <summary>
	/// Set the Reset bit of the TX/RX Firefly
	/// </summary>
	/// <param name="reset">Value to set</param>
	void SetFireflyTXRXReset(bool reset);
	/// <summary>
	/// Set the Reset bit of the RX Firefly
	/// </summary>
	/// <param name="reset">Value to set</param>
	void SetFireflyRXReset(bool reset);
	/// <summary>
	/// Set the Reset bit of the TX Firefly
	/// </summary>
	/// <param name="reset">Value to set</param>
	void SetFireflyTXReset(bool reset);
	/// <summary>
	/// Read the reset bit for the TX/RX Firefly
	/// </summary>
	/// <returns>Whether the Firefly is in reset state</returns>
	bool ReadFireflyTXRXReset(std::optional<uint32_t> val = std::nullopt);
	/// <summary>
	/// Read the reset bit for the RX Firefly
	/// </summary>
	/// <returns>Whether the Firefly is in reset state</returns>
	bool ReadFireflyRXReset(std::optional<uint32_t> val = std::nullopt);
	/// <summary>
	/// Read the reset bit for the TX Firefly
	/// </summary>
	/// <returns>Whether the Firefly is in reset state</returns>
	bool ReadFireflyTXReset(std::optional<uint32_t> val = std::nullopt);
	/// <summary>
	/// Formats the register's current value for register dumps
	/// </summary>
	/// <returns>RegisterFormatter object containing register information</returns>
	RegisterFormatter FormatFireflyCSR();

	// SERDES PRBS Control Registers
	/// <summary>
	/// Read the SERDES PRBS Error flag for the given link
	/// </summary>
	/// <param name="link">Link to read</param>
	/// <returns>Value of the flag</returns>
	bool ReadSERDESPRBSErrorFlag(const CFO_Link_ID& link, std::optional<uint32_t> val = std::nullopt);
	/// <summary>
	/// Read the TX PRBS SEL byte for the given link
	/// </summary>
	/// <param name="link">Link to read</param>
	/// <returns>Value of the byte</returns>
	uint8_t ReadSERDESTXPRBSSEL(const CFO_Link_ID& link, std::optional<uint32_t> val = std::nullopt);
	/// <summary>
	/// Set the TX PRBS SEL byte for the given link to the given value
	/// </summary>
	/// <param name="link">Link to set</param>
	/// <param name="byte">Value of the byte</param>
	void SetSERDESTXPRBSSEL(const CFO_Link_ID& link, uint8_t byte);
	/// <summary>
	/// Read the RX PRBS SEL byte for the given link
	/// </summary>
	/// <param name="link">Link to read</param>
	/// <returns>Value of the byte</returns>
	uint8_t ReadSERDESRXPRBSSEL(const CFO_Link_ID& link, std::optional<uint32_t> val = std::nullopt);
	/// <summary>
	/// Set the RX PRBS SEL byte for the given link to the given value
	/// </summary>
	/// <param name="link">Link to set</param>
	/// <param name="byte">Value of the byte</param>
	void SetSERDESRXPRBSSEL(const CFO_Link_ID& link, uint8_t byte);
	/// <summary>
	/// Read the SERDES TX PRBS Force Error bit for the given link
	/// </summary>
	/// <param name="link">Link to read</param>
	/// <returns>Value of the bit</returns>
	bool ReadSERDESTXPRBSForceError(const CFO_Link_ID& link, std::optional<uint32_t> val = std::nullopt);
	/// <summary>
	/// Set the SERDES TX PRBS Force Error bit for the given link to the given value
	/// </summary>
	/// <param name="link">Link to set</param>
	/// <param name="flag">Value to set</param>
	void SetSERDESTXPRBSForceError(const CFO_Link_ID& link, bool flag);
	/// <summary>
	/// Toggle the SERDES TX PRBS Force Error bit for the given link
	/// </summary>
	/// <param name="link">Link to toggle</param>
	void ToggleSERDESTXPRBSForceError(const CFO_Link_ID& link);
	/// <summary>
	/// Read the SERDES RX PRBS Count Reset bit for the given link
	/// </summary>
	/// <param name="link">Link to read</param>
	/// <returns>Value of the bit</returns>
	bool ReadSERDESRXPRBSCountReset(const CFO_Link_ID& link, std::optional<uint32_t> val = std::nullopt);
	/// <summary>
	/// Set the SERDES RX PRBS Count Reset bit for the given link to the given value
	/// </summary>
	/// <param name="link">Link to set</param>
	/// <param name="flag">Value to set</param>
	void SetSERDESRXPRBSCountReset(const CFO_Link_ID& link, bool flag);
	/// <summary>
	/// Toggle the SERDES RX PRBS Count Reset bit for the given link
	/// </summary>
	/// <param name="link">Link to toggle</param>
	void ToggleSERDESRXPRBSCountReset(const CFO_Link_ID& link);
	/// <summary>
	/// Formats the register's current value for register dumps
	/// </summary>
	/// <returns>RegisterFormatter object containing register information</returns>
	RegisterFormatter FormatSERDESPRBSControlLink0();
	/// <summary>
	/// Formats the register's current value for register dumps
	/// </summary>
	/// <returns>RegisterFormatter object containing register information</returns>
	RegisterFormatter FormatSERDESPRBSControlLink1();
	/// <summary>
	/// Formats the register's current value for register dumps
	/// </summary>
	/// <returns>RegisterFormatter object containing register information</returns>
	RegisterFormatter FormatSERDESPRBSControlLink2();
	/// <summary>
	/// Formats the register's current value for register dumps
	/// </summary>
	/// <returns>RegisterFormatter object containing register information</returns>
	RegisterFormatter FormatSERDESPRBSControlLink3();
	/// <summary>
	/// Formats the register's current value for register dumps
	/// </summary>
	/// <returns>RegisterFormatter object containing register information</returns>
	RegisterFormatter FormatSERDESPRBSControlLink4();
	/// <summary>
	/// Formats the register's current value for register dumps
	/// </summary>
	/// <returns>RegisterFormatter object containing register information</returns>
	RegisterFormatter FormatSERDESPRBSControlLink5();
	/// <summary>
	/// Formats the register's current value for register dumps
	/// </summary>
	/// <returns>RegisterFormatter object containing register information</returns>
	RegisterFormatter FormatSERDESPRBSControlLink6();
	/// <summary>
	/// Formats the register's current value for register dumps
	/// </summary>
	/// <returns>RegisterFormatter object containing register information</returns>
	RegisterFormatter FormatSERDESPRBSControlLink7();

	// Cable Delays
	uint32_t ReadCableDelayMeasureExponentialCount(std::optional<uint32_t> val = std::nullopt);
	void SetCableDelayMeasureExponentialCount(const uint32_t exponent);
	uint32_t ReadCableDelayMeasurement(const CFO_Link_ID link, const uint8_t roc, bool& done);

	// /// <summary>
	// /// Set the cable delay value for the given link
	// /// </summary>
	// /// <param name="link">Link to set</param>
	// /// <param name="delay">Delay value to set</param>
	// void SetCableDelayValue(const CFO_Link_ID& link, const uint32_t delay);
	// /// <summary>
	// /// Read the configured delay value for the given link
	// /// </summary>
	// /// <param name="link">Link to read</param>
	// /// <returns>Configured delay value for the given link</returns>
	// uint32_t ReadCableDelayValue(const CFO_Link_ID& link, std::optional<uint32_t> val = std::nullopt);
	// /// <summary>
	// /// Formats the register's current value for register dumps
	// /// </summary>
	// /// <returns>RegisterFormatter object containing register information</returns>
	// RegisterFormatter FormatCableDelayValueLink0();
	// /// <summary>
	// /// Formats the register's current value for register dumps
	// /// </summary>
	// /// <returns>RegisterFormatter object containing register information</returns>
	// RegisterFormatter FormatCableDelayValueLink1();
	// /// <summary>
	// /// Formats the register's current value for register dumps
	// /// </summary>
	// /// <returns>RegisterFormatter object containing register information</returns>
	// RegisterFormatter FormatCableDelayValueLink2();
	// /// <summary>
	// /// Formats the register's current value for register dumps
	// /// </summary>
	// /// <returns>RegisterFormatter object containing register information</returns>
	// RegisterFormatter FormatCableDelayValueLink3();
	// /// <summary>
	// /// Formats the register's current value for register dumps
	// /// </summary>
	// /// <returns>RegisterFormatter object containing register information</returns>
	// RegisterFormatter FormatCableDelayValueLink4();
	// /// <summary>
	// /// Formats the register's current value for register dumps
	// /// </summary>
	// /// <returns>RegisterFormatter object containing register information</returns>
	// RegisterFormatter FormatCableDelayValueLink5();
	// /// <summary>
	// /// Formats the register's current value for register dumps
	// /// </summary>
	// /// <returns>RegisterFormatter object containing register information</returns>
	// RegisterFormatter FormatCableDelayValueLink6();
	// /// <summary>
	// /// Formats the register's current value for register dumps
	// /// </summary>
	// /// <returns>RegisterFormatter object containing register information</returns>
	// RegisterFormatter FormatCableDelayValueLink7();

	// // Cable Delay Control And Status Register
	// /// <summary>
	// /// reset all the bits of the register
	// /// </summary>
	// void ResetDelayRegister();

	// /// <summary>
	// /// Read the Measure Error bit for the given link
	// /// </summary>
	// /// <param name="link">Link to read</param>
	// /// <returns>Value of the bit</returns>
	// bool ReadDelayMeasureError(const CFO_Link_ID& link, std::optional<uint32_t> val = std::nullopt);
	// /// <summary>
	// /// Read the Cable Delay External Loopback Enable bit
	// /// </summary>
	// /// <returns>Value of the bit</returns>
	// bool ReadDelayExternalLoopbackEnable(std::optional<uint32_t> val = std::nullopt);
	// /// <summary>
	// /// Set the Cable Delay External Loopback Enable bit
	// /// </summary>
	// /// <param name="value">Value to set</param>
	// void SetDelayExternalLoopbackEnable(bool value);
	// /// <summary>
	// /// Enable the Delay Measure Mode on the given link
	// /// </summary>
	// /// <param name="link">Link to enable</param>
	// void EnableDelayMeasureMode(const CFO_Link_ID& link);
	// /// <summary>
	// /// Disable the Delay Measure Mode on the given link
	// /// </summary>
	// /// <param name="link">Link to disable</param>
	// void DisableDelayMeasureMode(const CFO_Link_ID& link);
	// /// <summary>
	// /// Read the Measure Mode Enable bit for the given link
	// /// </summary>
	// /// <param name="link">Link to read</param>
	// /// <returns>Value of the bit</returns>
	// bool ReadDelayMeasureMode(const CFO_Link_ID& link, std::optional<uint32_t> val = std::nullopt);
	// /// <summary>
	// /// Set the Delay Measure Now bit on the given link
	// /// </summary>
	// /// <param name="link">Link to enable</param>
	// void EnableDelayMeasureNow(const CFO_Link_ID& link);
	// /// <summary>
	// /// Clear the Delay Measure Now bit on the given link
	// /// </summary>
	// /// <param name="link">Link to disable</param>
	// void DisableDelayMeasureNow(const CFO_Link_ID& link);
	// /// <summary>
	// /// Read the Measure Now bit for the given link
	// /// </summary>
	// /// <param name="link">Link to read</param>
	// /// <returns>Value of the bit</returns>
	// bool ReadDelayMeasureNow(const CFO_Link_ID& link, std::optional<uint32_t> val = std::nullopt);
	// /// <summary>
	// /// Formats the register's current value for register dumps
	// /// </summary>
	// /// <returns>RegisterFormatter object containing register information</returns>
	// RegisterFormatter FormatCableDelayControl();

	// FPGA PROM Program Data Register

	// FPGA PROM Program Status Register
	/// <summary>
	/// Read the full bit on the FPGA PROM FIFO
	/// </summary>
	/// <returns>the full bit on the FPGA PROM FIFO</returns>
	bool ReadFPGAPROMProgramFIFOFull(std::optional<uint32_t> val = std::nullopt);
	/// <summary>
	/// Read whether the FPGA PROM is ready for data
	/// </summary>
	/// <returns>whether the FPGA PROM is ready for data</returns>
	bool ReadFPGAPROMReady(std::optional<uint32_t> val = std::nullopt);
	/// <summary>
	/// Formats the register's current value for register dumps
	/// </summary>
	/// <returns>RegisterFormatter object containing register information</returns>
	RegisterFormatter FormatFPGAPROMProgramStatus();

	// FPGA Core Access Register
	/// <summary>
	/// Performs the chants necessary to reload the CFO firmware
	/// </summary>
	void ReloadFPGAFirmware();
	/// <summary>
	/// Read the FPGA Core Access FIFO Full bit
	/// </summary>
	/// <returns>Whether the FPGA Core Access FIFO is full</returns>
	bool ReadFPGACoreAccessFIFOFull(std::optional<uint32_t> val = std::nullopt);
	/// <summary>
	/// Read the FPGA Core Access FIFO Empty bit
	/// </summary>
	/// <returns>Whether the FPGA Core Access FIFO is empty</returns>
	bool ReadFPGACoreAccessFIFOEmpty(std::optional<uint32_t> val = std::nullopt);
	/// <summary>
	/// Formats the register's current value for register dumps
	/// </summary>
	/// <returns>RegisterFormatter object containing register information</returns>
	RegisterFormatter FormatFPGACoreAccess();

	// Oscillator Programming (DDR and SERDES)
	/// <summary>
	/// Set the given oscillator to the given frequency, calculating a new program in the process.
	/// </summary>
	/// <param name="targetFrequency">New frequency to program, in Hz</param>
	/// <returns>Whether the oscillator frequency was changed</returns>
	bool SetNewOscillatorFrequency(double targetFrequency);

	/// <summary>
	/// Disable all CFO links
	/// </summary>
	void DisableLinks();

	/// <summary>
	/// Disable all CFO outputs
	/// </summary>
	void DisableAllOutputs();

private:
	bool NeedToVerifyRegisterWrite_(const CFOandDTC_Register& address) override;
	void VerifyRegisterWrite_(const CFOandDTC_Register& address, uint32_t readbackValue, uint32_t dataToWrite) override;

	int DecodeHighSpeedDivider_(int input);
	int DecodeOutputDivider_(int input) { return input + 1; }
	double DecodeRFREQ_(uint64_t input) { return input / 268435456.0; }
	int EncodeHighSpeedDivider_(int input);
	int EncodeOutputDivider_(int input);
	uint64_t EncodeRFREQ_(double input) { return static_cast<uint64_t>(input * 268435456) & 0x3FFFFFFFFF; }
	uint64_t CalculateFrequencyForProgramming_(double targetFrequency, double currentFrequency,
											   uint64_t currentProgram);

protected:
	DTC_SimMode simMode_;         ///< Simulation mode
	uint32_t maxDTCs_;            ///< Map of active DTCs
	bool usingDetectorEmulator_;  ///< Whether Detector Emulation mode is enabled
	uint16_t dmaSize_;            ///< Size of DMAs, in bytes (default 32k)

public:
	virtual const std::vector<std::function<RegisterFormatter()>>& getFormattedDumpFunctions() override { return formattedDumpFunctions_; };
	virtual const std::vector<std::function<RegisterFormatter()>>& getFormattedSimpleDumpFunctions() override { return formattedSimpleDumpFunctions_; };

	const std::vector<std::function<RegisterFormatter()>> formattedSimpleDumpFunctions_{
		[this] { return this->FormatCFOControl(); },
		[this] { return this->FormatBeamOffMode(); },
		[this] { return this->FormatBeamOnMode(); },
		[this] { return this->FormatJitterAttenuatorCSR(); },
		[this] { return this->FormatSERDESPLLLocked(); },
		[this] { return this->FormatLinkEnable(); },
		[this] { return this->FormatSERDESRXCDRLock(); },
		[this] { return this->FormatSERDESResetDone(); },
		[this] { return this->FormatSERDESReset(); },
	};

	const std::vector<std::function<RegisterFormatter()>> formattedDumpFunctions_{
		[this] { return this->FormatDesignVersion(); },
		[this] { return this->FormatDesignDate(); },
		[this] { return this->FormatDesignStatus(); },
		[this] { return this->FormatVivadoVersion(); },
		[this] { return this->FormatCFOControl(); },
		[this] { return this->FormatDMATransferLength(); },
		[this] { return this->FormatSERDESLoopbackEnable(); },
		[this] { return this->FormatClockOscillatorStatus(); },
		[this] { return this->FormatLinkEnable(); },
		[this] { return this->FormatSERDESReset(); },
		[this] { return this->FormatSERDESRXDisparityError(); },
		[this] { return this->FormatSERDESRXCharacterNotInTableError(); },
		[this] { return this->FormatSERDESUnlockError(); },
		[this] { return this->FormatSERDESPLLLocked(); },
		[this] { return this->FormatSERDESRXStatus(); },
		[this] { return this->FormatSERDESResetDone(); },
		[this] { return this->FormatSERDESRXCDRLock(); },
		[this] { return this->FormatBeamOnTimerPreset(); },
		[this] { return this->FormatBeamOnMode(); },
		[this] { return this->FormatBeamOffMode(); },
		[this] { return this->FormatClockMarkerIntervalCount(); },
		[this] { return this->FormatSERDESOscillatorFrequency(); },
		[this] { return this->FormatSERDESOscillatorControl(); },
		[this] { return this->FormatSERDESOscillatorParameterLow(); },
		[this] { return this->FormatSERDESOscillatorParameterHigh(); },
		[this] { return this->FormatTimestampPreset0(); },
		[this] { return this->FormatTimestampPreset1(); },
		[this] { return this->FormatNUMDTCs(); },
		[this] { return this->FormatFIFOFullErrorFlag0(); },
		[this] { return this->FormatReceivePacketError(); },
		// [this] { return this->FormatEventWindowEmulatorIntervalTime(); },
		[this] { return this->FormatEventWindowHoldoffTime(); },
		[this] { return this->FormatEventWindowTimeoutError(); },
		[this] { return this->FormatEventWindowTimeoutInterval(); },
		[this] { return this->FormatDMAWriteStartAddress(); },
		[this] { return this->FormatDMAReadStartAddress(); },
		[this] { return this->FormatDMAReadByteCount(); },
		[this] { return this->FormatRunPlanBeamOnBaseAddress(); },
		[this] { return this->FormatRunPlanBeamOffBaseAddress(); },
		[this] { return this->FormatFireflyCSR(); },
		[this] { return this->FormatSERDESPRBSControlLink0(); },
		[this] { return this->FormatSERDESPRBSControlLink1(); },
		[this] { return this->FormatSERDESPRBSControlLink2(); },
		[this] { return this->FormatSERDESPRBSControlLink3(); },
		[this] { return this->FormatSERDESPRBSControlLink4(); },
		[this] { return this->FormatSERDESPRBSControlLink5(); },
		[this] { return this->FormatSERDESPRBSControlLink6(); },
		[this] { return this->FormatSERDESPRBSControlLink7(); },
		// [this] { return this->FormatCableDelayValueLink0(); },
		// [this] { return this->FormatCableDelayValueLink1(); },
		// [this] { return this->FormatCableDelayValueLink2(); },
		// [this] { return this->FormatCableDelayValueLink3(); },
		// [this] { return this->FormatCableDelayValueLink4(); },
		// [this] { return this->FormatCableDelayValueLink5(); },
		// [this] { return this->FormatCableDelayValueLink6(); },
		// [this] { return this->FormatCableDelayValueLink7(); },
		// [this] { return this->FormatCableDelayControl(); },
		[this] { return this->FormatFPGAPROMProgramStatus(); },
		[this] { return this->FormatFPGACoreAccess(); }};

	/// <summary>
	/// Dump Byte/Packet Counter Registers
	/// </summary>
	const std::vector<std::function<RegisterFormatter()>> formattedCounterFunctions_{
		[this] { return this->FormatReceiveByteCountLink0(); },
		[this] { return this->FormatReceiveByteCountLink1(); },
		[this] { return this->FormatReceiveByteCountLink2(); },
		[this] { return this->FormatReceiveByteCountLink3(); },
		[this] { return this->FormatReceiveByteCountLink4(); },
		[this] { return this->FormatReceiveByteCountLink5(); },
		[this] { return this->FormatReceiveByteCountLink6(); },
		[this] { return this->FormatReceiveByteCountLink7(); },
		[this] { return this->FormatReceivePacketCountLink0(); },
		[this] { return this->FormatReceivePacketCountLink1(); },
		[this] { return this->FormatReceivePacketCountLink2(); },
		[this] { return this->FormatReceivePacketCountLink3(); },
		[this] { return this->FormatReceivePacketCountLink4(); },
		[this] { return this->FormatReceivePacketCountLink5(); },
		[this] { return this->FormatReceivePacketCountLink6(); },
		[this] { return this->FormatReceivePacketCountLink7(); },
		[this] { return this->FormatTramsitByteCountLink0(); },
		[this] { return this->FormatTramsitByteCountLink1(); },
		[this] { return this->FormatTramsitByteCountLink2(); },
		[this] { return this->FormatTramsitByteCountLink3(); },
		[this] { return this->FormatTramsitByteCountLink4(); },
		[this] { return this->FormatTramsitByteCountLink5(); },
		[this] { return this->FormatTramsitByteCountLink6(); },
		[this] { return this->FormatTramsitByteCountLink7(); },
		[this] { return this->FormatTransmitPacketCountLink0(); },
		[this] { return this->FormatTransmitPacketCountLink1(); },
		[this] { return this->FormatTransmitPacketCountLink2(); },
		[this] { return this->FormatTransmitPacketCountLink3(); },
		[this] { return this->FormatTransmitPacketCountLink4(); },
		[this] { return this->FormatTransmitPacketCountLink5(); },
		[this] { return this->FormatTransmitPacketCountLink6(); },
		[this] { return this->FormatTransmitPacketCountLink7(); }};
};
}  // namespace CFOLib

#endif  // CFO_REGISTERS_H
