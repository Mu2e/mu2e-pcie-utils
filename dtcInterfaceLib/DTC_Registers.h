#ifndef DTC_REGISTERS_H
#define DTC_REGISTERS_H

// #include <bitset> // std::bitset
// #include <cstdint> // uint8_t, uint16_t
#include <functional>  // std::bind, std::function
#include <vector>      // std::vector

#include "CFOandDTC_Registers.h"
#include "artdaq-core-mu2e/Overlays/DTC_Types/DTC_EVBStatsType.h"

namespace DTCLib
{
enum DTC_Register : uint16_t
{
	DTCLIB_COMMON_REGISTERS,  // Moved here all registers in common with CFO

	DTC_Register_ROCEmulationEnable = 0x9110,

	// 0x913C Reserved
	DTC_Register_SERDES_RXCDRLockStatus = 0x9140,
	DTC_Register_DMATimeoutPreset       = 0x9144,
	DTC_Register_ROCReplyTimeout        = 0x9148,
	DTC_Register_ROCReplyTimeoutError   = 0x914C,
	// 0x9150 Reserved
	DTC_Register_EVBPartitionID            = 0x9154,
	DTC_Register_EVBConfiguration          = 0x9158,
	DTC_Register_EVBPacketControl          = 0x915C,
	DTC_Register_EVBStats                  = 0x9160,
	DTC_Register_SERDESClock_IICBusControl = 0x9164,
	// DTC_Register_DDRReferenceClockFrequency = 0x9170,
	// DTC_Register_DDRClock_IICBusControl = 0x9174,
	// DTC_Register_DDRClock_IICBusLow = 0x9178,
	// DTC_Register_DDRClock_IICBusHigh = 0x917C,
	// Reserved - formerly... DTC_Register_DDRWriteResponseTimer = 0x9180,
	DTC_Register_CFOEmulation_LoopbackDelayMeasure = 0x9184,
	// Reserved - formerly... DTC_Register_DataPendingTimer = 0x9188,
	// 0x918C Reserved
	DTC_Register_FIFOFullErrorFlag0             = 0x9190,
	DTC_Register_FIFOFullErrorFlag1             = 0x9194,
	DTC_Register_FIFOFullErrorFlag2             = 0x9198,
	DTC_Register_ReceivePacketError             = 0x919C,
	DTC_Register_CFOEmulation_TimestampLow      = 0x91A0,
	DTC_Register_CFOEmulation_TimestampHigh     = 0x91A4,
	DTC_Register_CFOEmulation_HeartbeatInterval = 0x91A8,
	DTC_Register_CFOEmulation_NumHeartbeats     = 0x91AC,
	DTC_Register_ROCEmulation_NumPacketsLinks10 = 0x91B0,
	DTC_Register_ROCEmulation_NumPacketsLinks32 = 0x91B4,
	DTC_Register_ROCEmulation_NumPacketsLinks54 = 0x91B8,
	DTC_Register_CFOEmulation_NumNullHeartbeats = 0x91BC,
	DTC_Register_CFOEmulation_EventMode1        = 0x91C0,
	DTC_Register_CFOEmulation_EventMode2        = 0x91C4,
	DTC_Register_CFOEventModeRequiredMask       = 0x91C8,
	DTC_Register_RXPacketCountErrorFlags        = 0x91CC,
	DTC_Register_DetEmulation_DMACount          = 0x91D0,
	DTC_Register_DetEmulation_DelayCount        = 0x91D4,
	DTC_Register_DetEmulation_Control0          = 0x91D8,
	DTC_Register_DetEmulation_Control1          = 0x91DC,
	DTC_Register_DetEmulation_DataStartAddress  = 0x91E0,
	DTC_Register_DetEmulation_DataEndAddress    = 0x91E4,
	DTC_Register_EthernetFramePayloadSize       = 0x91EC,
	DTC_Register_CFOEmulation_DataRequestDelay  = 0x91E8,
	DTC_Register_EthernetFrameMaxPayload        = 0x91EC,
	// 0x91F0 Reserved
	DTC_Register_CFOEmulation_40MHzClockMarkerInterval = 0x91F4,
	DTC_Register_CFOMarkerEnables                      = 0x91F8,
	DTC_Register_ROCFinishThreshold                    = 0x91FC,
	// Reserved - formerly... DTC_Register_ReceiveByteCount_Link0 = 0x9200,
	// Reserved - formerly... DTC_Register_ReceiveByteCount_Link1 = 0x9204,
	// Reserved - formerly... DTC_Register_ReceiveByteCount_Link2 = 0x9208,
	// Reserved - formerly... DTC_Register_ReceiveByteCount_Link3 = 0x920C,
	// Reserved - formerly... DTC_Register_ReceiveByteCount_Link4 = 0x9210,
	// Reserved - formerly... DTC_Register_ReceiveByteCount_Link5 = 0x9214,
	// Reserved - formerly... DTC_Register_ReceiveByteCount_CFOLink = 0x9218,
	// 0x921C Reserved
	// Reserved - formerly... DTC_Register_ReceivePacketCount_Link0 = 0x9220,
	// Reserved - formerly... DTC_Register_ReceivePacketCount_Link1 = 0x9224,
	// Reserved - formerly... DTC_Register_ReceivePacketCount_Link2 = 0x9228,
	// Reserved - formerly... DTC_Register_ReceivePacketCount_Link3 = 0x922C,
	// Reserved - formerly... DTC_Register_ReceivePacketCount_Link4 = 0x9230,
	// Reserved - formerly... DTC_Register_ReceivePacketCount_Link5 = 0x9234,
	// Reserved - formerly... DTC_Register_ReceivePacketCount_CFOLink = 0x9238,
	// 0x923C Reserved
	// Reserved - formerly... DTC_Register_TransmitByteCount_Link0 = 0x9240,
	// Reserved - formerly... DTC_Register_TransmitByteCount_Link1 = 0x9244,
	// Reserved - formerly... DTC_Register_TransmitByteCount_Link2 = 0x9248,
	// Reserved - formerly... DTC_Register_TransmitByteCount_Link3 = 0x924C,
	// Reserved - formerly... DTC_Register_TransmitByteCount_Link4 = 0x9250,
	// Reserved - formerly... DTC_Register_TransmitByteCount_Link5 = 0x9254,
	// Reserved - formerly... DTC_Register_TransmitByteCount_CFOLink = 0x9258,
	// 0x925C Reserved
	// Reserved - formerly... DTC_Register_TransmitPacketCount_Link0 = 0x9260,
	// Reserved - formerly... DTC_Register_TransmitPacketCount_Link1 = 0x9264,
	// Reserved - formerly... DTC_Register_TransmitPacketCount_Link2 = 0x9268,
	// Reserved - formerly... DTC_Register_TransmitPacketCount_Link3 = 0x926C,
	// Reserved - formerly... DTC_Register_TransmitPacketCount_Link4 = 0x9270,
	// Reserved - formerly... DTC_Register_TransmitPacketCount_Link5 = 0x9274,
	// Reserved - formerly... DTC_Register_TransmitPacketCount_CFOLink = 0x9278,
	// 0x927C Reserved
	// 0x9280 Reserved
	// 0x9290 Reserved
	// 0x92A0 Reserved
	DTC_Register_TXPRBSControl = 0x92B0,
	DTC_Register_RXPRBSControl = 0x92B4,
	// 0x92B8 Reserved
	// 0x92BC Reserved
	DTC_Register_EventModeLookupTableControl = 0x92C0,
	// 0x92C4-0x92EC Reserved
	DTC_Register_DD3TestRegister = 0x92F0,
	// 0x92F4 Reserved
	// 0x92F8 Reserved
	// 0x92FC Reserved
	DTC_Register_SERDESTXRXInvertEnable = 0x9300,
	// 0x9304 Reserved
	DTC_Register_JitterAttenuatorCSR = 0x9308,
	// 0x930C Reserved
	// 0x9310 Reserved
	DTC_Register_SFP_IICBusControl            = 0x9314,
	DTC_Register_SFP_IICBusLow                = 0x9318,
	DTC_Register_SFP_IICBusHigh               = 0x931C,
	DTC_Register_RetransmitRequestCount_Link0 = 0x9320,
	DTC_Register_RetransmitRequestCount_Link1 = 0x9324,
	DTC_Register_RetransmitRequestCount_Link2 = 0x9328,
	DTC_Register_RetransmitRequestCount_Link3 = 0x932C,
	DTC_Register_RetransmitRequestCount_Link4 = 0x9330,
	DTC_Register_RetransmitRequestCount_Link5 = 0x9334,
	// 0x9338 Reserved
	// 0x933C Reserved
	DTC_Register_MissedCFOPacketCount_Link0 = 0x9340,
	DTC_Register_MissedCFOPacketCount_Link1 = 0x9344,
	DTC_Register_MissedCFOPacketCount_Link2 = 0x9348,
	DTC_Register_MissedCFOPacketCount_Link3 = 0x934C,
	DTC_Register_MissedCFOPacketCount_Link4 = 0x9350,
	DTC_Register_MissedCFOPacketCount_Link5 = 0x9354,
	// 0x9358 Reserved
	// 0x935C Reserved
	DTC_Register_LocalFragmentDropCount        = 0x9360,
	DTC_Register_EVBSubEventReceiveTimerPreset = 0x9364,
	DTC_Register_EVBSERDESPRBSControlStatus    = 0x9368,
	// 0x936C Reserved
	DTC_Register_EventBuilderErrorFlags = 0x9370,
	DTC_Register_InputBufferErrorFlags  = 0x9374,
	DTC_Register_OutputBufferErrorFlags = 0x9378,
	DTC_Register_Link0ErrorFlags        = 0x9380,
	DTC_Register_Link1ErrorFlags        = 0x9384,
	DTC_Register_Link2ErrorFlags        = 0x9388,
	DTC_Register_Link3ErrorFlags        = 0x938C,
	DTC_Register_Link4ErrorFlags        = 0x9390,
	DTC_Register_Link5ErrorFlags        = 0x9394,
	DTC_Register_CFOLinkErrorFlags      = 0x9398,
	DTC_Register_LinkMuxErrorFlags      = 0x939C,
	DTC_Register_SFPControlStatus       = 0x93A4,
	// 0x93A8 Reserved
	// 0x93AC Reserved
	DTC_Register_RXCDRUnlockCount_Link0          = 0x93B0,
	DTC_Register_RXCDRUnlockCount_Link1          = 0x93B4,
	DTC_Register_RXCDRUnlockCount_Link2          = 0x93B8,
	DTC_Register_RXCDRUnlockCount_Link3          = 0x93BC,
	DTC_Register_RXCDRUnlockCount_Link4          = 0x93C0,
	DTC_Register_RXCDRUnlockCount_Link5          = 0x93C4,
	DTC_Register_RXCDRUnlockCount_CFOLink        = 0x93C8,
	DTC_Register_JitterAttenuatorLossOfLockCount = 0x93CC,
	DTC_Register_CFOLinkEventStartErrorCount     = 0x93D0,
	DTC_Register_CFOLink40MHzErrorCount          = 0x93D4,
	DTC_Register_InputBufferDropCount            = 0x93D8,
	DTC_Register_OutputBufferDropCount           = 0x93DC,
	DTC_Register_ROCDCSTimerPreset               = 0x93E0,
	DTC_Register_DataRequest_Low                 = 0x93F8,
	DTC_Register_DataRequest_High                = 0x93FC,
	// 0x93E4 - 0x93FC Reserved
	DTC_Register_FPGAProgramData       = 0x9400,
	DTC_Register_FPGAPROMProgramStatus = 0x9404,
	DTC_Register_FPGACoreAccess        = 0x9408,
	// 0x940C Reserved
	DTC_Register_SlowOpticalLinksDiag  = 0x9410,
	DTC_Register_DiagSERDESErrorEnable = 0x9414,
	DTC_Register_DiagSERDESPacket0     = 0x9418,
	DTC_Register_DiagSERDESPacket1     = 0x941C,
	DTC_Register_DiagSERDESPacket2     = 0x9420,
	DTC_Register_DiagSERDESPacket3     = 0x9424,
	DTC_Register_DiagSERDESPacket4     = 0x9428,
	DTC_Register_DiagSERDESPacket5     = 0x942C,
	// Reserved - formerly... DTC_Register_DDR3LinkBufferEmptyFlags0 = 0x9430,
	// Reserved - formerly... DTC_Register_DDR3LinkBufferEmptyFlags1 = 0x9434,
	// Reserved - formerly... DTC_Register_DDR3LinkBufferEmptyFlags2 = 0x9438,
	// Reserved - formerly... DTC_Register_DDR3LinkBufferEmptyFlags3 = 0x943C,
	// Reserved - formerly... DTC_Register_DDR3LinkBufferHalfFullFlags0 = 0x9440,
	// Reserved - formerly... DTC_Register_DDR3LinkBufferHalfFullFlags1 = 0x9444,
	// Reserved - formerly... DTC_Register_DDR3LinkBufferHalfFullFlags2 = 0x9448,
	// Reserved - formerly... DTC_Register_DDR3LinkBufferHalfFullFlags3 = 0x944C,
	// Reserved - formerly... DTC_Register_DDR3LinkBufferFullFlags0 = 0x9450,
	// Reserved - formerly... DTC_Register_DDR3LinkBufferFullFlags1 = 0x9454,
	// Reserved - formerly... DTC_Register_DDR3LinkBufferFullFlags2 = 0x9458,
	// Reserved - formerly... DTC_Register_DDR3LinkBufferFullFlags3 = 0x945C,
	// Reserved - formerly... DTC_Register_DDR3EVBBufferEmptyFlags0 = 0x9460,
	// Reserved - formerly... DTC_Register_DDR3EVBBufferEmptyFlags1 = 0x9464,
	// Reserved - formerly... DTC_Register_DDR3EVBBufferEmptyFlags2 = 0x9468,
	// Reserved - formerly... DTC_Register_DDR3EVBBufferEmptyFlags3 = 0x946C,
	// Reserved - formerly... DTC_Register_DDR3EVBBufferHalfFullFlags0 = 0x9470,
	// Reserved - formerly... DTC_Register_DDR3EVBBufferHalfFullFlags1 = 0x9474,
	// Reserved - formerly... DTC_Register_DDR3EVBBufferHalfFullFlags2 = 0x9478,
	// Reserved - formerly... DTC_Register_DDR3EVBBufferHalfFullFlags3 = 0x947C,
	// Reserved - formerly... DTC_Register_DDR3EVBBufferFullFlags0 = 0x9480,
	// Reserved - formerly... DTC_Register_DDR3EVBBufferFullFlags1 = 0x9484,
	// Reserved - formerly... DTC_Register_DDR3EVBBufferFullFlags2 = 0x9488,
	// Reserved - formerly... DTC_Register_DDR3EVBBufferFullFlags3 = 0x948C,
	DTC_Register_DataPendingDiagTimer_Link0 = 0x9490,
	DTC_Register_DataPendingDiagTimer_Link1 = 0x9494,
	DTC_Register_DataPendingDiagTimer_Link2 = 0x9498,
	DTC_Register_DataPendingDiagTimer_Link3 = 0x949C,
	DTC_Register_DataPendingDiagTimer_Link4 = 0x94A0,
	DTC_Register_DataPendingDiagTimer_Link5 = 0x94A4,
	// 0x94A8 Reserved
	// 0x94AC Reserved
	// Reserved - formerly... DTC_Register_ROCEmulator_InduceTimeoutError_Link0 = 0x94B0,
	// Reserved - formerly... DTC_Register_ROCEmulator_InduceTimeoutError_Link1 = 0x94B4,
	// Reserved - formerly... DTC_Register_ROCEmulator_InduceTimeoutError_Link2 = 0x94B8,
	// Reserved - formerly... DTC_Register_ROCEmulator_InduceTimeoutError_Link3 = 0x94BC,
	// Reserved - formerly... DTC_Register_ROCEmulator_InduceTimeoutError_Link4 = 0x94C0,
	// Reserved - formerly... DTC_Register_ROCEmulator_InduceTimeoutError_Link5 = 0x94C4,
	// 0x94C8 Reserved
	// 0x94CC Reserved
	// Reserved - formerly... DTC_Register_ROCEmulator_InduceExtraWordError_Link0 = 0x94D0,
	// Reserved - formerly... DTC_Register_ROCEmulator_InduceExtraWordError_Link1 = 0x94D4,
	// Reserved - formerly... DTC_Register_ROCEmulator_InduceExtraWordError_Link2 = 0x94D8,
	// Reserved - formerly... DTC_Register_ROCEmulator_InduceExtraWordError_Link3 = 0x94DC,
	// Reserved - formerly... DTC_Register_ROCEmulator_InduceExtraWordError_Link4 = 0x94E0,
	// Reserved - formerly... DTC_Register_ROCEmulator_InduceExtraWordError_Link5 = 0x94E4,
	// 0x94E8 - 0x94FC Reserved
	DTC_Register_SERDES_CharacterNotInTableErrorCount_Link0   = 0x9500,
	DTC_Register_SERDES_CharacterNotInTableErrorCount_Link1   = 0x9504,
	DTC_Register_SERDES_CharacterNotInTableErrorCount_Link2   = 0x9508,
	DTC_Register_SERDES_CharacterNotInTableErrorCount_Link3   = 0x950C,
	DTC_Register_SERDES_CharacterNotInTableErrorCount_Link4   = 0x9510,
	DTC_Register_SERDES_CharacterNotInTableErrorCount_Link5   = 0x9514,
	DTC_Register_SERDES_CharacterNotInTableErrorCount_CFOLink = 0x9518,
	// 0x951C Reserved
	DTC_Register_SERDES_RXDisparityErrorCount_Link0   = 0x9520,
	DTC_Register_SERDES_RXDisparityErrorCount_Link1   = 0x9524,
	DTC_Register_SERDES_RXDisparityErrorCount_Link2   = 0x9528,
	DTC_Register_SERDES_RXDisparityErrorCount_Link3   = 0x952C,
	DTC_Register_SERDES_RXDisparityErrorCount_Link4   = 0x9530,
	DTC_Register_SERDES_RXDisparityErrorCount_Link5   = 0x9534,
	DTC_Register_SERDES_RXDisparityErrorCount_CFOLink = 0x9538,
	// 0x953C Reserved
	DTC_Register_SERDES_RXPRBSErrorCount_Link0   = 0x9540,
	DTC_Register_SERDES_RXPRBSErrorCount_Link1   = 0x9544,
	DTC_Register_SERDES_RXPRBSErrorCount_Link2   = 0x9548,
	DTC_Register_SERDES_RXPRBSErrorCount_Link3   = 0x954C,
	DTC_Register_SERDES_RXPRBSErrorCount_Link4   = 0x9550,
	DTC_Register_SERDES_RXPRBSErrorCount_Link5   = 0x9554,
	DTC_Register_SERDES_RXPRBSErrorCount_CFOLink = 0x9558,
	// 0x955C Reserved
	DTC_Register_SERDES_RXCRCErrorCount_Link0   = 0x9560,
	DTC_Register_SERDES_RXCRCErrorCount_Link1   = 0x9564,
	DTC_Register_SERDES_RXCRCErrorCount_Link2   = 0x9568,
	DTC_Register_SERDES_RXCRCErrorCount_Link3   = 0x956C,
	DTC_Register_SERDES_RXCRCErrorCount_Link4   = 0x9570,
	DTC_Register_SERDES_RXCRCErrorCount_Link5   = 0x9574,
	DTC_Register_SERDES_RXCRCErrorCount_CFOLink = 0x9578,
	DTC_Register_SERDES_RXCRCErrorControl       = 0x957C,
	// 0x9580 - 0x958C Reserved
	DTC_Register_EBVSERDES_RXPacketErrorCount = 0x9590,
	// 0x9594 - 0x959C Reserved
	DTC_Register_JitterAttenuator_SERDES_RXRecoveredClockLOSCount = 0x95A0,
	DTC_Register_JitterAttenuator_SERDES_RXExternalClockLOSCount  = 0x95A4,
	// 0x9598 - 0x960C Reserved
	DTC_Register_ROCEmulator_InterpacketDelay_Link0 = 0x9610,
	// Reserved - formerly... DTC_Register_ROCEmulator_InterpacketDelay_Link1 = 0x9614,
	// Reserved - formerly... DTC_Register_ROCEmulator_InterpacketDelay_Link2 = 0x9618,
	// Reserved - formerly... DTC_Register_ROCEmulator_InterpacketDelay_Link3 = 0x961C,
	// Reserved - formerly... DTC_Register_ROCEmulator_InterpacketDelay_Link4 = 0x9620,
	// Reserved - formerly... DTC_Register_ROCEmulator_InterpacketDelay_Link5 = 0x9624,
	// 0x9628 Reserved
	// 0x962C Reserved
	DTC_Register_TXDataRequestPacketCount_Link0 = 0x9630,
	DTC_Register_TXDataRequestPacketCount_Link1 = 0x9634,
	DTC_Register_TXDataRequestPacketCount_Link2 = 0x9638,
	DTC_Register_TXDataRequestPacketCount_Link3 = 0x963C,
	DTC_Register_TXDataRequestPacketCount_Link4 = 0x9640,
	DTC_Register_TXDataRequestPacketCount_Link5 = 0x9644,
	DTC_Register_CFOTXClockMarkerCount_Link6    = 0x9648,
	// 0x9648 Reserved
	// 0x964C Reserved
	DTC_Register_TXHeartbeatPacketCount_Link0    = 0x9650,
	DTC_Register_TXHeartbeatPacketCount_Link1    = 0x9654,
	DTC_Register_TXHeartbeatPacketCount_Link2    = 0x9658,
	DTC_Register_TXHeartbeatPacketCount_Link3    = 0x965C,
	DTC_Register_TXHeartbeatPacketCount_Link4    = 0x9660,
	DTC_Register_TXHeartbeatPacketCount_Link5    = 0x9664,
	DTC_Register_CFOTXHeartbeatPacketCount_Link5 = 0x9668,
	// 0x9668 Reserved
	// 0x966C Reserved
	DTC_Register_RXDataHeaderPacketCount_Link0 = 0x9670,
	DTC_Register_RXDataHeaderPacketCount_Link1 = 0x9674,
	DTC_Register_RXDataHeaderPacketCount_Link2 = 0x9678,
	DTC_Register_RXDataHeaderPacketCount_Link3 = 0x967C,
	DTC_Register_RXDataHeaderPacketCount_Link4 = 0x9680,
	DTC_Register_RXDataHeaderPacketCount_Link5 = 0x9684,
	// 0x9688 Reserved
	// 0x968C Reserved
	DTC_Register_RXDataPacketCount_Link0 = 0x9690,
	DTC_Register_RXDataPacketCount_Link1 = 0x9694,
	DTC_Register_RXDataPacketCount_Link2 = 0x9698,
	DTC_Register_RXDataPacketCount_Link3 = 0x969C,
	DTC_Register_RXDataPacketCount_Link4 = 0x96A0,
	DTC_Register_RXDataPacketCount_Link5 = 0x96A4,
	// 0x96A8 Reserved
	// 0x96AC Reserved
	DTC_Register_EVBDiagnosticRXPacket_Low  = 0x96B0,
	DTC_Register_EVBDiagnosticRXPacket_High = 0x96B4,
	DTC_Register_EventModeLookupTableStart  = 0xA000,
	DTC_Register_EventModeLookupTableEnd    = 0xA3FC,

	DTC_Register_RXDataDiagnosticFIFO_Link0 = 0x96D0,
	DTC_Register_RXDataDiagnosticFIFO_Link1 = 0x96D4,
	DTC_Register_RXDataDiagnosticFIFO_Link2 = 0x96D8,
	DTC_Register_RXDataDiagnosticFIFO_Link3 = 0x96DC,
	DTC_Register_RXDataDiagnosticFIFO_Link4 = 0x96E0,
	DTC_Register_RXDataDiagnosticFIFO_Link5 = 0x96E4,

	DTC_Register_TXDataDiagnosticFIFO_Link0 = 0x96E8,
	DTC_Register_TXDataDiagnosticFIFO_Link1 = 0x96EC,
	DTC_Register_TXDataDiagnosticFIFO_Link2 = 0x96F0,
	DTC_Register_TXDataDiagnosticFIFO_Link3 = 0x96F4,
	DTC_Register_TXDataDiagnosticFIFO_Link4 = 0x96F8,
	DTC_Register_TXDataDiagnosticFIFO_Link5 = 0x96FC,

	DTC_Register_RXDataDiagnosticFIFO_LinkCFO = 0x9700,
	DTC_Register_TXDataDiagnosticFIFO_LinkCFO = 0x9708,

	DTC_Register_TXEventWindowMarkerCount_Link0    = 0xA400,
	DTC_Register_TXEventWindowMarkerCount_Link1    = 0xA404,
	DTC_Register_TXEventWindowMarkerCount_Link2    = 0xA408,
	DTC_Register_TXEventWindowMarkerCount_Link3    = 0xA40C,
	DTC_Register_TXEventWindowMarkerCount_Link4    = 0xA410,
	DTC_Register_TXEventWindowMarkerCount_Link5    = 0xA414,
	DTC_Register_CFOTXEventWindowMarkerCount_Link6 = 0xA418,

	DTC_Register_Invalid,
	// };
};  // end DTC_Register enum

/// <summary>
/// The DTC_Registers class represents the DTC Register space, and all the methods necessary to read and write those
/// registers. Each register has, at the very least, a read method, a write method, and a RegisterFormatter method
/// which formats the register value in a human-readable way.
/// </summary>
class DTC_Registers : public CFOandDTC_Registers
{
  public:
	explicit DTC_Registers(DTC_SimMode mode, int dtc, std::string simFileName, unsigned linkMask = 0x1, std::string expectedDesignVersion = "", bool skipInit = false, const std::string& uid = "");

	virtual ~DTC_Registers();

	//
	// DTC Sim Mode Virtual Register
	//
	/// <summary>
	/// Get the current DTC_SimMode of this DTC_Registers object
	/// </summary>
	/// <returns></returns>
	DTC_SimMode GetSimMode() const { return simMode_; }

	DTC_SimMode SetSimMode(std::string expectedDesignVersion, DTC_SimMode mode, int dtc, std::string simMemoryFile, unsigned linkMask, bool skipInit = false, const std::string& uid = "");

	//
	// Register IO Functions
	//

	// Design Status Register
	bool              ReadDDRInterfaceReset(std::optional<uint32_t> val = std::nullopt);
	void              SetDDRInterfaceReset(bool reset);
	void              ResetDDRInterface();
	bool              ReadDDRAutoCalibrationDone(std::optional<uint32_t> val = std::nullopt);
	RegisterFormatter FormatDesignStatus();

	// DTC Control Register
	void EnableCFOEmulation();                                                 // B30
	void DisableCFOEmulation();                                                // B30
	bool ReadCFOEmulationEnabled(std::optional<uint32_t> val = std::nullopt);  // B30
	// Bit 29 Reserved
	void         EnableCFOLoopback();                                                   // B28
	void         DisableCFOLoopback();                                                  // B28
	bool         ReadCFOLoopback(std::optional<uint32_t> val = std::nullopt);           // B28
	void         ResetDDRWriteAddress();                                                // B27
	bool         ReadResetDDRWriteAddress(std::optional<uint32_t> val = std::nullopt);  // B27
	void         ResetDDRReadAddress();                                                 // B26
	bool         ReadResetDDRReadAddress(std::optional<uint32_t> val = std::nullopt);   // B26
	void         ResetDDR();                                                            // B25
	bool         ReadResetDDR(std::optional<uint32_t> val = std::nullopt);              // B25
	void         EnableCFOEmulatorDRP();                                                // B24
	void         DisableCFOEmulatorDRP();                                               // B24
	bool         ReadCFOEmulatorDRP(std::optional<uint32_t> val = std::nullopt);        // B24
	void         EnableAutogenDRP();                                                    // B23
	void         DisableAutogenDRP();                                                   // B23
	bool         ReadAutogenDRP(std::optional<uint32_t> val = std::nullopt);            // B23
	void         EnableSoftwareDRP();                                                   // alias to  DisableAutogenDRP()
	void         DisableSoftwareDRP();                                                  // B23
	bool         ReadSoftwareDRP(std::optional<uint32_t> val = std::nullopt);			// alias to  EnableAutogenDRP()
	void		 EnableKillTimeoutROCs();                                               // B22
	void		 DisableKillTimeoutROCs();												// B22
	bool         ReadKillTimeoutROCs(std::optional<uint32_t> val = std::nullopt);   	// B22
	virtual void ResetPCIe() override;                                       // B21
	bool         ReadResetPCIe(std::optional<uint32_t> val = std::nullopt);  // B21
	// Bit 20 Reserved
	void         EnableDownLED0();                                               // B19
	void         DisableDownLED0();                                              // B19
	bool         ReadDownLED0State(std::optional<uint32_t> val = std::nullopt);  // B19
	void         EnableUpLED1();                                                 // B18
	void         DisableUpLED1();                                                // B18
	bool         ReadUpLED1State(std::optional<uint32_t> val = std::nullopt);    // B18
	void         EnableUpLED0();                                                 // B17
	void         DisableUpLED0();                                                // B17
	bool         ReadUpLED0State(std::optional<uint32_t> val = std::nullopt);    // B17
	void         EnableLED6();                                                   // B16
	void         DisableLED6();                                                  // B16
	bool         ReadLED6State(std::optional<uint32_t> val = std::nullopt);      // B16
	void         EnableLEDs();
	void         DisableLEDs();
	virtual void FlashLEDs() override;

	void SetCFOEmulationMode();                                             // B15
	void ClearCFOEmulationMode();                                           // B15
	bool ReadCFOEmulationMode(std::optional<uint32_t> val = std::nullopt);  // B15
	// Bit 14 Reserved
	void SetDataFilterEnable();                                                           // B13
	void ClearDataFilterEnable();                                                         // B13
	bool ReadDataFilterEnable(std::optional<uint32_t> val = std::nullopt);                // B13
	void SetDRPPrefetchEnable();                                                          // B12
	void ClearDRPPrefetchEnable();                                                        // B12
	bool ReadDRPPrefetchEnable(std::optional<uint32_t> val = std::nullopt);               // B12
	void ROCInterfaceSoftReset();                                                         // B11
	bool ReadROCInterfaceSoftReset(std::optional<uint32_t> val = std::nullopt);           // B11
	void EnableDropDataToEmulateEventBuilding();                                          // B10
	void DisableDropDataToEmulateEventBuilding();                                         // B10
	bool ReadDropDataToEmulateEventBuilding(std::optional<uint32_t> val = std::nullopt);  // B10
	void SetPunchEnable();                                                                // B9
	void ClearPunchEnable();                                                              // B9
	bool ReadPunchEnable(std::optional<uint32_t> val = std::nullopt);                     // B9
	// void ResetSERDES();                       // B8 implemented for CFO and DTC
	// bool ReadResetSERDES(std::optional<uint32_t> val = std::nullopt);                   // B8 implemented for CFO and DTC
	void SetExternalCFOSampleEdgeMode(int forceCFOedge);                    // B6:5
	int  ReadExternalCFOSampleEdgeMode(std::optional<uint32_t> val);        // B6:5
	void SetExternalFanoutClockInput();                                     // B4
	void SetInternalFanoutClockInput();                                     // B4
	bool ReadFanoutClockInput(std::optional<uint32_t> val = std::nullopt);  // B4
	// void RunCFOEmulatorLoopbackTest();   // B3 implemented for CFO and DTC
	void EnableDCSReception();                                          // B2
	void DisableDCSReception();                                         // B2
	bool ReadDCSReception(std::optional<uint32_t> val = std::nullopt);  // B2
	// Bit 1 Reserved
	// Bit 0 Reserved
	RegisterFormatter FormatDTCControl();

	void SetCFOSamplePermanentOffset(int permanentOffset);
	int  ReadCFOSamplePermanentOffset(std::optional<uint32_t> val = std::nullopt);

	// DMA Transfer Length Register
	void              SetTriggerDMATransferLength(uint16_t length);
	uint16_t          ReadTriggerDMATransferLength(std::optional<uint32_t> val = std::nullopt);
	void              SetMinDMATransferLength(uint16_t length);
	uint16_t          ReadMinDMATransferLength(std::optional<uint32_t> val = std::nullopt);
	RegisterFormatter FormatDMATransferLength();

	// SERDES Loopback Enable Register
	void                   SetSERDESLoopbackMode(DTC_Link_ID const& link, const DTC_SERDESLoopbackMode& mode);
	DTC_SERDESLoopbackMode ReadSERDESLoopback(DTC_Link_ID const& link, std::optional<uint32_t> val = std::nullopt);
	RegisterFormatter      FormatSERDESLoopbackEnable();

	// Clock Status Register
	bool              ReadSERDESOscillatorIICError(std::optional<uint32_t> val = std::nullopt);
	bool              ReadDDROscillatorIICError(std::optional<uint32_t> val = std::nullopt);
	RegisterFormatter FormatClockOscillatorStatus();

	// ROC Emulation Enable Register
	void              EnableROCEmulator(DTC_Link_ID const& link, DTC_ROC_Emulation_Type const& type = DTC_ROC_Emulation_Type::ROC_Internal_Emulation);
	void              DisableROCEmulator(DTC_Link_ID const& link, DTC_ROC_Emulation_Type const& type = DTC_ROC_Emulation_Type::ROC_Internal_Emulation);
	bool              ReadROCEmulator(DTC_Link_ID const& link, DTC_ROC_Emulation_Type const& type = DTC_ROC_Emulation_Type::ROC_Internal_Emulation, std::optional<uint32_t> val = std::nullopt);
	void              SetROCEmulatorMask(uint32_t rocEnableMask);
	uint32_t          ReadROCEmulatorMask(std::optional<uint32_t> val = std::nullopt);
	RegisterFormatter FormatROCEmulationEnable();

	// Link Enable Register
	void               EnableReceiveCFOLink();
	void               DisableReceiveCFOLink();
	void               EnableTransmitCFOLink();
	void               DisableTransmitCFOLink();
	void               EnableLink(DTC_Link_ID const& link, const DTC_LinkEnableMode& mode = DTC_LinkEnableMode());
	void               DisableLink(DTC_Link_ID const& link, const DTC_LinkEnableMode& mode = DTC_LinkEnableMode());
	DTC_LinkEnableMode ReadLinkEnabled(DTC_Link_ID const& link, std::optional<uint32_t> val = std::nullopt);
	uint32_t           ReadLinkEnabledData();
	RegisterFormatter  FormatLinkEnable();

	// SERDES Reset Register
	void              ResetSERDESTX(DTC_Link_ID const& link, int interval = 100000);
	bool              ReadResetSERDESTX(DTC_Link_ID const& link, std::optional<uint32_t> val = std::nullopt);
	void              ResetSERDESRX(DTC_Link_ID const& link, int interval = 100000);
	bool              ReadResetSERDESRX(DTC_Link_ID const& link, std::optional<uint32_t> val = std::nullopt);
	void              ResetSERDESPLL(const DTC_PLL_ID& pll, int interval = 100000);
	bool              ReadResetSERDESPLL(const DTC_PLL_ID& pll, std::optional<uint32_t> val = std::nullopt);
	void              ResetSERDES(DTC_Link_ID const& link, int interval = 100000);
	bool              ReadResetSERDES(DTC_Link_ID const& link, std::optional<uint32_t> val = std::nullopt);
	RegisterFormatter FormatSERDESReset();

	// Link Diagnostic FIFOs
	DTCLib::RegisterFormatter FormatRXDiagFifo(DTC_Link_ID const& link);
	DTCLib::RegisterFormatter FormatTXDiagFifo(DTC_Link_ID const& link);

	// SERDES RX Disparity Error Register
	DTC_SERDESRXDisparityError ReadSERDESRXDisparityError(DTC_Link_ID const& link, std::optional<uint32_t> val = std::nullopt);
	RegisterFormatter          FormatSERDESRXDisparityError();

	// SERDES Character Not In Table Error Register
	DTC_CharacterNotInTableError ReadSERDESRXCharacterNotInTableError(DTC_Link_ID const& link, std::optional<uint32_t> val = std::nullopt);
	RegisterFormatter            FormatSERDESRXCharacterNotInTableError();

	// SERDES Unlock Error Register
	bool              ReadSERDESCDRUnlockError(DTC_Link_ID const& link, std::optional<uint32_t> val = std::nullopt);
	bool              ReadSERDESPLLUnlockError(const DTC_PLL_ID& pll, std::optional<uint32_t> val = std::nullopt);
	RegisterFormatter FormatSERDESUnlockError();

	// SERDES PLL Locked Register
	bool              ReadSERDESPLLLocked(DTC_Link_ID const& link, std::optional<uint32_t> val = std::nullopt);
	RegisterFormatter FormatSERDESPLLLocked();

	// SERDES PLL Power Down
	void              EnableSERDESPLL(DTC_Link_ID const& link);
	void              DisableSERDESPLL(DTC_Link_ID const& link);
	bool              ReadSERDESPLLPowerDown(DTC_Link_ID const& link, std::optional<uint32_t> val = std::nullopt);
	RegisterFormatter FormatSERDESPLLPowerDown();

	// SERDES RX Status Register
	DTC_RXStatus      ReadSERDESRXStatus(DTC_Link_ID const& link, std::optional<uint32_t> val = std::nullopt);
	RegisterFormatter FormatSERDESRXStatus();

	// SERDES Reset Done Register
	bool              ReadResetRXFSMSERDESDone(DTC_Link_ID const& link, std::optional<uint32_t> val = std::nullopt);
	bool              ReadResetRXSERDESDone(DTC_Link_ID const& link, std::optional<uint32_t> val = std::nullopt);
	bool              ReadResetTXFSMSERDESDone(DTC_Link_ID const& link, std::optional<uint32_t> val = std::nullopt);
	bool              ReadResetTXSERDESDone(DTC_Link_ID const& link, std::optional<uint32_t> val = std::nullopt);
	RegisterFormatter FormatSERDESResetDone();

	// SERDES CDR Lock Register
	bool              ReadSERDESRXCDRLock(DTC_Link_ID const& link, std::optional<uint32_t> val = std::nullopt);
	RegisterFormatter FormatRXCDRLockStatus();

	// DMA Timeout Preset Regsiter
	void              SetDMATimeoutPreset(uint32_t preset);
	uint32_t          ReadDMATimeoutPreset(std::optional<uint32_t> val = std::nullopt);
	RegisterFormatter FormatDMATimeoutPreset();

	// ROC Timeout (Header Packet to All Packets Received) Preset Register
	void              SetROCTimeoutPreset(uint32_t preset);
	uint32_t          ReadROCTimeoutPreset(std::optional<uint32_t> val = std::nullopt);
	RegisterFormatter FormatROCReplyTimeout();

	// ROC Timeout Error Register
	void              ClearROCTimeoutError(DTC_Link_ID const& link);
	bool              ReadROCTimeoutError(DTC_Link_ID const& link, std::optional<uint32_t> val = std::nullopt);
	RegisterFormatter FormatROCReplyTimeoutError();

	//----------------- Hardware Event Building configuration -----------------------

	// EVB Network Partition ID / EVB Network Local MAC Index Register
	void              SetEVBInfo(uint8_t dtcid, uint8_t mode, uint8_t partitionId, uint8_t macByte);
	void              SetDTCID(uint8_t dtcid);
	uint8_t           ReadDTCID(std::optional<uint32_t> val = std::nullopt);
	void              SetEVBMode(uint8_t mode);
	uint8_t           ReadEVBMode(std::optional<uint32_t> val = std::nullopt);
	void              SetEVBLocalParitionID(uint8_t id);
	uint8_t           ReadEVBLocalParitionID(std::optional<uint32_t> val = std::nullopt);
	void              SetEVBLocalMACAddress(uint8_t macByte);
	uint8_t           ReadEVBLocalMACAddress(std::optional<uint32_t> val = std::nullopt);
	RegisterFormatter FormatEVBLocalParitionIDMACIndex();

	// EVB Cluster Config
	void              SetEVBClusterInfo(uint16_t deadTime, uint8_t baseDTCAddress, uint8_t numOfDTCs);
	void              SetEVBDeadTime(uint16_t deadTime);
	uint16_t          ReadEVBDeadTime(std::optional<uint32_t> val = std::nullopt);
	void              SetEVBStartNode(uint8_t node);
	uint8_t           ReadEVBStartNode(std::optional<uint32_t> val = std::nullopt);
	void              SetEVBNumberOfDestinationNodes(uint8_t number);
	uint8_t           ReadEVBNumberOfDestinationNodes(std::optional<uint32_t> val = std::nullopt);
	RegisterFormatter FormatEVBClusterInfo();

	// EVB Packet Control
	void              SetEVBPacketControlInfo(uint16_t idlePacketWordCount, uint8_t interpacketGap, uint8_t loopbackCalibratedOffset);
	uint16_t          ReadEVBIdlePacketWordCount(std::optional<uint32_t> val = std::nullopt);
	uint8_t           ReadEVBInterpacketGap(std::optional<uint32_t> val = std::nullopt);
	uint8_t           ReadEVBLoopbackCalibratedOffset(std::optional<uint32_t> val = std::nullopt);
	RegisterFormatter FormatEVBPacketControlInfo();

	// EVB Stats
	uint32_t                                           ReadEVBStats(DTC_EVBStatsType type, uint8_t dtc_mac, std::optional<uint32_t> val = std::nullopt);
	RegisterFormatter                                  FormatEVBStats(DTC_EVBStatsType type = DTC_EVBStatsType::DTC_EVBStatsType_All);
	std::chrono::time_point<std::chrono::steady_clock> EVB_startDataTime, EVB_endDataTime;

	//----------------- end Hardware Event Building configuration -----------------------

	// SERDES Oscillator Registers
	bool ReadSERDESOscillatorIICInterfaceReset(std::optional<uint32_t> val = std::nullopt);
	void ResetSERDESOscillatorIICInterface();

	DTC_SerdesClockSpeed ReadSERDESOscillatorClock(std::optional<uint32_t> val = std::nullopt);
	void                 SetSERDESOscillatorClock(DTC_SerdesClockSpeed speed);
	void                 SetTimingOscillatorClock(uint32_t freq);
	RegisterFormatter    FormatSERDESOscillatorControl();

	// FIFO Full Error Flags Registers
	void                   ClearFIFOFullErrorFlags(DTC_Link_ID const& link);
	DTC_FIFOFullErrorFlags ReadFIFOFullErrorFlags(DTC_Link_ID const& link, std::optional<uint32_t> val0 = std::nullopt, std::optional<uint32_t> val1 = std::nullopt, std::optional<uint32_t> val2 = std::nullopt);
	RegisterFormatter      FormatFIFOFullErrorFlag0();
	RegisterFormatter      FormatFIFOFullErrorFlag1();
	RegisterFormatter      FormatFIFOFullErrorFlag2();

	// Receive Packet Error Register
	void              ClearPacketError(DTC_Link_ID const& link);
	bool              ReadPacketError(DTC_Link_ID const& link, std::optional<uint32_t> val = std::nullopt);
	void              ClearPacketCRCError(DTC_Link_ID const& link);
	bool              ReadPacketCRCError(DTC_Link_ID const& link, std::optional<uint32_t> val = std::nullopt);
	RegisterFormatter FormatReceivePacketError();

	// CFO Emulation Timestamp Registers
	void               SetCFOEmulationTimestamp(const DTC_EventWindowTag& ts);
	DTC_EventWindowTag ReadCFOEmulationTimestamp(std::optional<uint32_t> val = std::nullopt);
	RegisterFormatter  FormatCFOEmulationTimestampLow();
	RegisterFormatter  FormatCFOEmulationTimestampHigh();

	// CFO Emulation Heartbeat Interval Regsister
	void              SetCFOEmulationEventWindowInterval(uint32_t interval);
	uint32_t          ReadCFOEmulationEventWindowInterval(std::optional<uint32_t> val = std::nullopt);
	RegisterFormatter FormatCFOEmulationEventWindowInterval();

	// CFO Emulation Number of Heartbeats Register
	void              SetCFOEmulationNumHeartbeats(uint32_t numHeartbeats);
	uint32_t          ReadCFOEmulationNumHeartbeats(std::optional<uint32_t> val = std::nullopt);
	RegisterFormatter FormatCFOEmulationNumHeartbeats();

	// CFO Emulation Number of Packets Registers
	void              SetROCEmulationNumPackets(DTC_Link_ID const& link, uint16_t numPackets);
	uint16_t          ReadROCEmulationNumPackets(DTC_Link_ID const& link, std::optional<uint32_t> val = std::nullopt);
	RegisterFormatter FormatROCEmulationNumPacketsLink01();
	RegisterFormatter FormatROCEmulationNumPacketsLink23();
	RegisterFormatter FormatROCEmulationNumPacketsLink45();

	// CFO Emulation Loopback Delay Measure
	uint32_t          ReadCFOEmulationLoopbackDelayMeasure(std::optional<uint32_t> val = std::nullopt);
	RegisterFormatter FormatCFOEmulationLoopbackDelayMeasure();

	// CFO Emulation Number of Null Heartbeats Register
	void              SetCFOEmulationNumNullHeartbeats(const uint32_t& count);
	uint32_t          ReadCFOEmulationNumNullHeartbeats(std::optional<uint32_t> val = std::nullopt);
	RegisterFormatter FormatCFOEmulationNumNullHeartbeats();

	// CFO Event Mode Required Mask (required bits in event mode for CFO emulation to send a heartbeat. 1 uses them)
	// The bits that are required. If a mask-bit is 0, it's accepted anyway.
	// If a mask-bit is 1, then the eventMode-bit also needs to be 1 to be accepted.
	void              SetCFOEventModeRequiredMask(const uint32_t& mask);
	uint32_t          ReadCFOEventModeRequiredMask(std::optional<uint32_t> val = std::nullopt);
	RegisterFormatter FormatCFOEventModeRequiredMask();

	// CFO Emulation Event Mode Bytes Registers
	void              SetCFOEmulationEventMode(const uint64_t& eventMode);
	uint64_t          ReadCFOEmulationEventMode();
	void              SetCFOEmulationModeByte(const uint8_t& byteNum, uint8_t data);
	uint8_t           ReadCFOEmulationModeByte(const uint8_t& byteNum, std::optional<uint32_t> val = std::nullopt);
	RegisterFormatter FormatCFOEmulationModeBytes03();
	RegisterFormatter FormatCFOEmulationModeBytes45();

	// CFO Emulation Debug Packet Type Register
	// void EnableDebugPacketMode();
	// void DisableDebugPacketMode();
	// bool ReadDebugPacketMode(std::optional<uint32_t> val = std::nullopt);
	// void SetCFOEmulationDebugType(DTC_DebugType type);
	// DTC_DebugType ReadCFOEmulationDebugType(std::optional<uint32_t> val = std::nullopt);
	// RegisterFormatter FormatCFOEmulationDebugPacketType();

	// RX Packet Count Error Flags Register
	bool              ReadRXPacketCountErrorFlags(DTC_Link_ID const& link, std::optional<uint32_t> val = std::nullopt);
	void              ClearRXPacketCountErrorFlags(DTC_Link_ID const& link);
	void              ClearRXPacketCountErrorFlags();
	RegisterFormatter FormatRXPacketCountErrorFlags();

	// Detector Emulation DMA Count Register
	void              SetDetectorEmulationDMACount(uint32_t count);
	uint32_t          ReadDetectorEmulationDMACount(std::optional<uint32_t> val = std::nullopt);
	RegisterFormatter FormatDetectorEmulationDMACount();

	// Detector Emulation DMA Delay Count Register
	void              SetDetectorEmulationDMADelayCount(uint32_t count);
	uint32_t          ReadDetectorEmulationDMADelayCount(std::optional<uint32_t> val = std::nullopt);
	RegisterFormatter FormatDetectorEmulationDMADelayCount();

	// Detector Emulation Control Registers
	void EnableDetectorEmulatorMode();
	void DisableDetectorEmulatorMode();
	bool ReadDetectorEmulatorMode(std::optional<uint32_t> val = std::nullopt);
	void EnableDetectorEmulator();
	void DisableDetectorEmulator();
	bool ReadDetectorEmulatorEnable(std::optional<uint32_t> val = std::nullopt);
	bool ReadDetectorEmulatorEnableClear(std::optional<uint32_t> val = std::nullopt);
	/// <summary>
	/// Return the current value of the "Detector Emulator In Use" virtual register
	/// </summary>
	/// <returns>Whether the DTC Detector Emulator has been set up</returns>
	bool IsDetectorEmulatorInUse() const { return usingDetectorEmulator_; }
	/// <summary>
	/// Set the "Detector Emulator In Use" virtual register to true
	/// </summary>
	void SetDetectorEmulatorInUse()
	{
#ifndef __CLING__
		TLOG(TLVL_WARNING) << "DTC_Registers::SetDetectorEmulatorInUse: Enabling Detector Emulator!";
#endif
		usingDetectorEmulator_ = true;
	}
	void              ClearDetectorEmulatorInUse();
	RegisterFormatter FormatDetectorEmulationControl0();
	RegisterFormatter FormatDetectorEmulationControl1();

	// DDR Event Data Local Start Address Register
	void              SetDDRDataLocalStartAddress(uint32_t address);
	uint32_t          ReadDDRDataLocalStartAddress(std::optional<uint32_t> val = std::nullopt);
	RegisterFormatter FormatDDRDataLocalStartAddress();

	// DDR Event Data Local End Address Register
	void              SetDDRDataLocalEndAddress(uint32_t address);
	uint32_t          ReadDDRDataLocalEndAddress(std::optional<uint32_t> val = std::nullopt);
	RegisterFormatter FormatDDRDataLocalEndAddress();

	// CFO Emulator Data Request Interpacket Delay
	uint32_t          ReadCFOEmulatorInterpacketDelay(std::optional<uint32_t> val = std::nullopt);
	RegisterFormatter FormatCFOEmulatorInterpacketDelay();

	// Ethernet Frame Payload Max Size
	uint32_t          ReadEthernetPayloadSize(std::optional<uint32_t> val = std::nullopt);
	void              SetEthernetPayloadSize(uint32_t size);
	RegisterFormatter FormatEthernetPayloadSize();

	// CFO Emulation 40 MHz Clock Marker Interval
	uint32_t          ReadCFOEmulation40MHzMarkerInterval(std::optional<uint32_t> val = std::nullopt);
	void              SetCFOEmulation40MHzMarkerInterval(uint32_t interval);
	RegisterFormatter FormatCFOEmulation40MHzMarkerInterval();

	// CFO Emulation Marker Enables
	// bool ReadCFOEmulationEventStartMarkerEnable(DTC_Link_ID const& link, std::optional<uint32_t> val = std::nullopt);
	// void SetCFOEmulationEventStartMarkerEnable(DTC_Link_ID const& link, bool enable);
	bool              ReadCFO40MHzClockMarkerEnable(DTC_Link_ID const& link, std::optional<uint32_t> val = std::nullopt);
	void              SetCFO40MHzClockMarkerEnable(DTC_Link_ID const& link, bool enable);
	RegisterFormatter FormatCFO40MHzClockMarkerEnables();

	// ROC Finish Threshold Register
	uint8_t           ReadROCCommaLimit(std::optional<uint32_t> val = std::nullopt);
	void              SetROCCommaLimit(uint8_t limit);
	RegisterFormatter FormatROCFinishThreshold();

	// SERDES TX PRBS Control
	bool              ReadTXPRBSForceError(DTC_Link_ID const& link, std::optional<uint32_t> val = std::nullopt);
	void              SetTXPRBSForceError(DTC_Link_ID const& link);
	void              ClearTXPRBSForceError(DTC_Link_ID const& link);
	DTC_PRBSMode      ReadTXPRBSMode(DTC_Link_ID const& link, std::optional<uint32_t> val = std::nullopt);
	void              SetTXPRBSMode(DTC_Link_ID const& link, DTC_PRBSMode mode);
	RegisterFormatter FormatSERDESTXPRBSControl();

	// SERDES RX PRBS Control
	bool              ReadRXPRBSError(DTC_Link_ID const& link, std::optional<uint32_t> val = std::nullopt);
	DTC_PRBSMode      ReadRXPRBSMode(DTC_Link_ID const& link, std::optional<uint32_t> val = std::nullopt);
	void              SetRXPRBSMode(DTC_Link_ID const& link, DTC_PRBSMode mode);
	RegisterFormatter FormatSERDESRXPRBSControl();

	// DTC Mode Lookup
	bool              ReadEventModeTableEnable(std::optional<uint32_t> val = std::nullopt);
	void              SetEventModeTableEnable();
	void              ClearEventModeTableEnable();
	uint8_t           ReadEventModeLookupByteSelect(std::optional<uint32_t> val = std::nullopt);
	void              SetEventModeLookupByteSelect(uint8_t byte);
	RegisterFormatter FormatEventModeLookupTableControl();

	// DDR Memory Test Register
	bool              ReadDDRMemoryTestComplete(std::optional<uint32_t> val = std::nullopt);
	bool              ReadDDRMemoryTestError(std::optional<uint32_t> val = std::nullopt);
	void              ClearDDRMemoryTestError();
	RegisterFormatter FormatDDRMemoryTestRegister();

	// SERDES Serial Inversion Enable Register
	bool              ReadInvertSERDESRXInput(DTC_Link_ID const& link, std::optional<uint32_t> val = std::nullopt);
	void              SetInvertSERDESRXInput(DTC_Link_ID const& link, bool invert);
	bool              ReadInvertSERDESTXOutput(DTC_Link_ID const& link, std::optional<uint32_t> val = std::nullopt);
	void              SetInvertSERDESTXOutput(DTC_Link_ID const& link, bool invert);
	RegisterFormatter FormatSERDESSerialInversionEnable();

	// Jitter Attenuator CSR Register
	virtual std::bitset<2>    ReadJitterAttenuatorSelect(std::optional<uint32_t> val = std::nullopt) override;
	virtual void              SetJitterAttenuatorSelect(std::bitset<2> data, bool alsoResetJA = false) override;
	virtual bool              ReadJitterAttenuatorReset(std::optional<uint32_t> val = std::nullopt) override;
	virtual bool              ReadJitterAttenuatorLocked(std::optional<uint32_t> val = std::nullopt) override;
	virtual void              ResetJitterAttenuator() override;
	virtual RegisterFormatter FormatJitterAttenuatorCSR() override;

	// void ConfigureJitterAttenuator();

	// SFP IIC Registers
	bool              ReadSFPIICInterfaceReset(std::optional<uint32_t> val = std::nullopt);
	void              ResetSFPIICInterface();
	void              WriteSFPIICInterface(uint8_t device, uint8_t address, uint8_t data);
	uint8_t           ReadSFPIICInterface(uint8_t device, uint8_t address);
	RegisterFormatter FormatSFPIICControl();
	RegisterFormatter FormatSFPIICParameterLow();
	RegisterFormatter FormatSFPIICParameterHigh();

	// Retransmit Request Count Registers
	uint32_t          ReadRetransmitRequestCount(DTC_Link_ID const& link, std::optional<uint32_t> val = std::nullopt);
	void              ClearRetransmitRequestCount(DTC_Link_ID const& link);
	RegisterFormatter FormatRetransmitRequestCountLink0();
	RegisterFormatter FormatRetransmitRequestCountLink1();
	RegisterFormatter FormatRetransmitRequestCountLink2();
	RegisterFormatter FormatRetransmitRequestCountLink3();
	RegisterFormatter FormatRetransmitRequestCountLink4();
	RegisterFormatter FormatRetransmitRequestCountLink5();

	// Missed CFO Packet Count Registers
	uint32_t          ReadMissedCFOPacketCount(DTC_Link_ID const& link, std::optional<uint32_t> val = std::nullopt);
	void              ClearMissedCFOPacketCount(DTC_Link_ID const& link);
	RegisterFormatter FormatMissedCFOPacketCountLink0();
	RegisterFormatter FormatMissedCFOPacketCountLink1();
	RegisterFormatter FormatMissedCFOPacketCountLink2();
	RegisterFormatter FormatMissedCFOPacketCountLink3();
	RegisterFormatter FormatMissedCFOPacketCountLink4();
	RegisterFormatter FormatMissedCFOPacketCountLink5();

	// Local Fragment Drop Count Register
	uint32_t          ReadLocalFragmentDropCount(std::optional<uint32_t> val = std::nullopt);
	void              ClearLocalFragmentDropCount();
	RegisterFormatter FormatLocalFragmentDropCount();

	// EVB SubEvent Receive Timer Preset
	uint32_t          ReadEVBSubEventReceiveTimer(std::optional<uint32_t> val = std::nullopt);
	void              SetEVBSubEventReceiveTimer(uint32_t timer);
	RegisterFormatter FormatEVBSubEventReceiveTimer();

	// EVB SERDES PRBS Control / Status Register
	bool              ReadEVBSERDESPRBSErrorFlag(std::optional<uint32_t> val = std::nullopt);
	uint8_t           ReadEVBSERDESTXPRBSSEL(std::optional<uint32_t> val = std::nullopt);
	void              SetEVBSERDESTXPRBSSEL(uint8_t byte);
	uint8_t           ReadEVBSERDESRXPRBSSEL(std::optional<uint32_t> val = std::nullopt);
	void              SetEVBSERDESRXPRBSSEL(uint8_t byte);
	bool              ReadEVBSERDESPRBSForceError(std::optional<uint32_t> val = std::nullopt);
	void              SetEVBSERDESPRBSForceError(bool flag);
	void              ToggleEVBSERDESPRBSForceError();
	bool              ReadEVBSERDESPRBSReset(std::optional<uint32_t> val = std::nullopt);
	void              SetEVBSERDESPRBSReset(bool flag);
	void              ToggleEVBSERDESPRBSReset();
	RegisterFormatter FormatEVBSERDESPRBSControl();

	// Event Builder Error Register
	bool              ReadEventBuilder_SubEventReceiverFlagsBufferError(std::optional<uint32_t> val = std::nullopt);
	bool              ReadEventBuilder_EthernetInputFIFOFull(std::optional<uint32_t> val = std::nullopt);
	bool              ReadEventBuilder_LinkError(std::optional<uint32_t> val = std::nullopt);
	bool              ReadEventBuilder_TXPacketError(std::optional<uint32_t> val = std::nullopt);
	bool              ReadEventBuilder_LocalDataPointerFIFOQueueError(std::optional<uint32_t> val = std::nullopt);
	bool              ReadEventBuilder_TransmitDMAByteCountFIFOFull(std::optional<uint32_t> val = std::nullopt);
	RegisterFormatter FormatEventBuilderErrorRegister();

	// SERDES VFIFO Error Register
	bool              ReadSERDESVFIFO_EgressFIFOFull(std::optional<uint32_t> val = std::nullopt);
	bool              ReadSERDESVFIFO_IngressFIFOFull(std::optional<uint32_t> val = std::nullopt);
	bool              ReadSERDESVFIFO_EventByteCountTotalError(std::optional<uint32_t> val = std::nullopt);
	bool              ReadSERDESVFIFO_LastWordWrittenTimeoutError(std::optional<uint32_t> val = std::nullopt);
	bool              ReadSERDESVFIFO_FragmentCountError(std::optional<uint32_t> val = std::nullopt);
	bool              ReadSERDESVFIFO_DDRFullError(std::optional<uint32_t> val = std::nullopt);
	RegisterFormatter FormatSERDESVFIFOError();

	// PCI VFIFO Error Register
	bool              ReadPCIVFIFO_DDRFull(std::optional<uint32_t> val = std::nullopt);
	bool              ReadPCIVFIFO_MemoryMappedWriteCompleteFIFOFull(std::optional<uint32_t> val = std::nullopt);
	bool              ReadPCIVFIFO_PCIWriteEventFIFOFull(std::optional<uint32_t> val = std::nullopt);
	bool              ReadPCIVFIFO_LocalDataPointerFIFOFull(std::optional<uint32_t> val = std::nullopt);
	bool              ReadPCIVFIFO_EgressFIFOFull(std::optional<uint32_t> val = std::nullopt);
	bool              ReadPCIVFIFO_RXBufferSelectFIFOFull(std::optional<uint32_t> val = std::nullopt);
	bool              ReadPCIVFIFO_IngressFIFOFull(std::optional<uint32_t> val = std::nullopt);
	bool              ReadPCIVFIFO_EventByteCountTotalError(std::optional<uint32_t> val = std::nullopt);
	RegisterFormatter FormatPCIVFIFOError();

	// ROC Link Error Registers
	bool              ReadROCLink_ROCDataRequestSyncError(DTC_Link_ID const& link, std::optional<uint32_t> val = std::nullopt);
	bool              ReadROCLink_RXPacketCountError(DTC_Link_ID const& link, std::optional<uint32_t> val = std::nullopt);
	bool              ReadROCLink_RXPacketError(DTC_Link_ID const& link, std::optional<uint32_t> val = std::nullopt);
	bool              ReadROCLink_RXPacketCRCError(DTC_Link_ID const& link, std::optional<uint32_t> val = std::nullopt);
	bool              ReadROCLink_DataPendingTimeoutError(DTC_Link_ID const& link, std::optional<uint32_t> val = std::nullopt);
	bool              ReadROCLink_ReceiveDataPacketCountError(DTC_Link_ID const& link, std::optional<uint32_t> val = std::nullopt);
	RegisterFormatter FormatRocLink0Error();
	RegisterFormatter FormatRocLink1Error();
	RegisterFormatter FormatRocLink2Error();
	RegisterFormatter FormatRocLink3Error();
	RegisterFormatter FormatRocLink4Error();
	RegisterFormatter FormatRocLink5Error();

	// CFO Link Error Register
	RegisterFormatter FormatCFOLinkError();

	// Link Mux Error Register
	bool              ReadDCSMuxDecodeError(std::optional<uint32_t> val = std::nullopt);
	bool              ReadDataMuxDecodeError(std::optional<uint32_t> val = std::nullopt);
	RegisterFormatter FormatLinkMuxError();

	// SFP Control Status Register
	bool              ReadSFPPresent(std::optional<uint32_t> val = std::nullopt);
	bool              ReadSFPLOS(std::optional<uint32_t> val = std::nullopt);
	bool              ReadSFPTXFault(std::optional<uint32_t> val = std::nullopt);
	void              EnableSFPRateSelect();
	void              DisableSFPRateSelect();
	bool              ReadSFPRateSelect(std::optional<uint32_t> val = std::nullopt);
	void              DisableSFPTX();
	void              EnableSFPTX();
	bool              ReadSFPTXDisable(std::optional<uint32_t> val = std::nullopt);
	RegisterFormatter FormatSFPControlStatus();

	// RX CDR Unlock Count Registers
	uint32_t          ReadRXCDRUnlockCount(DTC_Link_ID const& link, std::optional<uint32_t> val = std::nullopt);
	void              ClearRXCDRUnlockCount(DTC_Link_ID const& link);
	RegisterFormatter FormatRXCDRUnlockCountLink0();
	RegisterFormatter FormatRXCDRUnlockCountLink1();
	RegisterFormatter FormatRXCDRUnlockCountLink2();
	RegisterFormatter FormatRXCDRUnlockCountLink3();
	RegisterFormatter FormatRXCDRUnlockCountLink4();
	RegisterFormatter FormatRXCDRUnlockCountLink5();
	RegisterFormatter FormatRXCDRUnlockCountCFOLink();

	// RX Jitter Attenuator Unlock Count Register
	uint32_t          ReadJitterAttenuatorUnlockCount(std::optional<uint32_t> val = std::nullopt);
	void              ClearJitterAttenuatorUnlockCount();
	RegisterFormatter FormatJitterAttenuatorUnlockCount();

	// RX CFO Link Event Start Character Error Count Register
	uint32_t          ReadRXCFOLinkEventStartCharacterErrorCount(std::optional<uint32_t> val = std::nullopt);
	void              ClearRXCFOLinkEventStartCharacterErrorCount();
	RegisterFormatter FormatRXCFOLinkEventStartCharacterErrorCount();

	// RX CFO Link 40MHz Clock Character Error Count Register
	uint32_t          ReadRXCFOLink40MHzCharacterErrorCount(std::optional<uint32_t> val = std::nullopt);
	void              ClearRXCFOLink40MHzCharacterErrorCount();
	RegisterFormatter FormatRXCFOLink40MHzCharacterErrorCount();

	// Input Buffer Fragment Dump Count
	uint32_t          ReadInputBufferFragmentDumpCount(std::optional<uint32_t> val = std::nullopt);
	void              ClearInputBufferFragmentDumpCount();
	RegisterFormatter FormatInputBufferFragmentDumpCount();

	// Output Buffer Fragment Dump Count
	uint32_t          ReadOutputBufferFragmentDumpCount(std::optional<uint32_t> val = std::nullopt);
	void              ClearOutputBufferFragmentDumpCount();
	RegisterFormatter FormatOutputBufferFragmentDumpCount();

	// ROC DCS Response Timer Preset
	uint32_t          ReadROCDCSResponseTimer(std::optional<uint32_t> val = std::nullopt);
	void              SetROCDCSResponseTimer(uint32_t timer);
	RegisterFormatter FormatROCDCSResponseTimerPreset();

	// Software DataRequests
	void               SetSoftwareDataRequest(const DTC_EventWindowTag& ts);
	DTC_EventWindowTag ReadSoftwareDataRequest(std::optional<uint32_t> val = std::nullopt);
	RegisterFormatter  FormatSoftwareDataRequestLow();
	RegisterFormatter  FormatSoftwareDataRequestHigh();

	// FPGA PROM Program Data Register

	// FPGA PROM Program Status Register
	bool              ReadFPGAPROMProgramFIFOFull(std::optional<uint32_t> val = std::nullopt);
	bool              ReadFPGAPROMReady(std::optional<uint32_t> val = std::nullopt);
	RegisterFormatter FormatFPGAPROMProgramStatus();

	// FPGA Core Access Register
	void              ReloadFPGAFirmware();
	bool              ReadFPGACoreAccessFIFOFull(std::optional<uint32_t> val = std::nullopt);
	bool              ReadFPGACoreAccessFIFOEmpty(std::optional<uint32_t> val = std::nullopt);
	RegisterFormatter FormatFPGACoreAccess();

	// Slow Optical Links Control/Status Register
	bool              ReadRXOKErrorSlowOpticalLink3(std::optional<uint32_t> val = std::nullopt);
	bool              ReadRXOKErrorSlowOpticalLink2(std::optional<uint32_t> val = std::nullopt);
	bool              ReadRXOKErrorSlowOpticalLink1(std::optional<uint32_t> val = std::nullopt);
	bool              ReadRXOKErrorSlowOpticalLink0(std::optional<uint32_t> val = std::nullopt);
	bool              ReadLatchedSpareSMAInputOKError(std::optional<uint32_t> val = std::nullopt);
	void              ClearLatchedSpareSMAInputOKError();
	bool              ReadLatchedEventMarkerSMAInputOKError(std::optional<uint32_t> val = std::nullopt);
	void              ClearLatchedEventMarkerSMASMAInputOKError();
	bool              ReadLatchedRXOKErrorSlowOpticalLink3(std::optional<uint32_t> val = std::nullopt);
	void              ClearLatchedRXOKErrorSlowOpticalLink3();
	bool              ReadLatchedRXOKErrorSlowOpticalLink2(std::optional<uint32_t> val = std::nullopt);
	void              ClearLatchedRXOKErrorSlowOpticalLink2();
	bool              ReadLatchedRXOKErrorSlowOpticalLink1(std::optional<uint32_t> val = std::nullopt);
	void              ClearLatchedRXOKErrorSlowOpticalLink1();
	bool              ReadLatchedRXOKErrorSlowOpticalLink0(std::optional<uint32_t> val = std::nullopt);
	void              ClearLatchedRXOKErrorSlowOpticalLink0();
	RegisterFormatter FormatSlowOpticalLinkControlStatus();

	// Diagnostic SERDES Induce Error Enable Register
	bool              ReadSERDESInduceErrorEnable(DTC_Link_ID const& link, std::optional<uint32_t> val = std::nullopt);
	void              EnableSERDESInduceError(DTC_Link_ID const& link);
	void              DisableSERDESInduceError(DTC_Link_ID const& link);
	RegisterFormatter FormatSERDESInduceErrorEnable();

	// Diagnostic SERDES Incude Error Link Registers
	uint32_t          ReadSERDESInduceErrorSequenceNumber(DTC_Link_ID const& link, std::optional<uint32_t> val = std::nullopt);
	void              SetSERDESInduceErrorSequenceNumber(DTC_Link_ID const& link, uint32_t sequence);
	RegisterFormatter FormatSERDESInduceErrorSequenceNumberLink0();
	RegisterFormatter FormatSERDESInduceErrorSequenceNumberLink1();
	RegisterFormatter FormatSERDESInduceErrorSequenceNumberLink2();
	RegisterFormatter FormatSERDESInduceErrorSequenceNumberLink3();
	RegisterFormatter FormatSERDESInduceErrorSequenceNumberLink4();
	RegisterFormatter FormatSERDESInduceErrorSequenceNumberLink5();

	// // DDR Memory Flags Registers
	// DTC_DDRFlags ReadDDRFlags(uint8_t buffer_id);
	// std::bitset<128> ReadDDRLinkBufferFullFlags();
	// std::bitset<128> ReadDDRLinkBufferEmptyFlags();
	// std::bitset<128> ReadDDRLinkBufferHalfFullFlags();
	// std::bitset<128> ReadDDREventBuilderBufferFullFlags();
	// std::bitset<128> ReadDDREventBuilderBufferEmptyFlags();
	// std::bitset<128> ReadDDREventBuilderBufferHalfFullFlags();
	// RegisterFormatter FormatDDRLinkBufferEmptyFlags0();
	// RegisterFormatter FormatDDRLinkBufferEmptyFlags1();
	// RegisterFormatter FormatDDRLinkBufferEmptyFlags2();
	// RegisterFormatter FormatDDRLinkBufferEmptyFlags3();
	// RegisterFormatter FormatDDRLinkBufferHalfFullFlags0();
	// RegisterFormatter FormatDDRLinkBufferHalfFullFlags1();
	// RegisterFormatter FormatDDRLinkBufferHalfFullFlags2();
	// RegisterFormatter FormatDDRLinkBufferHalfFullFlags3();
	// RegisterFormatter FormatDDRLinkBufferFullFlags0();
	// RegisterFormatter FormatDDRLinkBufferFullFlags1();
	// RegisterFormatter FormatDDRLinkBufferFullFlags2();
	// RegisterFormatter FormatDDRLinkBufferFullFlags3();
	// RegisterFormatter FormatDDREventBuilderBufferEmptyFlags0();
	// RegisterFormatter FormatDDREventBuilderBufferEmptyFlags1();
	// RegisterFormatter FormatDDREventBuilderBufferEmptyFlags2();
	// RegisterFormatter FormatDDREventBuilderBufferEmptyFlags3();
	// RegisterFormatter FormatDDREventBuilderBufferHalfFullFlags0();
	// RegisterFormatter FormatDDREventBuilderBufferHalfFullFlags1();
	// RegisterFormatter FormatDDREventBuilderBufferHalfFullFlags2();
	// RegisterFormatter FormatDDREventBuilderBufferHalfFullFlags3();
	// RegisterFormatter FormatDDREventBuilderBufferFullFlags0();
	// RegisterFormatter FormatDDREventBuilderBufferFullFlags1();
	// RegisterFormatter FormatDDREventBuilderBufferFullFlags2();
	// RegisterFormatter FormatDDREventBuilderBufferFullFlags3();

	// Data Pending Diagnostic Timer Registers
	uint32_t          ReadDataPendingDiagnosticTimer(DTC_Link_ID const& link, std::optional<uint32_t> val = std::nullopt);
	void              ResetDataPendingDiagnosticTimerFIFO(DTC_Link_ID const& link);
	RegisterFormatter FormatDataPendingDiagnosticTimerLink0();
	RegisterFormatter FormatDataPendingDiagnosticTimerLink1();
	RegisterFormatter FormatDataPendingDiagnosticTimerLink2();
	RegisterFormatter FormatDataPendingDiagnosticTimerLink3();
	RegisterFormatter FormatDataPendingDiagnosticTimerLink4();
	RegisterFormatter FormatDataPendingDiagnosticTimerLink5();

	// SERDES CNIT Error Count
	uint32_t          ReadSERDESCharacterNotInTableErrorCount(DTC_Link_ID const& link, std::optional<uint32_t> val = std::nullopt);
	void              ClearSERDESCharacterNotInTableErrorCount(DTC_Link_ID const& link);
	RegisterFormatter FormatSERDESCharacterNotInTableErrorCountLink0();
	RegisterFormatter FormatSERDESCharacterNotInTableErrorCountLink1();
	RegisterFormatter FormatSERDESCharacterNotInTableErrorCountLink2();
	RegisterFormatter FormatSERDESCharacterNotInTableErrorCountLink3();
	RegisterFormatter FormatSERDESCharacterNotInTableErrorCountLink4();
	RegisterFormatter FormatSERDESCharacterNotInTableErrorCountLink5();
	RegisterFormatter FormatSERDESCharacterNotInTableErrorCountCFOLink();

	// SERDES RX Disparity Error Count
	uint32_t          ReadSERDESRXDisparityErrorCount(DTC_Link_ID const& link, std::optional<uint32_t> val = std::nullopt);
	void              ClearSERDESRXDisparityErrorCount(DTC_Link_ID const& link);
	RegisterFormatter FormatSERDESRXDisparityErrorCountLink0();
	RegisterFormatter FormatSERDESRXDisparityErrorCountLink1();
	RegisterFormatter FormatSERDESRXDisparityErrorCountLink2();
	RegisterFormatter FormatSERDESRXDisparityErrorCountLink3();
	RegisterFormatter FormatSERDESRXDisparityErrorCountLink4();
	RegisterFormatter FormatSERDESRXDisparityErrorCountLink5();
	RegisterFormatter FormatSERDESRXDisparityErrorCountCFOLink();

	// SERDES RX PRBS Error Count
	uint32_t          ReadSERDESRXPRBSErrorCount(DTC_Link_ID const& link, std::optional<uint32_t> val = std::nullopt);
	void              ClearSERDESRXPRBSErrorCount(DTC_Link_ID const& link);
	RegisterFormatter FormatSERDESRXPRBSErrorCountLink0();
	RegisterFormatter FormatSERDESRXPRBSErrorCountLink1();
	RegisterFormatter FormatSERDESRXPRBSErrorCountLink2();
	RegisterFormatter FormatSERDESRXPRBSErrorCountLink3();
	RegisterFormatter FormatSERDESRXPRBSErrorCountLink4();
	RegisterFormatter FormatSERDESRXPRBSErrorCountLink5();
	RegisterFormatter FormatSERDESRXPRBSErrorCountCFOLink();

	// SERDES RX CRC Error Count
	uint32_t          ReadSERDESRXCRCErrorCount(DTC_Link_ID const& link, std::optional<uint32_t> val = std::nullopt);
	void              ClearSERDESRXCRCErrorCount(DTC_Link_ID const& link);
	RegisterFormatter FormatSERDESRXCRCErrorCountLink0();
	RegisterFormatter FormatSERDESRXCRCErrorCountLink1();
	RegisterFormatter FormatSERDESRXCRCErrorCountLink2();
	RegisterFormatter FormatSERDESRXCRCErrorCountLink3();
	RegisterFormatter FormatSERDESRXCRCErrorCountLink4();
	RegisterFormatter FormatSERDESRXCRCErrorCountLink5();
	RegisterFormatter FormatSERDESRXCRCErrorCountCFOLink();

	// SERDES RX CRC Error Control
	bool              ReadEnableInduceSERDESRXCRCError(DTC_Link_ID const& link, std::optional<uint32_t> val = std::nullopt);
	void              EnableInduceSERDESRXCRCError(DTC_Link_ID const& link);
	void              DisableInduceSERDESRXCRCError(DTC_Link_ID const& link);
	RegisterFormatter FormatSERDESRXCRCErrorControl();

	uint32_t          ReadEVBSERDESRXPacketErrorCounter(std::optional<uint32_t> val = std::nullopt);
	void              ClearEVBSERDESRXPacketErrorCounter();
	RegisterFormatter FormatEVBSERDESRXPacketErrorCounter();

	// Jitter Attenuator SERDES RX Recovered Clock LOS Counter
	uint32_t          ReadJitterAttenuatorRecoveredClockLOSCount(std::optional<uint32_t> val = std::nullopt);
	void              ClearJitterAttenuatorRecoeveredClockLOSCount();
	RegisterFormatter FormatJitterAttenuatorRecoveredClockLOSCount();

	// Jitter Attenuator SERDES RX External Clock LOS Counter
	uint32_t          ReadJitterAttenuatorExternalClockLOSCount(std::optional<uint32_t> val = std::nullopt);
	void              ClearJitterAttenuatorExternalClockLOSCount();
	RegisterFormatter FormatJitterAttenuatorExternalClockLOSCount();

	// ROC Emulator Interpacket Delay
	uint32_t          ReadROCEmulatorInterpacketDelay(DTC_Link_ID const& link, std::optional<uint32_t> val = std::nullopt);
	void              SetROCEmulatorInterpacketDelay(DTC_Link_ID const& link, uint32_t delay);
	RegisterFormatter FormatROCEmulatorInterpacketDelayLink0();

	// TX Data Request Packet Count
	uint32_t          ReadTXDataRequestPacketCount(DTC_Link_ID const& link, std::optional<uint32_t> val = std::nullopt);
	RegisterFormatter FormatTXDataRequestPacketCountLink(DTC_Link_ID const& link);
	DTC_Register      GetTXDataRequestPacketCountLinkRegister(DTC_Link_ID const& link);

	RegisterFormatter FormatCFOTXClockMarkerCountLink6();
	uint32_t          ReadCFOTXClockMarkerCountLink6(std::optional<uint32_t> val = std::nullopt);

	// TX Heartbeat Packet Count
	uint32_t          ReadTXHeartbeatPacketCount(DTC_Link_ID const& link, std::optional<uint32_t> val = std::nullopt);
	RegisterFormatter FormatTXHeartbeatPacketCountLink(DTC_Link_ID const& link);
	DTC_Register      GetTXHeartbeatPacketCountLinkRegister(DTC_Link_ID const& link);

	// TX Data Request Packet Count
	uint32_t          ReadTXEventWindowMarkerCount(DTC_Link_ID const& link, std::optional<uint32_t> val = std::nullopt);
	RegisterFormatter FormatTXEventWindowMarkerCountLink(DTC_Link_ID const& link);
	DTC_Register      GetTXEventWindowMarkerCountLinkRegister(DTC_Link_ID const& link);

	// RX Data Header Packet Count
	uint32_t          ReadRXDataHeaderPacketCount(DTC_Link_ID const& link, std::optional<uint32_t> val = std::nullopt);
	RegisterFormatter FormatRXDataHeaderPacketCountLink(DTC_Link_ID const& link);
	DTC_Register      GetRXDataHeaderPacketCountLinkRegister(DTC_Link_ID const& link);

	// RX Data Packet Count
	uint32_t          ReadRXDataPacketCount(DTC_Link_ID const& link, std::optional<uint32_t> val = std::nullopt);
	RegisterFormatter FormatRXDataPacketCountLink(DTC_Link_ID const& link);
	DTC_Register      GetRXDataPacketCountLinkRegister(DTC_Link_ID const& link);

	// EVB Diagnostic RX Packet FIFO
	uint64_t ReadEVBDiagnosticFIFO(std::optional<uint32_t> val = std::nullopt);
	void     ClearEVBDiagnosticFIFO();
	// No formatters for FIFO!

	// Event Mode Lookup Table
	void     SetAllEventModeWords(uint32_t data);
	void     SetEventModeWord(uint8_t which, uint32_t data);
	uint32_t ReadEventModeWord(uint8_t which);

	// Oscillator Programming (DDR and SERDES)
	bool     SetNewOscillatorFrequency(DTC_OscillatorType oscillator, double targetFrequency);
	uint64_t ReadCurrentProgram(DTC_OscillatorType oscillator);
	void     WriteCurrentProgram(uint64_t program, DTC_OscillatorType oscillator);

  private:
	bool NeedToVerifyRegisterWrite_(const CFOandDTC_Register& /*address*/) override { return true; }
	void VerifyRegisterWrite_(const CFOandDTC_Register& address, uint32_t readbackValue, uint32_t dataToWrite) override;

	int      DecodeHighSpeedDivider_(int input);
	int      DecodeOutputDivider_(int input) { return input + 1; }
	double   DecodeRFREQ_(uint64_t input) { return input / 268435456.0; }
	int      EncodeHighSpeedDivider_(int input);
	int      EncodeOutputDivider_(int input);
	uint64_t EncodeRFREQ_(double input) { return static_cast<uint64_t>(input * 268435456) & 0x3FFFFFFFFF; }
	uint64_t CalculateFrequencyForProgramming_(double targetFrequency, double currentFrequency, uint64_t currentProgram);
	uint64_t ReadSERDESOscillatorParameters_();
	uint64_t ReadTimingOscillatorParameters_();
	// uint64_t ReadDDROscillatorParameters_();
	void SetSERDESOscillatorParameters_(uint64_t program);
	void SetTimingOscillatorParameters_(uint64_t program);
	// void SetDDROscillatorParameters_(uint64_t program);

	bool WaitForLinkReady_(DTC_Link_ID const& link, size_t interval, double timeout = 2.0 /*seconds*/);

	std::unordered_map<uint8_t, std::pair<uint32_t, std::chrono::steady_clock::time_point>> lastPacketCount;
	std::unordered_map<uint8_t, double>                                                     lastPacketRate;

  protected:
	DTC_SimMode simMode_;                       ///< Simulation mode
	bool        usingDetectorEmulator_{false};  ///< Whether Detector Emulation mode is enabled
	uint16_t    dmaSize_;                       ///< Size of DMAs, in bytes (default 32k)

  public:
	std::pair<uint32_t, std::chrono::steady_clock::time_point> getPacketCountInfo(uint8_t dtc) const;
	void                                                       updatePacketCount(uint8_t dtc, uint32_t currentCount);
	double                                                     getLastPacketRate(uint8_t dtc) const;

	bool                                                           WaitForLinkReady(DTC_Link_ID const& link, size_t interval, double timeout = 2.0) { return WaitForLinkReady_(link, interval, timeout); }
	virtual const std::vector<std::function<RegisterFormatter()>>& getFormattedDumpFunctions() override { return formattedDumpFunctions_; };              // pure virtual
	virtual const std::vector<std::function<RegisterFormatter()>>& getFormattedSimpleDumpFunctions() override { return formattedSimpleDumpFunctions_; };  // pure virtual

	const std::vector<std::function<RegisterFormatter()>> formattedSimpleDumpFunctions_{
	    [this] { return this->FormatDTCControl(); },
	    [this] { return this->FormatJitterAttenuatorCSR(); },
	    [this] { return this->FormatSERDESPLLLocked(); },
	    [this] { return this->FormatROCEmulationEnable(); },
	    [this] { return this->FormatLinkEnable(); },
	    [this] { return this->FormatCFOLinkError(); },
	    [this] { return this->FormatCFO40MHzClockMarkerEnables(); },
	    [this] { return this->FormatRXCDRLockStatus(); },
	    [this] { return this->FormatSERDESResetDone(); },
	    [this] { return this->FormatSERDESReset(); },
	};
	const std::vector<std::function<RegisterFormatter()>> formattedROCEmulationFunctions_{
	    [this] { return this->FormatROCEmulationEnable(); },
	    [this] { return this->FormatLinkEnable(); },
	    [this] { return this->FormatRXCDRLockStatus(); },
	    [this] { return this->FormatROCEmulationNumPacketsLink01(); },
	    [this] { return this->FormatROCEmulationNumPacketsLink23(); },
	    [this] { return this->FormatROCEmulationNumPacketsLink45(); },
	};

	const std::vector<std::function<RegisterFormatter()>> formattedDumpFunctions_{
	    [this] { return this->FormatDesignVersion(); },
	    [this] { return this->FormatDesignDate(); },
	    [this] { return this->FormatDesignStatus(); },
	    [this] { return this->FormatVivadoVersion(); },
	    [this] { return this->FormatFPGATemperature(); },
	    [this] { return this->FormatFPGAVCCINT(); },
	    [this] { return this->FormatFPGAVCCAUX(); },
	    [this] { return this->FormatFPGAVCCBRAM(); },
	    [this] { return this->FormatFPGAAlarms(); },
	    [this] { return this->FormatDTCControl(); },
	    [this] { return this->FormatSERDESPLLLocked(); },
	    [this] { return this->FormatDMATransferLength(); },
	    [this] { return this->FormatSERDESLoopbackEnable(); },
	    [this] { return this->FormatClockOscillatorStatus(); },
	    [this] { return this->FormatROCEmulationEnable(); },
	    [this] { return this->FormatLinkEnable(); },
	    [this] { return this->FormatSERDESReset(); },
	    [this] { return this->FormatSERDESRXDisparityError(); },
	    [this] { return this->FormatSERDESRXCharacterNotInTableError(); },
	    [this] { return this->FormatSERDESUnlockError(); },
	    [this] { return this->FormatSERDESPLLPowerDown(); },
	    [this] { return this->FormatSERDESRXStatus(); },
	    [this] { return this->FormatSERDESResetDone(); },
	    [this] { return this->FormatRXCDRLockStatus(); },
	    [this] { return this->FormatDMATimeoutPreset(); },
	    [this] { return this->FormatROCReplyTimeout(); },
	    [this] { return this->FormatROCReplyTimeoutError(); },
	    [this] { return this->FormatEVBLocalParitionIDMACIndex(); },
	    [this] { return this->FormatEVBClusterInfo(); },
	    [this] { return this->FormatSERDESOscillatorControl(); },
	    [this] { return this->FormatSERDESOscillatorParameterLow(); },
	    [this] { return this->FormatSERDESOscillatorParameterHigh(); },
	    // [this] { return this->FormatDDROscillatorFrequency(); },
	    // [this] { return this->FormatDDROscillatorControl(); },
	    // [this] { return this->FormatDDROscillatorParameterLow(); },
	    // [this] { return this->FormatDDROscillatorParameterHigh(); },
	    [this] { return this->FormatFIFOFullErrorFlag0(); },
	    [this] { return this->FormatFIFOFullErrorFlag1(); },
	    [this] { return this->FormatFIFOFullErrorFlag2(); },
	    [this] { return this->FormatReceivePacketError(); },
	    [this] { return this->FormatCFOEmulationTimestampLow(); },
	    [this] { return this->FormatCFOEmulationTimestampHigh(); },
	    [this] { return this->FormatCFOEmulationEventWindowInterval(); },
	    [this] { return this->FormatCFOEmulationNumHeartbeats(); },
	    [this] { return this->FormatROCEmulationNumPacketsLink01(); },
	    [this] { return this->FormatROCEmulationNumPacketsLink23(); },
	    [this] { return this->FormatROCEmulationNumPacketsLink45(); },
	    [this] { return this->FormatCFOEmulationNumNullHeartbeats(); },
	    [this] { return this->FormatCFOEmulationModeBytes03(); },
	    [this] { return this->FormatCFOEmulationModeBytes45(); },
	    //[this] { return this->FormatCFOEmulationDebugPacketType(); },
	    [this] { return this->FormatRXPacketCountErrorFlags(); },
	    [this] { return this->FormatDetectorEmulationDMACount(); },
	    [this] { return this->FormatDetectorEmulationDMADelayCount(); },
	    [this] { return this->FormatDetectorEmulationControl0(); },
	    [this] { return this->FormatDetectorEmulationControl1(); },
	    [this] { return this->FormatDDRDataLocalStartAddress(); },
	    [this] { return this->FormatDDRDataLocalEndAddress(); },
	    [this] { return this->FormatCFOEmulatorInterpacketDelay(); },
	    [this] { return this->FormatEthernetPayloadSize(); },
	    [this] { return this->FormatCFOEmulation40MHzMarkerInterval(); },
	    [this] { return this->FormatCFO40MHzClockMarkerEnables(); },
	    [this] { return this->FormatROCFinishThreshold(); },
	    [this] { return this->FormatFireflyTXIICControl(); },
	    [this] { return this->FormatFireflyTXIICParameterLow(); },
	    [this] { return this->FormatFireflyTXIICParameterHigh(); },
	    [this] { return this->FormatFireflyRXIICControl(); },
	    [this] { return this->FormatFireflyRXIICParameterLow(); },
	    [this] { return this->FormatFireflyRXIICParameterHigh(); },
	    [this] { return this->FormatFireflyTXRXIICControl(); },
	    [this] { return this->FormatFireflyTXRXIICParameterLow(); },
	    [this] { return this->FormatFireflyTXRXIICParameterHigh(); },
	    [this] { return this->FormatSERDESTXPRBSControl(); },
	    [this] { return this->FormatSERDESRXPRBSControl(); },
	    [this] { return this->FormatEventModeLookupTableControl(); },
	    [this] { return this->FormatDDRMemoryTestRegister(); },
	    [this] { return this->FormatSERDESSerialInversionEnable(); },
	    [this] { return this->FormatJitterAttenuatorCSR(); },
	    [this] { return this->FormatSFPIICControl(); },
	    [this] { return this->FormatSFPIICParameterLow(); },
	    [this] { return this->FormatSFPIICParameterHigh(); },
	    [this] { return this->FormatEVBSubEventReceiveTimer(); },
	    [this] { return this->FormatEVBSERDESPRBSControl(); },
	    [this] { return this->FormatEventBuilderErrorRegister(); },
	    [this] { return this->FormatSERDESVFIFOError(); },
	    [this] { return this->FormatPCIVFIFOError(); },
	    [this] { return this->FormatRocLink0Error(); },
	    [this] { return this->FormatRocLink1Error(); },
	    [this] { return this->FormatRocLink2Error(); },
	    [this] { return this->FormatRocLink3Error(); },
	    [this] { return this->FormatRocLink4Error(); },
	    [this] { return this->FormatRocLink5Error(); },
	    [this] { return this->FormatCFOLinkError(); },
	    [this] { return this->FormatLinkMuxError(); },
	    [this] { return this->FormatFireflyCSR(); },
	    [this] { return this->FormatSFPControlStatus(); },
	    // [this] { return this->FormatROCDCSResponseTimerPreset(); },
	    [this] { return this->FormatSoftwareDataRequestLow(); },
	    [this] { return this->FormatSoftwareDataRequestHigh(); },
	    [this] { return this->FormatFPGAPROMProgramStatus(); },
	    [this] { return this->FormatFPGACoreAccess(); },
	    [this] { return this->FormatSlowOpticalLinkControlStatus(); },
	    [this] { return this->FormatSERDESRXCRCErrorControl(); },
	};

	const std::vector<std::function<RegisterFormatter()>> formattedPerformanceCounterFunctions_{
	    // [this] { return this->FormatDDRLinkBufferEmptyFlags0(); },
	    // [this] { return this->FormatDDRLinkBufferEmptyFlags1(); },
	    // [this] { return this->FormatDDRLinkBufferEmptyFlags2(); },
	    // [this] { return this->FormatDDRLinkBufferEmptyFlags3(); },
	    // [this] { return this->FormatDDRLinkBufferHalfFullFlags0(); },
	    // [this] { return this->FormatDDRLinkBufferHalfFullFlags1(); },
	    // [this] { return this->FormatDDRLinkBufferHalfFullFlags2(); },
	    // [this] { return this->FormatDDRLinkBufferHalfFullFlags3(); },
	    // [this] { return this->FormatDDRLinkBufferFullFlags0(); },
	    // [this] { return this->FormatDDRLinkBufferFullFlags1(); },
	    // [this] { return this->FormatDDRLinkBufferFullFlags2(); },
	    // [this] { return this->FormatDDRLinkBufferFullFlags3(); },
	    // [this] { return this->FormatDDREventBuilderBufferEmptyFlags0(); },
	    // [this] { return this->FormatDDREventBuilderBufferEmptyFlags1(); },
	    // [this] { return this->FormatDDREventBuilderBufferEmptyFlags2(); },
	    // [this] { return this->FormatDDREventBuilderBufferEmptyFlags3(); },
	    // [this] { return this->FormatDDREventBuilderBufferHalfFullFlags0(); },
	    // [this] { return this->FormatDDREventBuilderBufferHalfFullFlags1(); },
	    // [this] { return this->FormatDDREventBuilderBufferHalfFullFlags2(); },
	    // [this] { return this->FormatDDREventBuilderBufferHalfFullFlags3(); },
	    // [this] { return this->FormatDDREventBuilderBufferFullFlags0(); },
	    // [this] { return this->FormatDDREventBuilderBufferFullFlags1(); },
	    // [this] { return this->FormatDDREventBuilderBufferFullFlags2(); },
	    // [this] { return this->FormatDDREventBuilderBufferFullFlags3(); },
	    [this] { return this->FormatRetransmitRequestCountLink0(); },
	    [this] { return this->FormatRetransmitRequestCountLink1(); },
	    [this] { return this->FormatRetransmitRequestCountLink2(); },
	    [this] { return this->FormatRetransmitRequestCountLink3(); },
	    [this] { return this->FormatRetransmitRequestCountLink4(); },
	    [this] { return this->FormatRetransmitRequestCountLink5(); },
	    [this] { return this->FormatDataPendingDiagnosticTimerLink0(); },
	    [this] { return this->FormatDataPendingDiagnosticTimerLink1(); },
	    [this] { return this->FormatDataPendingDiagnosticTimerLink2(); },
	    [this] { return this->FormatDataPendingDiagnosticTimerLink3(); },
	    [this] { return this->FormatDataPendingDiagnosticTimerLink4(); },
	    [this] { return this->FormatDataPendingDiagnosticTimerLink5(); }

	};

	const std::vector<std::function<RegisterFormatter()>> formattedSERDESErrorFunctions_{
	    [this] { return this->FormatCFOLinkError(); },
	    [this] { return this->FormatSERDESCharacterNotInTableErrorCountLink0(); },
	    [this] { return this->FormatSERDESCharacterNotInTableErrorCountLink1(); },
	    [this] { return this->FormatSERDESCharacterNotInTableErrorCountLink2(); },
	    [this] { return this->FormatSERDESCharacterNotInTableErrorCountLink3(); },
	    [this] { return this->FormatSERDESCharacterNotInTableErrorCountLink4(); },
	    [this] { return this->FormatSERDESCharacterNotInTableErrorCountLink5(); },
	    [this] { return this->FormatSERDESCharacterNotInTableErrorCountCFOLink(); },
	    [this] { return this->FormatSERDESRXDisparityErrorCountLink0(); },
	    [this] { return this->FormatSERDESRXDisparityErrorCountLink1(); },
	    [this] { return this->FormatSERDESRXDisparityErrorCountLink2(); },
	    [this] { return this->FormatSERDESRXDisparityErrorCountLink3(); },
	    [this] { return this->FormatSERDESRXDisparityErrorCountLink4(); },
	    [this] { return this->FormatSERDESRXDisparityErrorCountLink5(); },
	    [this] { return this->FormatSERDESRXDisparityErrorCountCFOLink(); },
	    [this] { return this->FormatSERDESRXPRBSErrorCountLink0(); },
	    [this] { return this->FormatSERDESRXPRBSErrorCountLink1(); },
	    [this] { return this->FormatSERDESRXPRBSErrorCountLink2(); },
	    [this] { return this->FormatSERDESRXPRBSErrorCountLink3(); },
	    [this] { return this->FormatSERDESRXPRBSErrorCountLink4(); },
	    [this] { return this->FormatSERDESRXPRBSErrorCountLink5(); },
	    [this] { return this->FormatSERDESRXPRBSErrorCountCFOLink(); },
	    [this] { return this->FormatSERDESRXCRCErrorCountLink0(); },
	    [this] { return this->FormatSERDESRXCRCErrorCountLink1(); },
	    [this] { return this->FormatSERDESRXCRCErrorCountLink2(); },
	    [this] { return this->FormatSERDESRXCRCErrorCountLink3(); },
	    [this] { return this->FormatSERDESRXCRCErrorCountLink4(); },
	    [this] { return this->FormatSERDESRXCRCErrorCountLink5(); },
	    [this] { return this->FormatSERDESRXCRCErrorCountCFOLink(); },
	    [this] { return this->FormatEVBSERDESRXPacketErrorCounter(); },
	    [this] { return this->FormatJitterAttenuatorRecoveredClockLOSCount(); },
	    [this] { return this->FormatJitterAttenuatorExternalClockLOSCount(); }

	};

	const std::vector<std::function<RegisterFormatter()>> formattedPacketCounterFunctions_{
	    // [this] { return this->FormatMissedCFOPacketCountLink0(); },
	    // [this] { return this->FormatMissedCFOPacketCountLink1(); },
	    // [this] { return this->FormatMissedCFOPacketCountLink2(); },
	    // [this] { return this->FormatMissedCFOPacketCountLink3(); },
	    // [this] { return this->FormatMissedCFOPacketCountLink4(); },
	    // [this] { return this->FormatMissedCFOPacketCountLink5(); },
	    // [this] { return this->FormatLocalFragmentDropCount(); },
	    // [this] { return this->FormatOutputBufferFragmentDumpCount(); },

	    [this] { return this->FormatCFOTXClockMarkerCountLink6(); },
	    [this] { return this->FormatTXEventWindowMarkerCountLink(DTC_Link_CFO); },
	    [this] { return this->FormatTXHeartbeatPacketCountLink(DTC_Link_CFO); },
	    [this] { return this->FormatCFOLinkError(); },

	    [this] { return this->FormatTXEventWindowMarkerCountLink(DTC_Link_0); },
	    [this] { return this->FormatTXEventWindowMarkerCountLink(DTC_Link_1); },
	    [this] { return this->FormatTXEventWindowMarkerCountLink(DTC_Link_2); },
	    [this] { return this->FormatTXEventWindowMarkerCountLink(DTC_Link_3); },
	    [this] { return this->FormatTXEventWindowMarkerCountLink(DTC_Link_4); },
	    [this] { return this->FormatTXEventWindowMarkerCountLink(DTC_Link_5); },

	    [this] { return this->FormatTXDataRequestPacketCountLink(DTC_Link_0); },
	    [this] { return this->FormatTXDataRequestPacketCountLink(DTC_Link_1); },
	    [this] { return this->FormatTXDataRequestPacketCountLink(DTC_Link_2); },
	    [this] { return this->FormatTXDataRequestPacketCountLink(DTC_Link_3); },
	    [this] { return this->FormatTXDataRequestPacketCountLink(DTC_Link_4); },
	    [this] { return this->FormatTXDataRequestPacketCountLink(DTC_Link_5); },

	    [this] { return this->FormatTXHeartbeatPacketCountLink(DTC_Link_0); },
	    [this] { return this->FormatTXHeartbeatPacketCountLink(DTC_Link_1); },
	    [this] { return this->FormatTXHeartbeatPacketCountLink(DTC_Link_2); },
	    [this] { return this->FormatTXHeartbeatPacketCountLink(DTC_Link_3); },
	    [this] { return this->FormatTXHeartbeatPacketCountLink(DTC_Link_4); },
	    [this] { return this->FormatTXHeartbeatPacketCountLink(DTC_Link_5); },

	    [this] { return this->FormatRXDataHeaderPacketCountLink(DTC_Link_0); },
	    [this] { return this->FormatRXDataHeaderPacketCountLink(DTC_Link_1); },
	    [this] { return this->FormatRXDataHeaderPacketCountLink(DTC_Link_2); },
	    [this] { return this->FormatRXDataHeaderPacketCountLink(DTC_Link_3); },
	    [this] { return this->FormatRXDataHeaderPacketCountLink(DTC_Link_4); },
	    [this] { return this->FormatRXDataHeaderPacketCountLink(DTC_Link_5); },

	    [this] { return this->FormatRXDataPacketCountLink(DTC_Link_0); },
	    [this] { return this->FormatRXDataPacketCountLink(DTC_Link_1); },
	    [this] { return this->FormatRXDataPacketCountLink(DTC_Link_2); },
	    [this] { return this->FormatRXDataPacketCountLink(DTC_Link_3); },
	    [this] { return this->FormatRXDataPacketCountLink(DTC_Link_4); },
	    [this] { return this->FormatRXDataPacketCountLink(DTC_Link_5); }

	};

	const std::vector<std::function<RegisterFormatter()>> formattedHWEventBuildingFunctions_{

	    [this] { return this->FormatEVBLocalParitionIDMACIndex(); },
	    [this] { return this->FormatEVBClusterInfo(); },
	    [this] { return this->FormatCFOTXClockMarkerCountLink6(); },
	    [this] { return this->FormatTXEventWindowMarkerCountLink(DTC_Link_CFO); },
	    [this] { return this->FormatTXHeartbeatPacketCountLink(DTC_Link_CFO); },
	    [this] { return this->FormatCFOLinkError(); },
	    [this] { return this->FormatEVBPacketControlInfo(); },
	    [this] { return this->FormatEVBStats(); },

	};
};
}  // namespace DTCLib

#endif  // DTC_REGISTERS_H
