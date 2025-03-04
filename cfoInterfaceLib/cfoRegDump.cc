// This program is a simple Register Dump module
// Eric Flumerfelt, FNAL RSI
// 5/15/2015

#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <iostream>

#include "cfoInterfaceLib/CFO_Registers.h"

void printHelpMsg()
{
	std::cout << "Usage: CFORegDump [options]" << std::endl;
	std::cout << "Options are:" << std::endl
			  << "    -h: This message." << std::endl
			  << "    -R: DON'T Print Register Dump." << std::endl
			  << "    -s: Print SERDES Byte and Packet Counters." << std::endl
			  << "    -p: Print Performance Counters." << std::endl
			  << "    -d: CFO instance to use (defaults to CFOLIB_CFO if set, 0 otherwise)" << std::endl;

	exit(0);
}

unsigned getOptionValue(int* index, char** argv[])
{
	auto arg = (*argv)[*index];
	if (arg[2] == '\0')
	{
		(*index)++;
		unsigned ret = strtoul((*argv)[*index], nullptr, 0);
		if (ret == 0 && (*argv)[*index][0] != '0')  // No option given
		{
			(*index)--;
		}
		return ret;
	}
	auto offset = 2;
	if (arg[2] == '=')
	{
		offset = 3;
	}

	return strtoul(&arg[offset], nullptr, 0);
}

int main(int argc, char* argv[])
{
	auto printSERDESCounters = false;
	auto printRegisterDump = true;
	int CFO = -1;

	for (auto optind = 1; optind < argc; ++optind)
	{
		if (argv[optind][0] == '-')
		{
			switch (argv[optind][1])
			{
				case 's':
					printSERDESCounters = true;
					break;
				case 'R':
					printRegisterDump = false;
					break;
				case 'd':
					CFO = getOptionValue(&optind, &argv);
					break;
				default:
					std::cout << "Unknown option: " << argv[optind] << std::endl;
					printHelpMsg();
					break;
				case 'h':
					printHelpMsg();
					break;
			}
		}
	}

	auto thisCFO = new CFOLib::CFO_Registers(DTCLib::DTC_SimMode_Disabled, CFO, "", true);

	auto cols = 80;
	auto lines = 24;

#ifdef TIOCGSIZE
	struct ttysize ts;
	ioctl(STDIN_FILENO, TIOCGSIZE, &ts);
	cols = ts.ts_cols;
	lines = ts.ts_lines;
#elif defined(TIOCGWINSZ)
	struct winsize ts;
	ioctl(STDIN_FILENO, TIOCGWINSZ, &ts);
	cols = ts.ws_col;
	lines = ts.ws_row;
#endif /* TIOCGSIZE */

	printf("Terminal is %dx%d\n", cols, lines);

	if (cols > 400)
	{
		cols = 120;
	}

	if (printRegisterDump)
	{
		std::cout << thisCFO->FormattedRegDump(cols, thisCFO->formattedDumpFunctions_) << std::endl;
	}

	if (printSERDESCounters)
	{
		std::cout << std::endl
				  << std::endl;
		std::cout << thisCFO->FormattedRegDump(cols, thisCFO->formattedCounterFunctions_) << std::endl;
	}

	delete thisCFO;
	return 0;
}
