// This file (util_main.cc) was created by Ron Rechenmacher <ron@fnal.gov> on
// May 13, 2015. "TERMS AND CONDITIONS" governing this file are in the README
// or COPYING file. If you do not have such a file, one can be obtained by
// contacting Ron or Fermi Lab in Batavia IL, 60510, phone: 630-840-3000.
// $RCSfile: .emacs.gnu,v $
// rev="$Revision: 1.23 $$Date: 2012/01/23 15:32:40 $";

#include "dtcInterfaceLib/DTC_Data_Verifier.h"

#include "TRACE/tracemf.h"
#define TRACE_NAME "data_file_verifier"

void printHelpMsg()
{
	std::cout << "Verifies the DMA and Data block content of DTC binary file(s)" << std::endl;
	std::cout << "Usage: data_file_verifier [options] file.."
			  << std::endl;
	std::cout
		<< "Options are:" << std::endl
		<< "    -h, --help: This message." << std::endl
		<< std::endl;
	exit(0);
}

int main(int argc, char* argv[])
{
	std::vector<std::string> binaryFiles;
	for (auto optind = 1; optind < argc; ++optind)
	{
		if (argv[optind][0] == '-')
		{
			switch (argv[optind][1])
			{
				case '-':  // Long option
				{
					auto option = DTCLib::Utilities::getLongOptionOption(&optind, &argv);
					if (option == "--help")
					{
						printHelpMsg();
					}
					break;
				}
				default:
					TLOG(TLVL_ERROR) << "Unknown option: " << argv[optind] << std::endl;
					printHelpMsg();
					break;
				case 'h':
					printHelpMsg();
					break;
			}
		}
		else
		{
			binaryFiles.push_back(std::string(argv[optind]));
		}
	}

	for (auto& file : binaryFiles)
	{
		DTCLib::DTC_Data_Verifier verifier;
		verifier.VerifyFile(file);
	}
}
