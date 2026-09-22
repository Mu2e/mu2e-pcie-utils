#include "dtcInterfaceLib/EVBErrorStatus.h"

#include <array>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const std::string& message)
{
	if (!condition)
		throw std::runtime_error(message);
}

std::string bitLine(const std::vector<std::string>& lines, unsigned bit)
{
	const std::string prefix = "  Bit " + std::to_string(bit) + " set: ";
	for (const auto& line : lines)
		if (line.compare(0, prefix.size(), prefix) == 0)
			return line;
	return {};
}
}  // namespace

int main()
try
{
	const std::array<const char*, 32> names = {{
		"write-while-full",
		"sequence gap",
		"valid EVB frame rejected",
		"DDR write CDC FIFO empty",
		"undefined TX state",
		"sent/drained counters disagree",
		"local stream misaligned",
		"stats pipeline collision",
		"DDR read desync",
		"staging FIFO write-while-full",
		"staging FIFO empty inside TX payload",
		"TX frame malformed",
		"Rx frame size mismatch",
		"Rx FCS bad",
		"ROC tag slip",
		"ROC record shape",
		"ROC input held",
		"self-subevent throttle",
		"destination has no credit",
		"DDR channel almost-full",
		"DDR write CDC FIFO full",
		"both staging FIFOs owned",
		"Rx source buffer",
		"DMA back-pressure",
		"peer frontier known",
		"DDR calibration complete",
		"frame not for this DTC",
		nullptr,
		nullptr,
		nullptr,
		nullptr,
		nullptr,
	}};
	require(DTCLib::DecodeEVBErrorStatus(0).size() == 2, "Zero value must hide all bit details");
	for (unsigned bit = 0; bit < names.size(); ++bit)
	{
		const auto lines = DTCLib::DecodeEVBErrorStatus(uint32_t{1} << bit);
		require(lines.size() == (names[bit] ? 3u : 2u),
				"Unexpected detail count for bit " + std::to_string(bit));
		for (unsigned other = 0; other < names.size(); ++other)
		{
			const auto line = bitLine(lines, other);
			if (other == bit && names[bit])
				require(line.find(names[bit]) != std::string::npos,
						"Wrong decode for bit " + std::to_string(bit));
			else
				require(line.empty(), "Displayed an unset or reserved bit");
		}
	}

	const auto mixed = DTCLib::DecodeEVBErrorStatus(0x040007C4u);
	require(mixed.front().find("0x07c4") != std::string::npos, "Wrong sticky error mask");
	require(bitLine(mixed, 2).find("valid EVB frame rejected") != std::string::npos,
			"Bit 2 must describe rejected EVB frames");
	require(bitLine(mixed, 6).find("not dropped") != std::string::npos,
			"Bit 6 must describe forwarding, not loss at the buffer manager");
	require(bitLine(mixed, 7).find("data intact") != std::string::npos,
			"Bit 7 must distinguish stats corruption from data loss");
	require(bitLine(mixed, 26).find("sticky since SoftReset, informational") != std::string::npos,
			"Bit 26 must be sticky informational");
	const auto foreignOnly = DTCLib::DecodeEVBErrorStatus(1u << 26);
	require(foreignOnly.front().find("0x0000") != std::string::npos,
			"Foreign frames must not set the error field");
	require(foreignOnly[1].find("bit 26 sticky-informational, others live") != std::string::npos,
			"Status heading must describe the sticky exception");
	require(foreignOnly[1].find("0x0400") != std::string::npos, "Wrong status mask");
	const auto all = DTCLib::DecodeEVBErrorStatus(0xFFFFFFFFu);
	require(all.size() == 29, "All bits must display only the 27 defined details and two headings");
	require(all.front().find("0xffff") != std::string::npos, "Raw error field must show all 16 bits");
	require(bitLine(all, 4).find("TX window overrun") != std::string::npos,
	        "Bit 4 must include the 2026-09-19 window-overrun meaning");
	require(bitLine(all, 14).find("upstream") != std::string::npos,
	        "Bit 14 must say the slip originated upstream of EVB3");
	require(bitLine(all, 2).find("addressed to this DTC but source offset > 31") != std::string::npos,
	        "Bit 2 must exclude foreign frames");
	require(bitLine(all, 7).find("bit 1 and Rx BRAM rows/rates untrustworthy") != std::string::npos,
	        "Bit 7 must qualify sequence-gap and Rx statistics");
	require(bitLine(all, 12).find("without necessarily losing declared payload") != std::string::npos,
	        "A long frame need not lose declared payload");
	require(bitLine(all, 13).find("shifted 0x33 frames not checked") != std::string::npos,
	        "FCS checking must not be claimed for shifted frames");

	constexpr uint32_t calibrated = 1u << 25;
	require(!DTCLib::CheckEVBStatus(0).readyToStart(), "Missing DDR calibration must block start");
	require(DTCLib::CheckEVBStatus(calibrated).readyToStart(), "Calibrated idle DTC must be ready");
	for(unsigned bit = 0; bit < 32; ++bit)
	{
		const auto check = DTCLib::CheckEVBStatus(calibrated | (1u << bit));
		require(check.stickyErrors == (bit < 16 ? (1u << bit) : 0u),
		        "Wrong defined error mask for bit " + std::to_string(bit));
		require(check.readyToStart() == (bit >= 16),
		        "Only defined sticky errors should block a calibrated DTC");
	}
	require(!DTCLib::CheckEVBStatus(calibrated, false).missingFrontier,
	        "No frontier warning before traffic");
	require(DTCLib::CheckEVBStatus(calibrated, true).missingFrontier,
	        "Missing frontier after traffic must warn");
	require(!DTCLib::CheckEVBStatus(calibrated | (1u << 24), true).missingFrontier,
	        "Valid frontier must not warn");
	require(!DTCLib::CheckEVBStatus(calibrated, true, false).missingFrontier,
	        "Single-node partition has no peer frontier requirement");
	const auto report = DTCLib::FormatEVBStatusCheck(calibrated | 0xFFFFu, "DTC_1", true);
	require(report.find("DTC DTC_1 EVB error/status (0x9370): 0x0200ffff") != std::string::npos,
	        "Report must identify the DTC and exact raw snapshot");
	require(report.find("ROC input (upstream of EVB3)") == std::string::npos,
	        "Bits 14-15 belong in the data-corrupt group, not a separate one");
	require(report.find("ERROR: run INVALID") != std::string::npos, "Errors must invalidate run");
	require(report.find("Data lost: Bit 0 RX_BUF_WRITE_FULL; Bit 1 RX_SEQ_GAP; Bit 9 STAGING_WRITE_FULL; Bit 12 RX_FRAME_SIZE;") != std::string::npos,
	        "Wrong data-loss group");
	require(report.find("Data corrupt: Bit 3 DDR_WR_UNDERFLOW; Bit 6 LOCAL_BAD_HEADER; Bit 8 DDR_RD_BAD_COUNT; Bit 10 TX_PAYLOAD_UNDERFLOW; Bit 11 TX_FRAME_MALFORMED; Bit 13 RX_FCS_BAD; Bit 14 ROC_TAG_SLIP; Bit 15 ROC_RECORD_SHAPE;") != std::string::npos,
	        "Wrong data-corruption group");
	require(report.find("Protocol / config: Bit 2 RX_PKT_REJECTED; Bit 4 TX_FSM_FAULT; Bit 5 CREDIT_VIOLATION; Bit 7 RX_STATS_COLLISION;") != std::string::npos,
	        "Wrong protocol/config group");
	require(report.find("SoftReset all DTCs together") != std::string::npos,
	        "Report must require coordinated reset");
	const auto informational = DTCLib::FormatEVBStatusCheck(0x07FF0000u, "DTC_0", true);
	require(informational.find("ERROR:") == std::string::npos,
	        "Back-pressure and foreign frames must never invalidate a run");
	const auto plain = DTCLib::FormatEVBStatusCheck(calibrated | (1u << 11), "DTC_0", false, true, false);
	require(plain.find("TX_FRAME_MALFORMED") == std::string::npos &&
	        plain.find("Bit 11 set: TX frame malformed") != std::string::npos,
	        "GUI mode must retain plain-language descriptions without symbolic names");
	const auto noDDR = DTCLib::FormatEVBStatusCheck(0, "DTC_0");
	require(noDDR.find("DDR not calibrated; do not start") != std::string::npos,
	        "Missing calibration must be explicit");

	std::cout << "EVB error/status decoding and B3 checks passed\n";
	return 0;
}
catch (const std::exception& error)
{
	std::cerr << error.what() << '\n';
	return 1;
}
