#include <assert.h>
#include <math.h>
#include <bitset>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <vector>

typedef uint16_t adc_t;

/// Struct to hold information from simulated CAL digis read in from external file
struct calhit
{
	int evt;                       ///< Event Number
	int crystalId;                 ///< Crystal ID
	int recoDigiId;                ///< Digit ID
	int recoDigiT0;                ///< TDC Value
	int recoDigiSamples;           ///< Sample count
	std::vector<double> waveform;  ///< Reconstructed waveform

	int rocID;   ///< ROC ID of CAL digit
	int linkID;  ///< Link ID of CAL digit
	int apdID;   ///< APD ID of CAL digit
};

/// Struct to hold information from simulated TRK digis read in from external file
struct trkhit
{
	int evt;                       ///< Event Number
	int strawIdx;                  ///< Straw Index
	int recoDigiT0;                ///< TDC Value 0
	int recoDigiT1;                ///< TDC Value 1
	int recoDigiSamples;           ///< Sample count
	std::vector<double> waveform;  ///< Reconstructed waveform

	int rocID;   ///< ROC ID of TRK digit
	int linkID;  ///< Link ID of TRK digit
};

// Waveform for TRK:
double f(double t, double tau, double sigma, double offset);
// Waveform for CAL
double logn(double x, double eta, double sigma, double Epeak, double norm);
// CAL Digitizer specs:
//    200M samples per second => 5ns steps
//    Zero-suppression on board
//    12 bit resolution over 2V dynamic range

std::vector<adc_t> generateDMABlockHeader(size_t byteCount);

enum PacketType
{
	PacketType_TRK = 1,
	PacketType_CAL = 2,
};

unsigned getOptionValue(int* index, char** argv[])
{
	char* arg = (*argv)[*index];
	if (arg[2] == '\0')
	{
		(*index)++;
		return strtoul((*argv)[*index], nullptr, 0);
	}
	int offset = 2;
	if (arg[2] == '=')
	{
		offset = 3;
	}

	return strtoul(&(arg[offset]), nullptr, 0);
}

std::string getOptionString(int* index, char** argv[])
{
	char* arg = (*argv)[*index];
	if (arg[2] == '\0')
	{
		(*index)++;
		return std::string((*argv)[*index]);
	}
	int offset = 2;
	if (arg[2] == '=')
	{
		offset = 3;
	}

	return std::string(&(arg[offset]));
}

void printHelpMsg()
{
	std::cout << "Usage: packetGenerator [options]" << std::endl;
	std::cout << "Options are:" << std::endl
			  << "    -h: This message." << std::endl
			  << "    -n <number>: Number of timestamps to generate. (Default: 200000)" << std::endl
			  << "    -o <number>: Starting Timestamp offest. (Default: 1)." << std::endl
			  << "    -v: Verbose mode" << std::endl
			  << "    -V: Ridiculously verbose mode" << std::endl
			  << "    -f <path>: Filename for output" << std::endl
			  << "    -F: Do not output a file" << std::endl
			  << "    -S: <number>: PRNG seed value" << std::endl
			  << "    -t: Generate Tracker packets (this is the default, default file name is TRK_packets.bin)"
			  << std::endl
			  << "    -c: Generate Calorimeter packets (default file name is CAL_packets.bin)" << std::endl;
	exit(0);
}

int main(int argc, char** argv)
{
	bool verbose = false;
	bool veryverbose = false;
	bool save_adc_values = true;
	double nevents = 200000;  // Number of hits to generate

	bool read_cal_digis_from_file = true;
	std::string inputCalDigiFile = "artdaq_cut_cal.txt";
	std::ifstream inputCalDigiStream;
	std::vector<calhit> calHitVector;                  // Vector of cal hit digi data
	std::vector<std::vector<calhit> > calEventVector;  // Vector of vectors of cal hit digi data for each event
	// For now, crystal IDs start at minCrystalID and end at minCrystalID+number_of_links*rocs_per_link-1
	size_t number_of_crystals_per_roc = 10;  // 20 channels, 2 APDs per crystal
	size_t minCrystalID = 200;

	bool read_trk_digis_from_file = true;
	std::string inputTrkDigiFile = "artdaq_cut_trk.txt";
	std::ifstream inputTrkDigiStream;
	std::vector<trkhit> trkHitVector;                  // Vector of trk hit digi data
	std::vector<std::vector<trkhit> > trkEventVector;  // Vector of vectors of cal hit digi data for each event
	// For now, crystal IDs start at minCrystalID and end at minCrystalID+number_of_links*rocs_per_link-1
	size_t number_of_straws_per_roc = 96;  // Each panel in the tracker has 96 straws
	size_t minStrawID = 960;

	size_t SEED = 100;  // Random seed

	// Packet type to generate
	PacketType packetType = PacketType_TRK;

	std::string outputFile = "";
	//	size_t numADCSamples = 8;
	size_t numADCSamples = 12;

	// The timestamp count will be offset by the following amount:
	size_t starting_timestamp = 1;

	for (int optind = 1; optind < argc; ++optind)
	{
		if (argv[optind][0] == '-')
		{
			switch (argv[optind][1])
			{
				case 'n':
					nevents = getOptionValue(&optind, &argv);
					break;
				case 'o':
					starting_timestamp = getOptionValue(&optind, &argv);
					break;
				case 'v':
					verbose = true;
					break;
				case 'V':
					verbose = true;
					veryverbose = true;
					break;
				case 'f':
					save_adc_values = true;
					outputFile = getOptionString(&optind, &argv);
					break;
				case 'F':
					save_adc_values = false;
					break;
				case 't':
					packetType = PacketType_TRK;
					break;
				case 'c':
					packetType = PacketType_CAL;
					break;
				case 'S':
					SEED = getOptionValue(&optind, &argv);
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

	size_t max_DMA_block_size = 32000;  // Maximum size in bytes of a DMA block
	// Normally a DMA block begins when a new timestamp begins, however
	// if the size of the DMA block will exceed the limit within the current
	// timestamp, a new block is created

	if (outputFile == "" && packetType == PacketType_TRK)
	{
		outputFile = "TRK_packets.bin";
	}
	else if (outputFile == "" && packetType == PacketType_CAL)
	{
		outputFile = "CAL_packets.bin";
	}
	std::ofstream binFile;
	if (save_adc_values)
	{
		binFile.open(outputFile, std::ios::out | std::ios::app | std::ios::binary);
	}

	int number_of_links = 6;
	int rocs_per_link = 5;

	if (read_cal_digis_from_file && packetType == PacketType_CAL)
	{
		inputCalDigiStream.open(inputCalDigiFile, std::ifstream::in);
		int curEvt;
		while (inputCalDigiStream >> curEvt)
		{
			calhit curHit;
			curHit.evt = curEvt;
			inputCalDigiStream >> curHit.crystalId;
			inputCalDigiStream >> curHit.recoDigiId;
			inputCalDigiStream >> curHit.recoDigiT0;
			inputCalDigiStream >> curHit.recoDigiSamples;
			double curSample;
			for (int curSampleNum = 0; curSampleNum < curHit.recoDigiSamples; curSampleNum++)
			{
				inputCalDigiStream >> curSample;
				curHit.waveform.push_back(curSample);
			}

			curHit.linkID = int((curHit.crystalId - minCrystalID) / (rocs_per_link * number_of_crystals_per_roc));
			curHit.rocID =
				((curHit.crystalId - minCrystalID) - (rocs_per_link * number_of_crystals_per_roc) * curHit.linkID) /
				number_of_crystals_per_roc;
			curHit.apdID = curHit.recoDigiId % 2;  // Even is APD 0, Odd is APD 1

			calHitVector.push_back(curHit);
		}
		inputCalDigiStream.close();

		size_t firstEvtNum = calHitVector[0].evt;
		size_t lastEvtNum = calHitVector[calHitVector.size() - 1].evt;

		std::cout << "Read in " << calHitVector.size() << " calorimeter digi hits" << std::endl;
		std::cout << "First CAL event num: " << firstEvtNum << std::endl;
		std::cout << "Last CAL event num: " << lastEvtNum << std::endl;

		if (lastEvtNum < nevents)
		{
			std::cout << "WARNING: nevents is greater than number of events in CAL digi file. "
					  << "Setting nevents to " << lastEvtNum + 1 << std::endl;
			nevents = lastEvtNum + 1;
		}

		std::vector<calhit> curEventVector;
		size_t prevEvent = calHitVector[0].evt;
		size_t curEvent;
		for (size_t curHitVecPos = 0; curHitVecPos < calHitVector.size(); curHitVecPos++)
		{
			curEvent = calHitVector[curHitVecPos].evt;
			if (curEvent != prevEvent)
			{
				calEventVector.push_back(curEventVector);
				curEventVector.clear();
				prevEvent = curEvent;
			}
			curEventVector.push_back(calHitVector[curHitVecPos]);
		}
		calEventVector.push_back(curEventVector);

		std::cout << "Length of calEventVector: " << calEventVector.size() << std::endl;
	}

	if (read_trk_digis_from_file && packetType == PacketType_TRK)
	{
		inputTrkDigiStream.open(inputTrkDigiFile, std::ifstream::in);
		int curEvt;
		while (inputTrkDigiStream >> curEvt)
		{
			trkhit curHit;
			curHit.evt = curEvt;
			inputTrkDigiStream >> curHit.strawIdx;
			inputTrkDigiStream >> curHit.recoDigiT0;
			inputTrkDigiStream >> curHit.recoDigiT1;
			inputTrkDigiStream >> curHit.recoDigiSamples;

			// The tracker should have a fixed number of digitizer samples (12)
			assert(static_cast<int>(numADCSamples) == curHit.recoDigiSamples);

			double curSample;
			for (int curSampleNum = 0; curSampleNum < curHit.recoDigiSamples; curSampleNum++)
			{
				inputTrkDigiStream >> curSample;
				curHit.waveform.push_back(curSample);
			}

			curHit.linkID = int((curHit.strawIdx - minStrawID) / (rocs_per_link * number_of_straws_per_roc));
			curHit.rocID = ((curHit.strawIdx - minStrawID) - (rocs_per_link * number_of_straws_per_roc) * curHit.linkID) /
						   number_of_straws_per_roc;

			trkHitVector.push_back(curHit);
		}
		inputTrkDigiStream.close();

		size_t firstEvtNum = trkHitVector[0].evt;
		size_t lastEvtNum = trkHitVector[trkHitVector.size() - 1].evt;

		std::cout << "Read in " << trkHitVector.size() << " tracker digi hits" << std::endl;
		std::cout << "First TRK event num: " << firstEvtNum << std::endl;
		std::cout << "Last TRK event num: " << lastEvtNum << std::endl;

		if (lastEvtNum < nevents)
		{
			std::cout << "WARNING: nevents is greater than number of events in TRK digi file. "
					  << "Setting nevents to " << lastEvtNum + 1 << std::endl;
			nevents = lastEvtNum + 1;
		}

		std::vector<trkhit> curEventVector;
		size_t prevEvent = trkHitVector[0].evt;
		size_t curEvent;
		for (size_t curHitVecPos = 0; curHitVecPos < trkHitVector.size(); curHitVecPos++)
		{
			curEvent = trkHitVector[curHitVecPos].evt;
			if (curEvent != prevEvent)
			{
				trkEventVector.push_back(curEventVector);
				curEventVector.clear();
				prevEvent = curEvent;
			}
			curEventVector.push_back(trkHitVector[curHitVecPos]);
		}
		trkEventVector.push_back(curEventVector);

		std::cout << "Length of trkEventVector: " << trkEventVector.size() << std::endl;
	}

	double peakVal;
	int noise_level;
	int sigma_noise;
	double threshold;
	auto nominal_tau = -0.9;
	double nominal_sigma;
	double nominal_offset = 10;
	auto nominal_eta = 0.03;
	double nominal_Epeak = 10;
	double nominal_norm = 1000;
	double sigma_tau = 2;
	double sigma_sigma;
	double sigma_offset = 2;
	auto sigma_eta = 0.03;
	double sigma_Epeak = 10;
	double sigma_norm = 15;
	double minTime;
	double maxTime;
	double stepSize;

	switch (packetType)
	{
		case PacketType_TRK:
			// Set maximum waveform value for scaling purposes
			peakVal = 0.02;

			noise_level = 200;
			sigma_noise = 15;

			threshold = 0.0001;  // Detection threshold

			// Nominal values for hit generation
			nominal_tau = 20;
			nominal_sigma = 20;
			nominal_offset = 10;
			// Uncertainties on nominal values for use in randomizing hits
			sigma_tau = 2;
			sigma_sigma = 2;
			sigma_offset = 2;

			minTime = 0;
			maxTime = 300;  // nanoseconds
			stepSize = 20;  // 20ns => 50M samples per second
			break;
		case PacketType_CAL:
			// Set maximum logn value for scaling purposes
			peakVal = 32;  // Actually about 28.8372
			noise_level = 140;
			sigma_noise = 10;

			threshold = 0.3;  // Detection threshold

			// Nominal values for hit generation
			nominal_eta = -0.9;
			nominal_sigma = 55;
			nominal_Epeak = 200;
			nominal_norm = 1000;
			// Uncertainties on nominal values for use in randomizing hits
			sigma_eta = 0.03;
			sigma_sigma = 5;
			sigma_Epeak = 10;
			sigma_norm = 15;

			minTime = 0;
			maxTime = 2000;  // nanoseconds
			stepSize = 5;    // 5ns => 200M samples per second
			break;
		default:
			std::cout << "ERROR: Invalid packetType: " << packetType << std::endl;
			return 1;
	}

	// PRNG initialization
	// Seed with a real random value, if available
	std::random_device r;
	std::default_random_engine generator(r());
	generator.seed(SEED);

	std::normal_distribution<double> tau_distribution(nominal_tau, sigma_tau);
	std::normal_distribution<double> sigma_distribution(nominal_sigma, sigma_sigma);
	std::normal_distribution<double> offset_distribution(nominal_offset, sigma_offset);

	std::uniform_int_distribution<int> strawIndex_distribution(0, 23039);

	// For now, just use a random value between 0 and 50000 for the TDC
	std::uniform_int_distribution<int> TDC_distribution(0, 50000);

	std::normal_distribution<double> eta_distribution(nominal_eta, sigma_eta);
	std::normal_distribution<double> Epeak_distribution(nominal_Epeak, sigma_Epeak);
	std::normal_distribution<double> norm_distribution(nominal_norm, sigma_norm);

	std::uniform_int_distribution<int> crystalID_distribution(0, 1860);
	std::uniform_int_distribution<int> apdID_distribution(0, 1);

	std::normal_distribution<double> noise_distribution(noise_level, sigma_noise);

	// timeStampVector holds 1 vector per timestamp
	// Each of these vectors contains all the datablocks for that timestamp
	// Each datablock vector contains the adc_t values for the header packet
	// and associated data packets
	std::vector<std::vector<std::vector<adc_t> > > timeStampVector;

	for (size_t eventNum = 0; eventNum < nevents; eventNum++)
	{
		// Create a vector to hold all Link ID / ROC ID combinations to simulate
		std::vector<std::pair<int, int> > rocLinkVector;
		for (auto curLinkID = 0; curLinkID < number_of_links; curLinkID++)
		{
			for (auto curROCID = 0; curROCID < rocs_per_link; curROCID++)
			{
				std::pair<int, int> curPair(curLinkID, curROCID);
				rocLinkVector.push_back(curPair);
			}
		}
		// // Randomize the order in which the Links and ROCs are received
		// std::shuffle(rocLinkVector.begin(),rocLinkVector.end(),generator);

		// Vector to hold all the DataBlocks for this event
		std::vector<std::vector<adc_t> > curDataBlockVector;

		// Loop over the ROC/link pairs and generate datablocks for each ROC on
		// all the links
		auto targetNumROCs = rocLinkVector.size();
		for (size_t curPairNum = 0; curPairNum < targetNumROCs; curPairNum++)
		{
			size_t linkID = rocLinkVector[curPairNum].first;
			size_t rocID = rocLinkVector[curPairNum].second;

			if (read_cal_digis_from_file && packetType == PacketType_CAL)
			{
				std::vector<calhit> curHitVector;
				// Find all hits for this event coming from the specified Link/ROC
				for (size_t curHitIdx = 0; curHitIdx < calEventVector[eventNum].size(); curHitIdx++)
				{
					if (calEventVector[eventNum][curHitIdx].rocID == static_cast<int>(rocID) &&
						calEventVector[eventNum][curHitIdx].linkID == static_cast<int>(linkID))
					{
						curHitVector.push_back(calEventVector[eventNum][curHitIdx]);
					}
				}

				if (curHitVector.size() == 0)
				{
					// No hits, so just fill a header packet and no data packets
					std::vector<adc_t> curDataBlock;
					// Add the header packet to the DataBlock (leaving including a placeholder for
					// the number of packets in the DataBlock);
					adc_t null_adc = 0;
					// First 16 bits of header (reserved values)
					curDataBlock.push_back(null_adc);
					// Second 16 bits of header (ROC ID, packet type, and link ID):
					std::bitset<16> curROCID = rocID;      // 4 bit ROC ID
					std::bitset<16> headerPacketType = 5;  // 4 bit Data packet header type is 5
					headerPacketType <<= 4;                // Shift left by 4
					std::bitset<16> curLinkID = linkID;    // 3 bit link ID
					curLinkID <<= 8;                       // Shift left by 8
					std::bitset<16> secondEntry = (curROCID | headerPacketType | curLinkID);
					secondEntry[15] = 1;  // valid bit
					curDataBlock.push_back((adc_t)secondEntry.to_ulong());
					// Third 16 bits of header (number of data packets is 0)
					curDataBlock.push_back(null_adc);
					// Fourth through sixth 16 bits of header (timestamp)
					uint64_t timestamp = eventNum + starting_timestamp;
					curDataBlock.push_back(static_cast<adc_t>(timestamp & 0xFFFF));
					curDataBlock.push_back(static_cast<adc_t>((timestamp >> 16) & 0xFFFF));
					curDataBlock.push_back(static_cast<adc_t>((timestamp >> 32) & 0xFFFF));

					// Seventh 16 bits of header (data packet format version and status)
					adc_t status = 0;                // 0 Corresponds to "Timestamp has valid data"
					adc_t formatVersion = (5 << 8);  // Using 5 for now
					curDataBlock.push_back(formatVersion + status);
					// Eighth 16 bits of header (Unassigned)
					curDataBlock.push_back(null_adc);

					// Fill in the byte count field of the header packet
					adc_t numBytes = 16;  // Just the header packet
					curDataBlock[0] = numBytes;

					curDataBlockVector.push_back(curDataBlock);
				}
				else
				{
					for (size_t curHitIdx = 0; curHitIdx < curHitVector.size(); curHitIdx++)
					{
						// Generate a DataBlock for every hit on this ROC

						auto curHit = curHitVector[curHitIdx];

						std::vector<adc_t> curDataBlock;
						// Add the header packet to the DataBlock (leaving including a placeholder for
						// the number of packets in the DataBlock);
						adc_t null_adc = 0;
						// First 16 bits of header (reserved values)
						curDataBlock.push_back(null_adc);
						// Second 16 bits of header (ROC ID, packet type, and link ID):
						std::bitset<16> curROCID = rocID;      // 4 bit ROC ID
						std::bitset<16> headerPacketType = 5;  // 4 bit Data packet header type is 5
						headerPacketType <<= 4;                // Shift left by 4
						std::bitset<16> curLinkID = linkID;    // 3 bit link ID
						curLinkID <<= 8;                       // Shift left by 8
						auto secondEntry = (curROCID | headerPacketType | curLinkID);
						secondEntry[15] = 1;  // valid bit
						curDataBlock.push_back(static_cast<adc_t>(secondEntry.to_ulong()));
						// Third 16 bits of header (number of data packets is 0)
						curDataBlock.push_back(null_adc);
						// Fourth through sixth 16 bits of header (timestamp)
						uint64_t timestamp = eventNum + starting_timestamp;
						curDataBlock.push_back(static_cast<adc_t>(timestamp & 0xFFFF));
						curDataBlock.push_back(static_cast<adc_t>((timestamp >> 16) & 0xFFFF));
						curDataBlock.push_back(static_cast<adc_t>((timestamp >> 32) & 0xFFFF));

						// Seventh 16 bits of header (data packet format version and status)
						adc_t status = 0;                // 0 Corresponds to "Timestamp has valid data"
						adc_t formatVersion = (5 << 8);  // Using 5 for now
						curDataBlock.push_back(formatVersion + status);
						// Eighth 16 bits of header (Unassigned)
						curDataBlock.push_back(null_adc);

						// Create a vector of adc_t values corresponding to
						// the content of CAL data packets.
						std::vector<adc_t> packetVector;

						// Fill the data packets:
						// Assume the 0th apd is always read out before the second
						adc_t crystalID = curHit.crystalId;
						adc_t apdID = curHit.recoDigiId % 2;
						adc_t IDNum = ((apdID << 12) | crystalID);

						packetVector.push_back(IDNum);
						packetVector.push_back((adc_t)(curHit.recoDigiT0));
						packetVector.push_back((adc_t)(curHit.recoDigiSamples));
						for (auto sampleIdx = 0; sampleIdx < curHit.recoDigiSamples; sampleIdx++)
						{
							adc_t scaledVal = static_cast<adc_t>(curHit.waveform[sampleIdx]);
							packetVector.push_back(scaledVal);
						}
						// Pad any empty space in the last packet with 0s
						size_t padding_slots = 8 - ((curHit.recoDigiSamples - 5) % 8);
						if (padding_slots < 8)
						{
							for (size_t i = 0; i < padding_slots; i++)
							{
								packetVector.push_back((adc_t)0);
							}
						}

						// Fill in the number of data packets entry in the header packet
						adc_t numDataPackets = static_cast<adc_t>(packetVector.size() / 8);
						curDataBlock[2] = numDataPackets;

						// Fill in the byte count field of the header packet
						adc_t numBytes = (numDataPackets + 1) * 16;
						curDataBlock[0] = numBytes;

						// Append the data packets after the header packet in the DataBlock
						curDataBlock.insert(curDataBlock.end(), packetVector.begin(), packetVector.end());
						curDataBlockVector.push_back(curDataBlock);
					}
				}
			}
			else if (read_trk_digis_from_file && packetType == PacketType_TRK)
			{
				std::vector<trkhit> curHitVector;
				// Find all hits for this event coming from the specified Link/ROC
				for (size_t curHitIdx = 0; curHitIdx < trkEventVector[eventNum].size(); curHitIdx++)
				{
					if (trkEventVector[eventNum][curHitIdx].rocID == (int)rocID &&
						trkEventVector[eventNum][curHitIdx].linkID == (int)linkID)
					{
						curHitVector.push_back(trkEventVector[eventNum][curHitIdx]);
					}
				}

				if (curHitVector.size() == 0)
				{
					// No hits, so just fill a header packet and no data packets
					std::vector<adc_t> curDataBlock;
					// Add the header packet to the DataBlock (leaving including a placeholder for
					// the number of packets in the DataBlock);
					adc_t null_adc = 0;
					// First 16 bits of header (reserved values)
					curDataBlock.push_back(null_adc);
					// Second 16 bits of header (ROC ID, packet type, and link ID):
					std::bitset<16> curROCID = rocID;      // 4 bit ROC ID
					std::bitset<16> headerPacketType = 5;  // 4 bit Data packet header type is 5
					headerPacketType <<= 4;                // Shift left by 4
					std::bitset<16> curLinkID = linkID;    // 3 bit link ID
					curLinkID <<= 8;                       // Shift left by 8
					std::bitset<16> secondEntry = (curROCID | headerPacketType | curLinkID);
					secondEntry[15] = 1;  // valid bit
					curDataBlock.push_back((adc_t)secondEntry.to_ulong());
					// Third 16 bits of header (number of data packets is 0)
					curDataBlock.push_back(null_adc);
					// Fourth through sixth 16 bits of header (timestamp)
					uint64_t timestamp = eventNum + starting_timestamp;
					curDataBlock.push_back(static_cast<adc_t>(timestamp & 0xFFFF));
					curDataBlock.push_back(static_cast<adc_t>((timestamp >> 16) & 0xFFFF));
					curDataBlock.push_back(static_cast<adc_t>((timestamp >> 32) & 0xFFFF));

					// Seventh 16 bits of header (data packet format version and status)
					adc_t status = 0;                // 0 Corresponds to "Timestamp has valid data"
					adc_t formatVersion = (5 << 8);  // Using 5 for now
					curDataBlock.push_back(formatVersion + status);
					// Eighth 16 bits of header (Unassigned)
					curDataBlock.push_back(null_adc);

					// Fill in the byte count field of the header packet
					adc_t numBytes = 16;  // Just the header packet
					curDataBlock[0] = numBytes;

					curDataBlockVector.push_back(curDataBlock);
				}
				else
				{
					for (size_t curHitIdx = 0; curHitIdx < curHitVector.size(); curHitIdx++)
					{
						// Generate a DataBlock for every hit on this ROC

						trkhit curHit = curHitVector[curHitIdx];

						std::vector<adc_t> curDataBlock;
						// Add the header packet to the DataBlock (leaving including a placeholder for
						// the number of packets in the DataBlock);
						adc_t null_adc = 0;
						// First 16 bits of header (reserved values)
						curDataBlock.push_back(null_adc);
						// Second 16 bits of header (ROC ID, packet type, and link ID):
						std::bitset<16> curROCID = rocID;      // 4 bit ROC ID
						std::bitset<16> headerPacketType = 5;  // 4 bit Data packet header type is 5
						headerPacketType <<= 4;                // Shift left by 4
						std::bitset<16> curLinkID = linkID;    // 3 bit link ID
						curLinkID <<= 8;                       // Shift left by 8
						std::bitset<16> secondEntry = (curROCID | headerPacketType | curLinkID);
						secondEntry[15] = 1;  // valid bit
						curDataBlock.push_back((adc_t)secondEntry.to_ulong());
						// Third 16 bits of header (number of data packets is 0)
						curDataBlock.push_back(null_adc);
						// Fourth through sixth 16 bits of header (timestamp)
						uint64_t timestamp = eventNum + starting_timestamp;
						curDataBlock.push_back(static_cast<adc_t>(timestamp & 0xFFFF));
						curDataBlock.push_back(static_cast<adc_t>((timestamp >> 16) & 0xFFFF));
						curDataBlock.push_back(static_cast<adc_t>((timestamp >> 32) & 0xFFFF));

						// Seventh 16 bits of header (data packet format version and status)
						adc_t status = 0;                // 0 Corresponds to "Timestamp has valid data"
						adc_t formatVersion = (5 << 8);  // Using 5 for now
						curDataBlock.push_back(formatVersion + status);
						// Eighth 16 bits of header (Unassigned)
						curDataBlock.push_back(null_adc);

						// Create a vector of adc_t values corresponding to
						// the content of TRK data packets.
						std::vector<adc_t> packetVector;

						// Fill the data packets:
						// Assume the 0th apd is always read out before the second
						adc_t strawIndex = curHit.strawIdx;

						uint32_t TDC0 = curHit.recoDigiT0;
						uint32_t TDC1 = curHit.recoDigiT1;

						adc_t TDC0_low = TDC0;
						adc_t TDC0_high = TDC0 >> 16;
						adc_t TDC1_low = TDC1 << 8;
						adc_t TDC1_high = TDC1 >> 8;

						adc_t TDC0_high_TDC1_low = TDC0_high | TDC1_low;

						packetVector.push_back(strawIndex);
						packetVector.push_back(TDC0_low);
						packetVector.push_back(TDC0_high_TDC1_low);
						packetVector.push_back(TDC1_high);

						for (int sampleIdx = 0; sampleIdx < curHit.recoDigiSamples; sampleIdx++)
						{
							adc_t scaledVal = static_cast<adc_t>(curHit.waveform[sampleIdx]);
							packetVector.push_back(scaledVal);
						}
						// Pad any empty space in the last packet with 0s
						size_t padding_slots = 8 - ((curHit.recoDigiSamples - 4) % 8);
						if (padding_slots < 8)
						{
							for (size_t i = 0; i < padding_slots; i++)
							{
								packetVector.push_back((adc_t)0);
							}
						}

						// Fill in the number of data packets entry in the header packet

						adc_t numDataPackets = static_cast<adc_t>(packetVector.size() / 8);
						curDataBlock[2] = numDataPackets;

						// Fill in the byte count field of the header packet
						adc_t numBytes = (numDataPackets + 1) * 16;
						curDataBlock[0] = numBytes;

						// Append the data packets after the header packet in the DataBlock
						curDataBlock.insert(curDataBlock.end(), packetVector.begin(), packetVector.end());
						curDataBlockVector.push_back(curDataBlock);
					}
				}
			}
			else
			{
				// Generate packet content based on approximate waveforms instead of more complete simulation
				std::vector<adc_t> curDataBlock;
				// Add the header packet to the DataBlock (leaving including a placeholder for
				// the number of packets in the DataBlock);
				adc_t null_adc = 0;
				// First 16 bits of header (reserved values)
				curDataBlock.push_back(null_adc);
				// Second 16 bits of header (ROC ID, packet type, and link ID):
				std::bitset<16> curROCID = rocID;      // 4 bit ROC ID
				std::bitset<16> headerPacketType = 5;  // 4 bit Data packet header type is 5
				headerPacketType <<= 4;                // Shift left by 4
				std::bitset<16> curLinkID = linkID;    // 3 bit link ID
				curLinkID <<= 8;                       // Shift left by 8
				std::bitset<16> secondEntry = (curROCID | headerPacketType | curLinkID);
				secondEntry[15] = 1;  // valid bit
				curDataBlock.push_back((adc_t)secondEntry.to_ulong());
				// Third 16 bits of header (number of data packets is 0)
				curDataBlock.push_back(null_adc);
				// Fourth through sixth 16 bits of header (timestamp)
				uint64_t timestamp = eventNum + starting_timestamp;
				curDataBlock.push_back(static_cast<adc_t>(timestamp & 0xFFFF));
				curDataBlock.push_back(static_cast<adc_t>((timestamp >> 16) & 0xFFFF));
				curDataBlock.push_back(static_cast<adc_t>((timestamp >> 32) & 0xFFFF));

				// Seventh 16 bits of header (data packet format version and status)
				adc_t status = 0;                // 0 Corresponds to "Timestamp has valid data"
				adc_t formatVersion = (5 << 8);  // Using 5 for now
				curDataBlock.push_back(formatVersion + status);
				// Eighth 16 bits of header (Unassigned)
				curDataBlock.push_back(null_adc);

				// Vector to hold raw digitized waveform
				std::vector<double> digiVector;
				double hitTime = 0.0;

				// Create a vector of adc_t values corresponding to
				// the content of TRK/CAL data packets.
				std::vector<adc_t> packetVector;

				// Sample distributions
				double cur_tau = tau_distribution(generator);
				double cur_sigma = sigma_distribution(generator);
				double cur_offset = offset_distribution(generator);

				double cur_eta = eta_distribution(generator);
				double cur_Epeak = Epeak_distribution(generator);
				double cur_norm = norm_distribution(generator);

				// Control flags
				bool inWindow = false;
				bool exitWindow = false;
				double curVal = 0;
				size_t padding_slots;

				switch (packetType)
				{
					case PacketType_TRK: {
						// Generate TRK data packets

						for (double curTime = minTime; curTime < maxTime && !exitWindow; curTime += stepSize)
						{
							curVal = f(curTime, cur_tau, cur_sigma, cur_offset);
							if (!inWindow && curVal > threshold)
							{
								inWindow = true;
								curTime -= 2 * stepSize;  // Save samples starting 2 steps before the threshold is reached
								hitTime = curTime;
								curVal = f(curTime, cur_tau, cur_sigma, cur_offset);
							}
							else if (curVal < threshold && inWindow && curTime > hitTime + 2 * stepSize)
							{
								inWindow = false;
								exitWindow = true;
							}

							if (inWindow)
							{
								digiVector.push_back(curVal);
							}
						}

						adc_t strawIndex = strawIndex_distribution(generator);
						uint32_t TDC0 = TDC_distribution(generator);
						uint32_t TDC1 = TDC_distribution(generator);

						adc_t TDC0_low = TDC0;
						adc_t TDC0_high = TDC0 >> 16;
						adc_t TDC1_low = TDC1 << 8;
						adc_t TDC1_high = TDC1 >> 8;

						adc_t TDC0_high_TDC1_low = TDC0_high | TDC1_low;

						packetVector.push_back(strawIndex);
						packetVector.push_back(TDC0_low);
						packetVector.push_back(TDC0_high_TDC1_low);
						packetVector.push_back(TDC1_high);

						for (size_t i = 0; i < numADCSamples; i++)
						{
							adc_t scaledVal = 0;
							// Scale the function value relative to a peak
							// value of around 0.02 and convert to a 12 bit integer
							// stored in a 16 bit adc_t
							if (i < digiVector.size())
							{
								scaledVal = static_cast<adc_t>(digiVector[i] / (1.0 * peakVal) * (1 << 12));
							}
							// Add some noise
							adc_t noise = static_cast<adc_t>(noise_distribution(generator));
							packetVector.push_back(scaledVal + noise);
						}
						// Pad any empty space in the last packet with 0s
						padding_slots = 8 - ((numADCSamples - 4) % 8);
						if (padding_slots < 8)
						{
							for (size_t i = 0; i < padding_slots; i++)
							{
								packetVector.push_back((adc_t)0);
							}
						}
					}
					break;
					case PacketType_CAL: {
						{
							// Approximate CAL waveforms on the fly instead of reading them in from a file
							for (double curTime = minTime; curTime < maxTime && !exitWindow; curTime += stepSize)
							{
								curVal = logn(curTime, cur_eta, cur_sigma, cur_Epeak, cur_norm);
								if (!inWindow && curVal > threshold)
								{
									inWindow = true;
									curTime -= 5 * stepSize;  // Save samples starting 5 steps before the threshold is reached
									hitTime = curTime;
									curVal = logn(curTime, cur_eta, cur_sigma, cur_Epeak, cur_norm);
								}
								else if (curVal < threshold && inWindow && curTime > hitTime + 5 * stepSize)
								{
									inWindow = false;
									exitWindow = true;
								}

								if (inWindow)
								{
									digiVector.push_back(curVal);
								}
							}

							adc_t apdID = apdID_distribution(generator);
							adc_t crystalID = crystalID_distribution(generator);
							adc_t IDNum = ((apdID << 12) | crystalID);
							packetVector.push_back(IDNum);
							packetVector.push_back((adc_t)hitTime);
							packetVector.push_back((adc_t)digiVector.size());
							for (size_t i = 0; i < digiVector.size(); i++)
							{
								// Scale the function value relative to a peak possible
								// value of around 35 and convert to a 12 bit integer
								// stored in a 16 bit adc_t
								adc_t scaledVal = static_cast<adc_t>(digiVector[i] / (1.0 * peakVal) * (1 << 12));
								// Add some noise
								adc_t noise = static_cast<adc_t>(noise_distribution(generator));
								packetVector.push_back(scaledVal + noise);
							}
							// Pad any empty space in the last packet with 0s
							padding_slots = 8 - ((digiVector.size() - 5) % 8);
							if (padding_slots < 8)
							{
								for (size_t i = 0; i < padding_slots; i++)
								{
									packetVector.push_back((adc_t)0);
								}
							}
						}
					}
					break;
					default:
						break;
				}

				// Fill in the number of data packets entry in the header packet
				auto numDataPackets = static_cast<adc_t>(packetVector.size() / 8);
				curDataBlock[2] = numDataPackets;

				// Fill in the byte count field of the header packet
				adc_t numBytes = (numDataPackets + 1) * 16;
				curDataBlock[0] = numBytes;

				// Append the data packets after the header packet in the DataBlock
				curDataBlock.insert(curDataBlock.end(), packetVector.begin(), packetVector.end());
				curDataBlockVector.push_back(curDataBlock);
			}

		}  // Done generating DataBlocks for this timestamp
		timeStampVector.push_back(curDataBlockVector);

	}  // Done generating data for all timestamps

	std::cout << "Size of timestamp vector: " << timeStampVector.size() << std::endl;

	if (verbose)
	{
		std::cout << "\tNumber of DataBlocks for each timestamp vector: {";
		for (size_t i = 0; i < timeStampVector.size(); i++)
		{
			std::cout << timeStampVector[i].size();
			if (i < timeStampVector.size() - 1)
			{
				std::cout << ", ";
			}
		}
		std::cout << "}" << std::endl;

		std::cout << "\tNumber of packets in each data block: {";
		for (size_t i = 0; i < timeStampVector.size(); i++)
		{
			std::cout << "{";
			for (size_t j = 0; j < timeStampVector[i].size(); j++)
			{
				std::cout << (timeStampVector[i][j].size()) / 8;
				if (j < timeStampVector[i].size() - 1)
				{
					std::cout << ", ";
				}
			}
			std::cout << "}";
			if (i < timeStampVector.size() - 1)
			{
				std::cout << ",";
			}
		}
		std::cout << "}" << std::endl;
	}

	std::vector<adc_t> masterVector;
	for (size_t i = 0; i < timeStampVector.size(); i++)
	{
		// Determine how to divide DataBlocks between DMABlocks within each timestamp
		std::vector<size_t> dataBlockPartition;
		std::vector<size_t> dataBlockPartitionSizes;
		for (size_t j = 0, curDMABlockSize = 0, numDataBlocksInCurDMABlock = 0; j < timeStampVector[i].size(); j++)
		{
			numDataBlocksInCurDMABlock++;                         // Increment number of DataBlocks in the current DMA block
			curDMABlockSize += timeStampVector[i][j].size() * 2;  // Size of current data block in 8bit words
			assert(curDMABlockSize <= max_DMA_block_size - 8);
			if (j == timeStampVector[i].size() - 1)
			{
				dataBlockPartition.push_back(numDataBlocksInCurDMABlock);
				dataBlockPartitionSizes.push_back(curDMABlockSize + 8);
			}
			else if (curDMABlockSize + (2 * timeStampVector[i][j + 1].size()) > max_DMA_block_size - 8)
			{
				dataBlockPartition.push_back(numDataBlocksInCurDMABlock);
				dataBlockPartitionSizes.push_back(curDMABlockSize + 8);
				curDMABlockSize = 0;
				numDataBlocksInCurDMABlock = 0;
			}
		}

		// Break the DataBlocks into DMABlocks and add DMABlock headers
		for (size_t curDMABlockNum = 0, curDataBlockNum = 0; curDMABlockNum < dataBlockPartition.size(); curDMABlockNum++)
		{
			size_t numDataBlocksInCurDMABlock = dataBlockPartition[curDMABlockNum];
			size_t curDMABlockSize = dataBlockPartitionSizes[curDMABlockNum];
			std::vector<adc_t> header = generateDMABlockHeader(curDMABlockSize);
			for (size_t adcNum = 0; adcNum < header.size(); adcNum++)
			{
				masterVector.push_back(header[adcNum]);
			}

			for (size_t j = 0; j < numDataBlocksInCurDMABlock; j++)
			{
				std::vector<adc_t> curDataBlock = timeStampVector[i][curDataBlockNum];
				for (size_t adcNum = 0; adcNum < curDataBlock.size(); adcNum++)
				{
					masterVector.push_back(curDataBlock[adcNum]);
				}
				curDataBlockNum++;
			}
		}

		if (verbose)
		{
			// Print out number of DataBlocks in each DMABlock
			std::cout << "\tTimestamp " << i + starting_timestamp << " DataBlock partition: {";
			for (size_t k = 0; k < dataBlockPartition.size(); k++)
			{
				std::cout << dataBlockPartition[k];
				if (k < dataBlockPartition.size() - 1)
				{
					std::cout << ", ";
				}
			}
			std::cout << "}" << std::endl;
		}
	}  // Close loop over timestamps

	std::cout << std::endl
			  << "Length of final adc_t array: " << masterVector.size() << std::endl;

	// Print contents of final adc_t array:
	if (veryverbose)
	{
		std::cout << "Contents of final adc_t array: " << std::endl;
		for (size_t i = 0; i < masterVector.size(); i++)
		{
			std::bitset<16> curEntry = masterVector[i];
			std::cout << "\t" << curEntry.to_string() << " " << curEntry.to_ulong() << std::endl;
			if (i > 0 && (i + 1) % 4 == 0)
			{
				std::cout << std::endl;
			}
		}
	}

	// Save contents of final adc_t array to binary file
	std::cout << "Writing generated data to file " << outputFile << std::endl;
	if (save_adc_values)
	{
		for (size_t i = 0; i < masterVector.size(); i++)
		{
			binFile.write(reinterpret_cast<const char*>(&(masterVector[i])), sizeof(adc_t));
		}
		binFile.close();
	}

	return 0;
}

double f(double t, double tau, double sigma, double offset)
{
	double E = 2.718281828459045;
	double retval = (pow(E, (pow(sigma, 2) + 2 * offset * tau - 2 * t * tau) / (2. * pow(tau, 2))) *
					 (-pow(sigma, 2) + (-offset + t) * tau)) /
					pow(tau, 3);
	if (retval < 0)
	{
		retval = 0;
	}
	return retval;
}

double logn(double x, double eta, double sigma, double Epeak, double norm)
{
	double Aterm;
	double logterms0, s0;
	double logn, logterm;
	double expterm;
	double pigreco = 3.14159265;

	// double f = 2.35;
	double f = 20.0;

	logterms0 = eta * f / 2 + sqrt(1 + pow((eta * f / 2), 2));
	s0 = (2 / f) * log(logterms0);

	Aterm = eta / (sqrt(2 * pigreco) * sigma * s0);

	logterm = 1 - (eta / sigma) * (x - Epeak);

	if (logterm < 0)
	{
		logterm = 0.0001;
	}
	expterm = log(logterm) / s0;
	expterm = -0.5 * pow(expterm, 2);

	logn = norm * Aterm * exp(expterm);
	return logn;
}

std::vector<adc_t> generateDMABlockHeader(size_t theCount)
{
	std::bitset<64> byteCount = theCount;
	std::bitset<16> byteCount0 = 0;
	std::bitset<16> byteCount1 = 0;
	std::bitset<16> byteCount2 = 0;
	std::bitset<16> byteCount3 = 0;
	for (int i = 0; i < 16; i++)
	{
		byteCount0[i] = byteCount[i + 16 * 0];
		byteCount1[i] = byteCount[i + 16 * 1];
		byteCount2[i] = byteCount[i + 16 * 2];
		byteCount3[i] = byteCount[i + 16 * 3];
	}
	std::vector<adc_t> header;
	header.push_back((adc_t)byteCount0.to_ulong());
	header.push_back((adc_t)byteCount1.to_ulong());
	header.push_back((adc_t)byteCount2.to_ulong());
	header.push_back((adc_t)byteCount3.to_ulong());

	return header;
}
