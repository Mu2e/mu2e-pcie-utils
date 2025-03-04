// mu2esim.hh: mu2e DTC simulator main file
//
// Eric Flumerfelt
// January 27, 2015
//
#ifndef MU2ESIM_HH
#define MU2ESIM_HH 1

#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>

// #include "artdaq-core-mu2e/Overlays/DTC_Packets.h"
#include "artdaq-core-mu2e/Overlays/DTC_Packets/DTC_DataBlock.h"
#include "artdaq-core-mu2e/Overlays/DTC_Packets/DTC_DataHeaderPacket.h"
#include "artdaq-core-mu2e/Overlays/DTC_Packets/DTC_DataPacket.h"
#include "artdaq-core-mu2e/Overlays/DTC_Packets/DTC_DataRequestPacket.h"
#include "artdaq-core-mu2e/Overlays/DTC_Packets/DTC_DataStatus.h"
#include "artdaq-core-mu2e/Overlays/DTC_Packets/DTC_DCSReplyPacket.h"
#include "artdaq-core-mu2e/Overlays/DTC_Packets/DTC_DCSRequestPacket.h"
#include "artdaq-core-mu2e/Overlays/DTC_Packets/DTC_DMAPacket.h"
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
#include "artdaq-core-mu2e/Overlays/DTC_Types/DTC_SerdesClockSpeed.h"
#include "artdaq-core-mu2e/Overlays/DTC_Types/DTC_SERDESLoopbackMode.h"
#include "artdaq-core-mu2e/Overlays/DTC_Types/DTC_SERDESRXDisparityError.h"
#include "artdaq-core-mu2e/Overlays/DTC_Types/DTC_SimMode.h"
#include "artdaq-core-mu2e/Overlays/DTC_Types/DTC_Subsystem.h"
#include "artdaq-core-mu2e/Overlays/DTC_Types/Exceptions.h"
#include "artdaq-core-mu2e/Overlays/DTC_Types/Utilities.h"

#include "mu2e_driver/mu2e_mmap_ioctl.h"  //

#define SIM_BUFFCOUNT 40U

/// <summary>
/// The mu2esim class emulates a DTC in software. It can be used for hardware-independent testing of software,
/// especially higher-level trigger algorithms.
/// </summary>
class mu2esim
{
public:
	/// <summary>
	/// Construct the mu2esim class. Initializes register space and zeroes out memory.
	/// <param name="ddrFileName">Name of the simulated DDR memory file</param>
	/// </summary>
	mu2esim(std::string ddrFileName);
	~mu2esim();
	/// <summary>
	/// Initialize the simulator using the given simulation mode
	/// </summary>
	/// <param name="mode">Simulation mode to set</param>
	/// <returns>0 when successful (always)</returns>
	int init(DTCLib::DTC_SimMode mode = DTCLib::DTC_SimMode_Tracker);
	/// <summary>
	/// Reads data from the simulated DDR memory or using a packet emulator based on the simulation mode selected.
	/// Returns the number of bytes read. Negative values indicate errors.
	/// </summary>
	/// <param name="chn">Channel to read</param>
	/// <param name="buffer">Pointer to output buffer</param>
	/// <param name="tmo_ms">Timeout for read</param>
	/// <returns>Byte count of data read into buffer. Negative value indicates error.</returns>
	int read_data(int chn, void** buffer, int tmo_ms);
	/// <summary>
	/// Write data from the given buffer to the requested channel. The simulator will process the packets and enqueue
	/// appropriate responses.
	/// </summary>
	/// <param name="chn">Channel to write data to</param>
	/// <param name="buffer">Address of buffer to write</param>
	/// <param name="bytes">Bytes to write</param>
	/// <returns>0 when successful (always)</returns>
	int write_data(int chn, void* buffer, size_t bytes);
	/// <summary>
	/// Release a number of buffers held by the software on the given channel
	/// </summary>
	/// <param name="chn">Channel to release</param>
	/// <param name="num">Number of buffers to release</param>
	/// <returns>0 when successful (always)</returns>
	int read_release(int chn, unsigned num);
	/// <summary>
	/// Release all buffers held by the software on the given channel
	/// </summary>
	/// <param name="chn">Channel to release</param>
	/// <returns>0 when successful (always)</returns>
	int release_all(int chn);
	/// <summary>
	/// Read from the simulated register space
	/// </summary>
	/// <param name="address">Address to read</param>
	/// <param name="tmo_ms">timeout for read</param>
	/// <param name="output">Output pointer</param>
	/// <returns>0 when successful (always)</returns>
	int read_register(uint16_t address, int tmo_ms, uint32_t* output);
	/// <summary>
	/// Write to the simulated register space
	/// </summary>
	/// <param name="address">Address to write</param>
	/// <param name="tmo_ms">Timeout for write</param>
	/// <param name="data">Data to write</param>
	/// <returns>0 when successful (always)</returns>
	int write_register(uint16_t address, int tmo_ms, uint32_t data);

private:
	unsigned delta_(int chn, int dir);
	static void clearBuffer_(int chn, bool increment = true);
	void openEvent_(DTCLib::DTC_EventWindowTag ts);
	void closeEvent_();
	void closeSubEvent_();
	DTCLib::DTC_EventMode getEventMode_();
	void CFOEmulator_();
	void packetSimulator_(DTCLib::DTC_EventWindowTag ts, DTCLib::DTC_Link_ID link, uint16_t packetCount);
	void dcsPacketSimulator_(DTCLib::DTC_DCSRequestPacket in, DTCLib::DTC_Link_ID rocLink);

	void eventSimulator_(DTCLib::DTC_EventWindowTag ts);
	void trackerBlockSimulator_(DTCLib::DTC_EventWindowTag ts, DTCLib::DTC_Link_ID link, int DTCID);
	void calorimeterBlockSimulator_(DTCLib::DTC_EventWindowTag ts, DTCLib::DTC_Link_ID link, int DTCID);
	void crvBlockSimulator_(DTCLib::DTC_EventWindowTag ts, DTCLib::DTC_Link_ID link, int DTCID);

	void reopenDDRFile_();

	std::unordered_map<uint16_t /* DTC address */, uint32_t /* DTC data */> registers_;
	std::unordered_map<DTCLib::DTC_Link_ID,
					   std::unordered_map<uint16_t /* ROC address*/, uint16_t /* ROC data*/>>
		rocRegisters_;
	unsigned swIdx_[MU2E_MAX_CHANNELS];
	unsigned hwIdx_[MU2E_MAX_CHANNELS];
	// uint32_t detSimLoopCount_;
	uint32_t simDataEWT_ = 0;
	mu2e_databuff_t* dmaData_[MU2E_MAX_CHANNELS][SIM_BUFFCOUNT];
	std::string ddrFileName_;
	std::unique_ptr<std::fstream> ddrFile_;
	DTCLib::DTC_SimMode mode_;
	std::thread cfoEmulatorThread_;
	bool cancelCFO_;

	size_t event_mode_num_tracker_blocks_;
	size_t event_mode_num_calo_blocks_;
	uint16_t event_mode_num_calo_hits_;
	size_t event_mode_num_crv_blocks_;

	typedef std::bitset<6> readoutRequestData;
	std::map<uint64_t, readoutRequestData> readoutRequestReceived_;

	std::unique_ptr<DTCLib::DTC_Event> event_;
	std::unique_ptr<DTCLib::DTC_SubEvent> sub_event_;
};

#endif
