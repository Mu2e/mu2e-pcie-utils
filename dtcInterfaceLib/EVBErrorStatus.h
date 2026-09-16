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
inline std::vector<std::string> DecodeEVBErrorStatus(uint32_t value)
{
	auto hexField = [](uint32_t field) {
		std::ostringstream output;
		output << "0x" << std::hex << std::setfill('0') << std::setw(4) << field;
		return output.str();
	};
	std::vector<std::string> lines;
	lines.push_back("Sticky errors [15:0] (since SoftReset; nonzero invalidates run): " +
	                hexField(value & 0xFFFFu));
	static const char* const errorNames[] = {
	    "Rx source buffer write-while-full; words lost",
	    "Rx sequence gap; packet lost on wire",
	    "valid EVB frame rejected: source offset > 31 or addressed to another DTC",
	    "DDR write CDC FIFO empty inside burst; corrupt burst",
	    "undefined TX state or destination offset > 31",
	    "sent/drained counters disagree beyond credit; one-sided reset",
	    "local stream misaligned; word forwarded as a 1-word chunk, not dropped",
	    "Rx stats pipeline collision; stats rows may be wrong, data intact",
	    "DDR read desync; bad count quadword dropped, resync at next valid count",
	    "DDR-to-TX staging FIFO write-while-full; words lost",
	    "staging FIFO empty inside TX payload; wire repeated a word",
	};
	for(std::size_t bit = 0; bit < sizeof(errorNames) / sizeof(errorNames[0]); ++bit)
		if((value >> bit) & 1u)
			lines.push_back("  Bit " + std::to_string(bit) + " set: " + errorNames[bit]);

	lines.push_back("Status [31:16] (user_clk-synced; bit 26 sticky-informational, others live): " +
	                hexField(value >> 16));
	static const char* const statusNames[] = {
	    "ROC input held: tvalid and not tready",
	    "self-subevent throttle holding a subevent",
	    "window has data, destination has no credit",
	    "DDR channel almost-full back-pressure",
	    "DDR write CDC FIFO full",
	    "HEADER waiting; both staging FIFOs owned by other destinations",
	    "any Rx source buffer >= 3/4 full",
	    "DMA back-pressure: m_axis_tvalid and not tready",
	    "peer frontier known: min event tag delivered by all peer DTCs; self-throttle armed",
	    "DDR calibration complete",
	    "frame not for this DTC: non-EVB start, or destination MAC DTC byte / partition byte not ours; sticky since SoftReset, informational",
	};
	for(std::size_t index = 0; index < sizeof(statusNames) / sizeof(statusNames[0]); ++index)
	{
		const std::size_t bit = 16 + index;
		if((value >> bit) & 1u)
			lines.push_back("  Bit " + std::to_string(bit) + " set: " + statusNames[index]);
	}
	return lines;
}
}  // namespace DTCLib

#endif
