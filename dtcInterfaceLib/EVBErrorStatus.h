#ifndef MU2E_PCIE_UTILS_EVBERRORSTATUS_H
#define MU2E_PCIE_UTILS_EVBERRORSTATUS_H

#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace DTCLib
{
// All 16 low bits are defined errors (bits 14-15 ROC_TAG_SLIP / ROC_RECORD_SHAPE added
// 2026-09-16); any nonzero [15:0] invalidates the run.  B3 mask corrected to 0xFFFF
// by the hw agent 2026-09-21.
constexpr uint32_t EVBDefinedErrorMask = 0xFFFFu;

struct EVBStatusCheck
{
	uint32_t stickyErrors;
	bool ddrCalibrated;
	bool missingFrontier;

	bool readyToStart() const { return stickyErrors == 0 && ddrCalibrated; }
};

// Evaluate one caller-owned snapshot; never read or clear hardware here.
inline EVBStatusCheck CheckEVBStatus(uint32_t value, bool trafficStarted = false,
                                    bool hasPeers = true)
{
	return {value & EVBDefinedErrorMask, (value & (1u << 25)) != 0,
	        trafficStarted && hasPeers && !(value & (1u << 24))};
}

inline std::vector<std::string> DecodeEVBErrorStatus(uint32_t value)
{
	auto hexField = [](uint32_t field) {
		std::ostringstream output;
		output << "0x" << std::hex << std::setfill('0') << std::setw(4) << field;
		return output.str();
	};
	std::vector<std::string> lines;
	lines.push_back("Sticky errors [15:0] (any set invalidates run; SoftReset-only clear): " +
	                hexField(value & 0xFFFFu));
	static const char* const errorNames[] = {
	    "Rx source buffer write-while-full; words lost",
	    "Rx sequence gap; packet loss indication, unreliable if bit 7 is set; confirm with word-count parity",
	    "valid EVB frame rejected: addressed to this DTC but source offset > 31; check peer MAC and start node",
	    "DDR write CDC FIFO empty inside burst; corrupt burst",
	    "undefined TX state or destination offset > 31; or TX window overrun: this DTC's frame was still on the wire when its destination window closed (sender-side flag; through a switch the colliding frame is dropped uncounted)",
	    "sent/drained counters disagree beyond credit; one-sided reset",
	    "local stream misaligned; affected record corrupt, word forwarded as a 1-word chunk, not dropped",
	    "Rx stats pipeline collision; bit 1 and Rx BRAM rows/rates untrustworthy, data intact",
	    "DDR read desync; bad count quadword dropped, resync at next valid count",
	    "DDR-to-TX staging FIFO write-while-full; words lost",
	    "staging FIFO empty inside TX payload; wire repeated a word",
	    "TX frame malformed; illegal start/terminate, idle inside frame, data outside frame, or > 200 blocks",
	    "Rx frame size mismatch after legal padding; short frame loses data, long frame truncated to declared size without necessarily losing declared payload",
	    "Rx FCS bad on accepted 0x78 frame; corrupt data already stored; shifted 0x33 frames not checked",
	    "ROC tag slip at EVB3 input: header EWT != first ROC fragment EWT; slip originated upstream in RingController/AXIMux",
	    "ROC record shape at EVB3 input: accepted beats != count quadword, or header byte count < count quadword",
	};
	for(std::size_t bit = 0; bit < sizeof(errorNames) / sizeof(errorNames[0]); ++bit)
		if((value >> bit) & 1u)
			lines.push_back("  Bit " + std::to_string(bit) + " set: " + errorNames[bit]);

	lines.push_back("Status [31:16] (user_clk-synced; bit 26 sticky-informational, others live): " +
	                hexField(value >> 16));
	static const char* const statusNames[] = {
	    "ROC input held: tvalid and not tready; downstream back-pressure",
	    "self-subevent throttle holding a subevent; a peer is behind",
	    "window has data, destination has no credit; peer DMA not draining",
	    "DDR channel almost-full back-pressure",
	    "DDR write CDC FIFO full",
	    "HEADER waiting; both staging FIFOs owned by other destinations",
	    "any Rx source buffer >= 3/4 full; this DTC's DMA not draining",
	    "DMA back-pressure: m_axis_tvalid and not tready; software reading too slowly",
	    "peer frontier known: min event tag delivered by all peer DTCs; self-throttle armed",
	    "DDR calibration complete",
	    "frame not for this DTC: non-EVB start, or destination MAC DTC byte / partition byte not ours; sticky since SoftReset, informational; normal on a shared switch",
	};
	for(std::size_t index = 0; index < sizeof(statusNames) / sizeof(statusNames[0]); ++index)
	{
		const std::size_t bit = 16 + index;
		if((value >> bit) & 1u)
			lines.push_back("  Bit " + std::to_string(bit) + " set: " + statusNames[index]);
	}
	return lines;
}

inline std::string FormatEVBStatusCheck(uint32_t value, const std::string& dtc,
                                        bool trafficStarted = false, bool hasPeers = true,
                                        bool includeBitNames = true,
                                        uint32_t ignoreMask = 0)
{
	const auto check  = CheckEVBStatus(value & ~ignoreMask, trafficStarted, hasPeers);
	const uint32_t ignored = value & EVBDefinedErrorMask & ignoreMask;
	std::ostringstream output;
	output << "DTC " << dtc << " EVB error/status (0x9370): 0x" << std::hex
	       << std::setw(8) << std::setfill('0') << value << std::dec << std::setfill(' ') << "\n";
	if(check.stickyErrors)
	{
		output << "ERROR: run INVALID. Stop traffic, then SoftReset all DTCs together before the next first event; never write 0x9370 to clear.\n";
		static const char* const names[] = {
		    "RX_BUF_WRITE_FULL", "RX_SEQ_GAP", "RX_PKT_REJECTED", "DDR_WR_UNDERFLOW",
		    "TX_FSM_FAULT", "CREDIT_VIOLATION", "LOCAL_BAD_HEADER", "RX_STATS_COLLISION",
		    "DDR_RD_BAD_COUNT", "STAGING_WRITE_FULL", "TX_PAYLOAD_UNDERFLOW",
		    "TX_FRAME_MALFORMED", "RX_FRAME_SIZE", "RX_FCS_BAD",
		    "ROC_TAG_SLIP", "ROC_RECORD_SHAPE",
		};
		const auto group = [&](const char* label, uint32_t mask) {
			if(!(check.stickyErrors & mask)) return;
			output << "  " << label << ":";
			for(std::size_t bit = 0; bit < sizeof(names) / sizeof(names[0]); ++bit)
				if(check.stickyErrors & mask & (1u << bit))
				{
					output << " Bit " << bit;
					if(includeBitNames) output << " " << names[bit];
					output << ";";
				}
			output << "\n";
		};
		group("Data lost", (1u << 0) | (1u << 1) | (1u << 9) | (1u << 12));
		// bits 14-15 (ROC input checks, fault already present upstream of EVB3) are in the
		// "data corrupt" group per the corrected B3 text (hw agent, 2026-09-21)
		group("Data corrupt", (1u << 3) | (1u << 6) | (1u << 8) | (1u << 10) | (1u << 11) |
		                          (1u << 13) | (1u << 14) | (1u << 15));
		group("Protocol / config", (1u << 2) | (1u << 4) | (1u << 5) | (1u << 7));
		output << "Use end-of-run word-count parity as the authoritative loss check; 0x9370 identifies the stage.\n";
	}
	else
		output << "No defined sticky errors; this alone does not establish loss-free data.\n";
	if(ignored)
		output << "Informational (excluded from run validity): 0x" << std::hex << ignored << std::dec << "\n";
	if(!check.ddrCalibrated)
		output << "ERROR: Bit 25 clear: DDR not calibrated; do not start.\n";
	if(check.missingFrontier)
		output << "WARN: Bit 24 still clear after traffic began: no peer traffic accepted; check link, MAC, partition and start node.\n";
	output << "Back-pressure bits 16-23 and foreign-frame bit 26 are informational, not failures; blocked-stage hints matter only when levels stay high while event counts stop.\n";
	for(const auto& line : DecodeEVBErrorStatus(value))
		output << line << "\n";
	return output.str();
}
}  // namespace DTCLib

#endif
