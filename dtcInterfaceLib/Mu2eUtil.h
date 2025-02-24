#ifndef MU2EUTIL_H
#define MU2EUTIL_H

#include <unistd.h>  // usleep
#include <chrono>
#include <cmath>
#include <cstdio>   // printf
#include <cstdlib>  // strtoul
#include <fstream>
#include <iomanip>
#include <iostream>
#include <set>
#include <string>

#include "TRACE/tracemf.h"
#define TRACE_NAME "mu2eUtil"

#include "DTC.h"
#include "DTCSoftwareCFO.h"
#include "DTC_Data_Verifier.h"

namespace DTCLib {

class Mu2eUtil
{
public:
	void parse_arguments(int argc, char** argv);
	void run();

private:
	void read_data();

	void toggle_serdes();
	void reset_ddr();

	void reset_detector_emulator();

	void verify_sim_file();

	void verify_stream();

	void buffer_test();

	void read_release();

	void program_clock();

	void dma_info();

	void printHelpMsg();

	mu2e_databuff_t* readDTCBuffer(mu2edev* device, bool& readSuccess, bool& timeout, size_t& sts, bool continuedMode);

	bool incrementTimestamp = true;
	bool syncRequests = false;
	bool checkSERDES = false;
	bool quiet = false;
	unsigned quietCount = 0;
	bool reallyQuiet = false;
	bool rawOutput = false;
	bool binaryFileOutput = false;
	bool skipVerify = false;
	bool stopOnVerifyFailure = false;
	bool stopOnTimeout = false;
	bool writeDMAHeadersToOutput = false;
	bool useCFOEmulator = true;
	bool forceNoDebug = false;
	std::string rawOutputFile = "/tmp/mu2eUtil.raw";
	std::string expectedDesignVersion = "";
	std::string simFile = "";
	std::string timestampFile = "";
	bool useSimFile = false;
	unsigned delay = 0;
	unsigned cfodelay = 1000;
	unsigned number = 1;
	unsigned extraReads = 1;
	unsigned long timestampOffset = 1;
	unsigned eventCount = 1;
	unsigned blockCount = 1;
	unsigned packetCount = 0;
	int requestsAhead = 0;
	unsigned heartbeatsAfter = 16;
	std::string op = "";
	DTC_DebugType debugType = DTC_DebugType_SpecialSequence;
	bool stickyDebugType = true;
	int val = 0;
	bool readGenerated = false;
	std::ofstream outputStream;
	unsigned rocMask = 0x1;
	unsigned targetFrequency = 166666667;
	int clockToProgram = 0;
	bool useCFODRP = false;

	int dtc = -1;
};
}  // namespace DTCLib

#endif
