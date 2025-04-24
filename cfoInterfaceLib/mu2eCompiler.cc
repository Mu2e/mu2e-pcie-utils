#include "cfoInterfaceLib/CFO_Compiler.hh"

#include <cstring>
#include <fstream>
#include <iostream>

std::ifstream inFile;
std::fstream setupFile;
std::ofstream outFile;
int FPGAClock = 40000000;

/********************************************
				   Program Setup Function
 ********************************************/
void help()
{
	std::ifstream setupFile("setup.in");
	std::string clockSpeed;
	std::string inputFile;
	std::string outputFile;
	getline(setupFile, clockSpeed, '=');
	getline(setupFile, clockSpeed);
	getline(setupFile, inputFile, '=');
	getline(setupFile, inputFile);
	getline(setupFile, outputFile, '=');
	getline(setupFile, outputFile);
	setupFile.close();
	std::cout << "\n***************************************" << std::endl;
	std::cout << "This is the compiler for the MU2E's CFO" << std::endl;
	std::cout << "***************************************" << std::endl;
	std::cout << "To change the default clock speed along with input and output sources use ./CFO_Compiler --new\n"
			  << std::endl;
	std::cout << "The default FPGA Clock Speed is \"" << clockSpeed << "Hz\"" << std::endl;
	std::cout << "The default input source file is \"" << inputFile << "\"" << std::endl;
	std::cout << "The default output source file is \"" << outputFile << "\"" << std::endl;
	std::cout << "\nTo use non-default input source file, use the first command line argument" << std::endl;
	std::cout << "To use non-default output file, use the second command line argument" << std::endl;
	std::cout << "Clock Speed can only be changed through the [--new] command" << std::endl;

	std::cout << "\nE.g. ./CFO_Compiler [input file name] [output binary filename]\n"
			  << std::endl;

	std::cout << "\nFor more information on this compiler, look at the CFO's Compiler and Interpreter Mu2e Guides."
			  << std::endl;
}

void newIOFiles()
{
	std::string inputFile;
	std::string outputFile;
	std::string clockSpeed;
	setupFile.open("setup.in", std::ios::out | std::ios::trunc);

	std::cout << "What's the FPGA Clock Speed Being Used (Hz)? \n(Used to calculate time units in instructions) ";
	std::cin >> clockSpeed;
	setupFile << "clockSpeed=";
	for (size_t i = 0; i < clockSpeed.size(); i++)
	{
		setupFile << clockSpeed[i];
	}
	setupFile << "\n";

	std::cout << "What's the name of DEFAULT input source? ";
	std::cin >> inputFile;
	setupFile << "InputFile=";
	for (size_t i = 0; i < inputFile.size(); i++)
	{
		setupFile << inputFile[i];
	}
	setupFile << "\n";

	std::cout << "What's the name of DEFAULT output source? ";
	std::cin >> outputFile;
	setupFile << "OutputFile=";
	for (size_t i = 0; i < outputFile.size(); i++)
	{
		setupFile << outputFile[i];
	}
	setupFile.close();
	std::cout << "DEFAULT input file: " << inputFile << std::endl;
	std::cout << "DEFAULT output file: " << outputFile << std::endl;
	std::cout << "DEFAULT Clock Speeed: " << clockSpeed << "Hz (" << atoi(clockSpeed.c_str()) / 1000000 << "MHz)"
			  << std::endl;
	std::cout
		<< "\nThanks! Run again the ./CFO_Compiler with your new DEFAULT Files"
		<< "\n*To setup new default files use \"./Compiler -new\" "
		<< "\n*To run compiler without changing the DEFAULT files, use \"./CFO_Compiler [inputsource] [outputsource]"
		<< "\n*To repeat this information or if you need help use \"./CFO_Compiler --help\"" << std::endl;
}

void setIOFiles(int argc, char** argv)
{  // Sets the Input Output Files.
	std::string inFileName;
	std::string outFileName;
	std::string clockSpeed;
	setupFile.open("setup.in");

	if (argc <= 1)
	{
		getline(setupFile, clockSpeed, '=');
		getline(setupFile, clockSpeed);
		getline(setupFile, inFileName, '=');  // ignore string before '='
		getline(setupFile, inFileName);
		getline(setupFile, outFileName, '=');
		getline(setupFile, outFileName);
	}

	else if (argc == 2)
	{
		if (strcmp(argv[1], "--help") == 0)
		{
			setupFile.close();
			help();
			exit(EXIT_SUCCESS);
		}
		else if (strcmp(argv[1], "--new") == 0)
		{
			setupFile.close();
			newIOFiles();
			exit(EXIT_SUCCESS);
		}
		else
		{
			inFileName = argv[1];
			getline(setupFile, clockSpeed, '=');
			getline(setupFile, clockSpeed);
			getline(setupFile, inFileName, '=');
			getline(setupFile, inFileName);
		}
	}

	else if (argc == 3)
	{
		inFileName = argv[1];
		outFileName = argv[2];
		getline(setupFile, clockSpeed, '=');
		getline(setupFile, clockSpeed);
	}

	else
	{
		std::cout << "Compiler can only have two or less command tail arguments:  " << std::endl;
	}

	std::cout << "setting files..." << std::endl;

	inFile.open(inFileName.c_str(), std::ios::in);
	outFile.open(outFileName.c_str(), std::ios::out | std::ios::binary);
	if (!(setupFile.is_open()))
	{
		throw std::runtime_error(
			"Setup File (setup.in) didn't open. \nDoes it exist? \nIf not: run \"./CFO_Compiler --new\"");
	}
	if (!(inFile.is_open()))
	{
		throw std::runtime_error("Input File (" + inFileName + ") didn't open. Does it exist?");
	}
	if (!(outFile.is_open()))
	{
		throw std::runtime_error("Output File (" + outFileName + ") didn't open. Does it exist?");
	}

	std::cout << "Input Source: " << inFileName << std::endl;
	std::cout << "Output Binary: " << outFileName << std::endl;
	FPGAClock = atoi(clockSpeed.c_str());
	std::cout << "FPGA Clock Speed used: " << FPGAClock << "Hz" << std::endl;
	std::cout << "Done!" << std::endl
			  << std::endl;
	setupFile.close();
}

/*******************
 **     MAIN
 ******************/
int main(int argc, char* argv[])
{
	std::cout << "Start " << std::endl;

	std::string inFileName =
		"/home/mu2ehwdev/ots/srcs/mu2e_pcie_utils/cfoInterfaceLib/Commands.txt";
	std::string outFileName =
		"/home/mu2ehwdev/ots/srcs/mu2e_pcie_utils/cfoInterfaceLib/Commands.bin";

	CFOLib::CFO_Compiler compiler;
	compiler.processFile(inFileName, outFileName);

	std::cout << "Done " << std::endl;
}
