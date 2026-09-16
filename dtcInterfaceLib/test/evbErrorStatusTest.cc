#include "dtcInterfaceLib/EVBErrorStatus.h"

#include <array>
#include <iostream>
#include <stdexcept>

namespace
{
void require(bool condition, const std::string& message)
{
	if(!condition)
		throw std::runtime_error(message);
}

std::string bitLine(const std::vector<std::string>& lines, unsigned bit)
{
	const std::string prefix = "  Bit " + std::to_string(bit) + " set: ";
	for(const auto& line : lines)
		if(line.compare(0, prefix.size(), prefix) == 0)
			return line;
	return {};
}
}

int main()
try
{
	const std::array<const char*, 32> names = {{
	    "write-while-full", "sequence gap", "valid EVB frame rejected", "DDR write CDC FIFO empty",
	    "undefined TX state", "sent/drained counters disagree", "local stream misaligned", "stats pipeline collision",
	    "DDR read desync", "staging FIFO write-while-full", "staging FIFO empty inside TX payload",
	    nullptr, nullptr, nullptr, nullptr, nullptr,
	    "ROC input held", "self-subevent throttle", "destination has no credit", "DDR channel almost-full",
	    "DDR write CDC FIFO full", "both staging FIFOs owned", "Rx source buffer", "DMA back-pressure",
	    "peer frontier known", "DDR calibration complete", "frame not for this DTC",
	    nullptr, nullptr, nullptr, nullptr, nullptr,
	}};
	require(DTCLib::DecodeEVBErrorStatus(0).size() == 2, "Zero value must hide all bit details");
	for(unsigned bit = 0; bit < names.size(); ++bit)
	{
		const auto lines = DTCLib::DecodeEVBErrorStatus(uint32_t{1} << bit);
		require(lines.size() == (names[bit] ? 3u : 2u),
		        "Unexpected detail count for bit " + std::to_string(bit));
		for(unsigned other = 0; other < names.size(); ++other)
		{
			const auto line = bitLine(lines, other);
			if(other == bit && names[bit])
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
	require(all.size() == 24, "All bits must display only the 22 defined details and two headings");
	require(all.front().find("0xffff") != std::string::npos, "Reserved error bits must remain in raw field");

	std::cout << "EVB error/status decoding passed: zero, all 32 single bits, mixed flags, and all bits\n";
	return 0;
}
catch(const std::exception& error)
{
	std::cerr << error.what() << '\n';
	return 1;
}
