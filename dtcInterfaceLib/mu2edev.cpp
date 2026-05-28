// This file (mu2edev.cc) was created by Ron Rechenmacher <ron@fnal.gov> on
// Feb 13, 2014. "TERMS AND CONDITIONS" governing this file are in the README
// or COPYING file. If you do not have such a file, one can be obtained by
// contacting Ron or Fermi Lab in Batavia IL, 60510, phone: 630-840-3000.
// $RCSfile: .emacs.gnu,v $
// rev="$Revision: 1.23 $$Date: 2012/01/23 15:32:40 $";

/*
 *    make mu2edev.o CFLAGS='-g -Wall -std=c++0x'
 */

#include <signal.h>
#include <chrono>
#include <errno.h>

#include "TRACE/tracemf.h"

#include "mu2edev.h"

#include "dtcInterfaceLib/otsStyleCoutMacros.h"

typedef unsigned long dma_addr_t;
#include "mu2e_driver/mu2e_proto_globals.h"  // MU2E_NUM_RECV_BUFFS

#define DEV_TLOG(lvl) TLOG(lvl) << "DEVICE " << this->getDeviceUID() << ": "

static constexpr int TLVL_INIT_DMA_ENGINE = TLVL_DEBUG + 15;
static constexpr int TLVL_READ_DATA = TLVL_DEBUG + 16;
static constexpr int TLVL_READ_DATA_2 = TLVL_DEBUG + 17;
static constexpr int TLVL_READ_DATA_3 = TLVL_DEBUG + 18;
static constexpr int TLVL_READ_RELEASE = TLVL_DEBUG + 19;
static constexpr int TLVL_READ_REGISTER = TLVL_DEBUG + 20;
static constexpr int TLVL_WRITE_REGISTER = TLVL_DEBUG + 21;
static constexpr int TLVL_WRITE_REGISTER_CHECKED = TLVL_DEBUG + 22;
static constexpr int TLVL_META_DUMP = TLVL_DEBUG + 23;
static constexpr int TLVL_WRITE_DATA = TLVL_DEBUG + 24;
static constexpr int TLVL_RELEASE_ALL = TLVL_DEBUG + 25;
static constexpr int TLVL_BEGIN_DCS_TRANSACTION = TLVL_DEBUG + 26;
static constexpr int TLVL_BEGIN_DCS_TRANSACTION_VERBOSE = TLVL_DEBUG + 27;
static constexpr int TLVL_END_DCS_TRANSACTION = TLVL_DEBUG + 28;
static constexpr int TLVL_GET_DRIVER_VERSION = TLVL_DEBUG + 29;

static const std::thread::id NULL_TID = std::thread::id();

std::array<mu2edev::DCSLock, MU2E_MAX_NUM_DTCS> mu2edev::dcs_locks_{};

mu2edev::mu2edev()
	: devfd_(0), buffers_held_(0), simulator_(nullptr), activeDeviceIndex_(0), deviceTime_(0LL), writeSize_(0), readSize_(0), UID_("")
{
	// TRACE_CNTL( "lvlmskM", 0x3 );
	// TRACE_CNTL( "lvlmskS", 0x3 );
	TLOG(TLVL_INFO) << "CONSTRUCTOR";
}  // end constructor

mu2edev::~mu2edev()
{
	TLOG(TLVL_INFO) << "DESTRUCTOR " << UID_;

	end_dcs_transaction(false /* mustHaveLock */);
	if (debugFp_) fclose(debugFp_);

	close();
	TLOG(TLVL_INFO) << "DESTRUCTOR end";
}  // end destructor

int mu2edev::init(DTCLib::DTC_SimMode simMode, int deviceIndex, std::string simMemoryFileName, const std::string& uid)
{
	UID_ = uid;

	auto debugWriteFilePath = getenv("DTCLIB_DEBUG_WRITE_FILE_PATH");
	if (debugWriteFilePath != nullptr)
	{
		auto debugWriteFileStr = std::string(debugWriteFilePath) + "/Write_debug_" + uid + "_" + std::to_string(time(0)) + ".txt";
		if (debugFp_) fclose(debugFp_);
		debugFp_ = fopen(debugWriteFileStr.c_str(), "w");
		if (!debugFp_)
		{
			__SS__ << "mu2e Device write debug file could not be opened at path DTCLIB_DEBUG_WRITE_FILE_PATH! Exiting.\n"
				   << "open " << debugWriteFileStr << __E__;
			perror(ss.str().c_str());
			__SS_THROW__;
			// exit(1);
		}
	}

	auto start = std::chrono::steady_clock::now();
	lastWriteTime_ = start;  // init time

	if (simMode != DTCLib::DTC_SimMode_Disabled && simMode != DTCLib::DTC_SimMode_NoCFO &&
		simMode != DTCLib::DTC_SimMode_ROCEmulator && simMode != DTCLib::DTC_SimMode_Loopback)
	{
		simulator_ = new mu2esim(simMemoryFileName);
		simulator_->init(simMode);
	}
	else
	{
		if (simulator_ != nullptr)
		{
			delete simulator_;
			simulator_ = nullptr;
		}

		activeDeviceIndex_ = deviceIndex;
		initDMAEngine();
	}

	// init to free dcs locks
	end_dcs_transaction(true /* force */);  // force unlock

	deviceTime_ += std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count();
	return simMode;
}

//==============================================================================
// exec
//	run linux command and get result back in string
std::string exec(const char* cmd)
{
	__COUTV__(cmd);

	std::array<char, 128> buffer;
	std::string result;
	std::shared_ptr<FILE> pipe(popen(cmd, "r"), pclose);
	if (!pipe)
	{
		__COUT_ERR__ << "popen() failed!" << __E__;
		return "popen() failed!";
	}
	while (!feof(pipe.get()))
	{
		if (fgets(buffer.data(), 128, pipe.get()) != nullptr)
			result += buffer.data();
	}
	//__COUTV__(result);
	return result;
}  // end exec()

/*****************************
   initDMAEngine
   set up DMA engines
   */
void mu2edev::initDMAEngine()
{
	if (simulator_ != nullptr)
	{
		__COUT__ << "Using simulator, so no need to init DMA Engine..." << __E__;
		return;  // do nothing if using simulator
	}
	__COUT__ << "Initializing DMA engine..." << __E__;

	char devfile[11];
	snprintf(devfile, 11, "/dev/" MU2E_DEV_FILE, activeDeviceIndex_);
	int sts;
	if (devfd_) ::close(devfd_);
	devfd_ = open(devfile, O_RDWR);
	if (devfd_ == -1 || devfd_ == 0)
	{
		__SS__ << "mu2e Device file not found (or DTCLIB_SIM_ENABLE not set)! Exiting.\n"
			   << "Attempt to open '" << devfile << "' and received error: " << errno << " - " << strerror(errno) << __E__;

		ss << "Who owns it? (ls -l /dev/mu2e*)\n"
		   << exec("ls -l /dev/mu2e*") << __E__;
		ss << "\nWhat process is using it (lsof /dev/mu2e* 2>/dev/null, will only show processes owned by the current user)?\n"
		   << exec("lsof /dev/mu2e* 2>/dev/null") << __E__;
		perror(ss.str().c_str());
		__SS_THROW__;
		// exit(1);
	}
	for (unsigned chn = 0; chn < MU2E_MAX_CHANNELS; ++chn)
		for (unsigned dir = 0; dir < 2; ++dir)
		{
			m_ioc_get_info_t get_info;
			get_info.chn = chn;
			get_info.dir = dir;
			get_info.tmo_ms = 1000;
			TRACE(TLVL_INIT_DMA_ENGINE, UID_ + " - mu2edev::init before ioctl( devfd_, M_IOC_GET_INFO, &get_info ) chn=%u dir=%u", chn, dir);
			sts = ioctl(devfd_, M_IOC_GET_INFO, &get_info);
			if (sts != 0)
			{
				__SS__ << "Failed mu2edev::init before ioctl( devfd_, M_IOC_GET_INFO, &get_info)." << __E__;
				perror(ss.str().c_str());
				__SS_THROW__;
				// exit(1);
			}
			mu2e_channel_info_[activeDeviceIndex_][chn][dir] = get_info;
			TRACE(TLVL_INIT_DMA_ENGINE, UID_ + " - mu2edev::init %d %u:%u - num=%u size=%u hwIdx=%u, swIdx=%u delta=%u", activeDeviceIndex_, chn, dir,
				  get_info.num_buffs, get_info.buff_size, get_info.hwIdx, get_info.swIdx,
				  mu2e_chn_info_delta_(activeDeviceIndex_, chn, dir, &mu2e_channel_info_));
			for (unsigned map = 0; map < 2; ++map)
			{
				size_t length = get_info.num_buffs * ((map == MU2E_MAP_BUFF) ? get_info.buff_size : sizeof(int));
				// int prot = (((dir == S2C) && (map == MU2E_MAP_BUFF))? PROT_WRITE : PROT_READ);
				int prot = (((map == MU2E_MAP_BUFF)) ? PROT_WRITE : PROT_READ);
				off64_t offset = chnDirMap2offset(chn, dir, map);
				mu2e_mmap_ptrs_[activeDeviceIndex_][chn][dir][map] = mmap(0 /* hint address */
																		  ,
																		  length, prot, MAP_SHARED, devfd_, offset);
				if (mu2e_mmap_ptrs_[activeDeviceIndex_][chn][dir][map] == MAP_FAILED)
				{
					__SS__ << "mu2e Device mmap error! Exiting." << __E__;
					perror(ss.str().c_str());
					__SS_THROW__;
					// exit(1);
				}
				TRACE(TLVL_INIT_DMA_ENGINE, UID_ + " - mu2edev::init chnDirMap2offset=%lu mu2e_mmap_ptrs_[%d][%d][%d][%d]=%p p=%c l=%lu", offset, activeDeviceIndex_, chn,
					  dir, map, mu2e_mmap_ptrs_[activeDeviceIndex_][chn][dir][map], prot == PROT_READ ? 'R' : 'W', length);
			}

			// do not want to release all from constructor, in case this instance of the device is not the 'owner' of a DMA-channel
			//  if (dir == DTC_DMA_Direction_C2S)
			//  {
			//  	release_all(static_cast<DTC_DMA_Engine>(chn));
			//  }

			// Reset the DTC
			//{
			//	write_register(0x9100, 0, 0xa0000000);
			//	write_register(0x9118, 0, 0x0000003f);
			//	write_register(0x9100, 0, 0x00000000);
			//	write_register(0x9100, 0, 0x10000000);
			//	write_register(0x9100, 0, 0x30000000);
			//	write_register(0x9100, 0, 0x10000000);
			//	write_register(0x9118, 0, 0x00000000);
			//}

			// Enable DMA Engines
			{
				// uint16_t addr = DTC_Register_Engine_Control(chn, dir);
				// TRACE(TLVL_INIT_DMA_ENGINE, UID_ + " - mu2edev::init write Engine_Control reg 0x%x", addr);
				// write_register(addr, 0, 0x100);//bit 8 enable=1
			}
		}
}  // end initDMAEngine()

/*****************************
   read_data
   returns number of bytes read; negative value indicates an error
   */
int mu2edev::read_data(DTC_DMA_Engine const& chn, void** buffer, int tmo_ms)
{
	// WARNING NOTE: if there is existing data still sitting in unreleased buffer, the timeout will not add any delay
	int retsts;
	TRACE_EXIT { TRACE(TLVL_READ_DATA, UID_ + " - mu2edev::read_data returning retsts(bytes)=%d", retsts); };

	if (chn == DTC_DMA_Engine_DCS && !thread_owns_dcs_lock())
	{
		TRACE(TLVL_ERROR, UID_ + " - read_data dcs lock not held!");
		return retsts = -2;
	}

	auto start = std::chrono::steady_clock::now();
	if (simulator_ != nullptr)
	{
		retsts = simulator_->read_data(chn, buffer, tmo_ms);
	}
	else
	{
		retsts = 0;
		unsigned has_recv_data;
		TRACE(TLVL_READ_DATA, UID_ + " - mu2edev::read_data before (mu2e_mmap_ptrs_[%d][0][0][0]!=NULL) || ((retsts=init())==0) tmo_ms=%d", activeDeviceIndex_, tmo_ms);
		if ((mu2e_mmap_ptrs_[activeDeviceIndex_][0][0][0] != NULL) ||
			((retsts = init(DTCLib::DTC_SimMode_Disabled, 0)) == 0))  // Default-init mu2edev if not given guidance
		{
			has_recv_data = mu2e_chn_info_delta_(activeDeviceIndex_, chn, C2S, &mu2e_channel_info_);
			TRACE(TLVL_READ_DATA, UID_ + " - mu2edev::read_data after %u=has_recv_data = delta_( chn=%d, C2S ), held=%u", has_recv_data, chn, buffers_held_);
			mu2e_channel_info_[activeDeviceIndex_][chn][C2S].tmo_ms = tmo_ms;  // in case GET_INFO is called
			if ((has_recv_data > buffers_held_) ||
				((retsts = ioctl(devfd_, M_IOC_GET_INFO, &mu2e_channel_info_[activeDeviceIndex_][chn][C2S])) == 0 &&
				 (has_recv_data = mu2e_chn_info_delta_(activeDeviceIndex_, chn, C2S, &mu2e_channel_info_)) >
					 buffers_held_))
			{  // have data
				// get byte count from new/next
				unsigned newNxtIdx =
					idx_add(mu2e_channel_info_[activeDeviceIndex_][chn][C2S].swIdx, (int)buffers_held_ + 1, activeDeviceIndex_, chn, C2S);
				int* BC_p = (int*)mu2e_mmap_ptrs_[activeDeviceIndex_][chn][C2S][MU2E_MAP_META];
				retsts = BC_p[newNxtIdx];
				*buffer = ((mu2e_databuff_t*)(mu2e_mmap_ptrs_[activeDeviceIndex_][chn][C2S][MU2E_MAP_BUFF]))[newNxtIdx];
				TRACE(TLVL_READ_DATA_2,
					  "mu2edev::read_data chn%d hIdx=%u, sIdx=%u num_buffs=%u hasRcvDat=%u %p(BC_p)[newNxtIdx=%d]=retsts=%d %p(buf)[0]=0x%08x",
					  chn,
					  mu2e_channel_info_[activeDeviceIndex_][chn][C2S].hwIdx,
					  mu2e_channel_info_[activeDeviceIndex_][chn][C2S].swIdx,
					  mu2e_channel_info_[activeDeviceIndex_][chn][C2S].num_buffs,
					  has_recv_data, (void*)BC_p, newNxtIdx, retsts,
					  *buffer, *(uint32_t*)*buffer);
				TRACE(TLVL_READ_DATA_3, "first 80 bytes: %016lx %016lx %016lx %016lx %016lx %016lx %016lx %016lx %016lx %016lx",
					  *(((uint64_t*)*buffer) + 0), *(((uint64_t*)*buffer) + 1), *(((uint64_t*)*buffer) + 2), *(((uint64_t*)*buffer) + 3),
					  *(((uint64_t*)*buffer) + 4), *(((uint64_t*)*buffer) + 5), *(((uint64_t*)*buffer) + 6), *(((uint64_t*)*buffer) + 7),
					  *(((uint64_t*)*buffer) + 8), *(((uint64_t*)*buffer) + 9));

				/// increment buffers held if allowing multiple buffers to be held by user space (e.g. for Data DMA channel, which is different for CFO vs DTC)</param>
				if (chn == DTC_DMA_Engine_DAQ)
					++buffers_held_;
			}
			else
			{  // was it a tmo or error
				if (retsts != 0)
				{
					__SS__ << "Failed mu2edev::read_data with M_IOC_GET_INFO... return not 0." << __E__;
					perror(ss.str().c_str());
					__SS_THROW__;
					// exit(1);
				}
				TRACE(TLVL_READ_DATA_2, UID_ + " - mu2edev::read_data not error... return %d status", retsts);
			}
		}
	}
	deviceTime_ += std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count();
	if (retsts > 0) readSize_ += retsts;
	return retsts;
}  // end read_data()

int mu2edev::GetBufferIndex(DTC_DMA_Engine const& chn, const void* ptr) const
{
	const volatile void* mmapBase = mu2e_mmap_ptrs_[activeDeviceIndex_][chn][C2S][MU2E_MAP_BUFF];
	if (mmapBase == nullptr || ptr == nullptr) return -1;
	const auto bytesFromBase = reinterpret_cast<const volatile uint8_t*>(ptr) - reinterpret_cast<const volatile uint8_t*>(mmapBase);
	if (bytesFromBase < 0) return -1;
	const auto idx = static_cast<size_t>(bytesFromBase) / sizeof(mu2e_databuff_t);
	if (idx >= MU2E_NUM_RECV_BUFFS) return -1;
	return static_cast<int>(idx);
}

/* read_release
   release a number of buffers (usually 1)
   */
int mu2edev::read_release(DTC_DMA_Engine const& chn, unsigned num)
{
	if (chn == DTC_DMA_Engine_DCS && !thread_owns_dcs_lock())
	{
		TRACE(TLVL_ERROR, UID_ + " - read_release dcs lock not held!");
		return -2;
	}

	auto start = std::chrono::steady_clock::now();
	auto retsts = -1;
	if (simulator_ != nullptr)
	{
		retsts = simulator_->read_release(chn, num);
	}
	else
	{
		retsts = 0;
		unsigned long arg;
		unsigned has_recv_data;
		has_recv_data = mu2e_chn_info_delta_(activeDeviceIndex_, chn, C2S, &mu2e_channel_info_);

		TLOG(TLVL_READ_RELEASE) << "read_release(chn=" << chn << " num=" << num << ") dtc="
								<< activeDeviceIndex_ << " has_recv_data=" << has_recv_data;

		// if (num > has_recv_data) num = has_recv_data;
		if (num <= has_recv_data)
		{
			arg = (chn << 24) | (C2S << 16) | (num & 0xffff);  // THIS OBIVOUSLY SHOULD BE A MACRO
			retsts = ioctl(devfd_, M_IOC_BUF_GIVE, arg);
			if (retsts != 0)
			{
				__SS__ << "ioctl(devfd_, M_IOC_BUF_GIVE, arg)... return not 0." << __E__;
				perror(ss.str().c_str());
				DEV_TLOG(TLVL_ERROR) << ss.str();
				// __SS_THROW__;
			}  // exit(1); } // Don't exit for now

			// increment our cached info
			mu2e_channel_info_[activeDeviceIndex_][chn][C2S].swIdx =
				idx_add(mu2e_channel_info_[activeDeviceIndex_][chn][C2S].swIdx, (int)num, activeDeviceIndex_, chn, C2S);
			if (num <= buffers_held_)
				buffers_held_ -= num;
			else
				buffers_held_ = 0;
		}
		else
			TLOG(TLVL_WARN) << "read_release num=" << num << " > has_recv_data=" << has_recv_data;
	}
	deviceTime_ += std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count();
	return retsts;
}  // end read_release()

int mu2edev::read_register(uint16_t address, int tmo_ms, uint32_t* output)
{
	auto start = std::chrono::steady_clock::now();
	if (simulator_ != nullptr)
	{
		return simulator_->read_register(address, tmo_ms, output);
	}
	m_ioc_reg_access_t reg;
	reg.reg_offset = address;
	reg.access_type = 0;

	int counter = 0;
	int errorCode = -99;

	while (counter < 5 && errorCode < 0)
	{
		errorCode = ioctl(devfd_, M_IOC_REG_ACCESS, &reg);
		counter++;
		if (errorCode < 0) usleep(10000);
	}
	*output = reg.val;
	TRACE(TLVL_READ_REGISTER, UID_ + " - Read value 0x%x from register 0x%x errorcode %d", reg.val, address, errorCode);
	deviceTime_ += std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count();
	return errorCode;
}  // end read_register()

int mu2edev::write_register(uint16_t address, int tmo_ms, uint32_t data)
{
	auto start = std::chrono::steady_clock::now();
	auto retsts = -1;
	if (simulator_ != nullptr)
	{
		retsts = simulator_->write_register(address, tmo_ms, data);
	}
	else
	{
		m_ioc_reg_access_t reg;
		reg.reg_offset = address;
		reg.access_type = 1;
		reg.val = data;
		TRACE(TLVL_WRITE_REGISTER, UID_ + " - Writing value 0x%x to register 0x%x", data, address);
		if (debugFp_) fprintf(debugFp_, (UID_ + " - Writing value 0x%x to register 0x%x - time delta %ld\n").c_str(), data, address,
							  std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - lastWriteTime_).count());
		retsts = ioctl(devfd_, M_IOC_REG_ACCESS, &reg);
		lastWriteTime_ = start;
	}
	deviceTime_ += std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count();
	return retsts;
}  // end write_register()

int mu2edev::write_register_checked(uint16_t address, int tmo_ms, uint32_t data, uint32_t* output)
{
	auto start = std::chrono::steady_clock::now();
	auto retsts = -1;
	if (simulator_ != nullptr)
	{
		retsts = simulator_->write_register(address, tmo_ms, data);
		*output = data;
	}
	else
	{
		m_ioc_reg_access_t reg;
		reg.reg_offset = address;
		reg.access_type = 2;
		reg.val = data;
		if (debugFp_) fprintf(debugFp_, (UID_ + " - Writing value 0x%x to register 0x%x with readback - time delta %ld\n").c_str(), data, address,
							  std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - lastWriteTime_).count());
		retsts = ioctl(devfd_, M_IOC_REG_ACCESS, &reg);
		*output = reg.val;
		lastWriteTime_ = start;

		TRACE(TLVL_WRITE_REGISTER_CHECKED, UID_ + " - Writing value 0x%x to register 0x%x with readback=0x%x", data, address, *output);
	}
	deviceTime_ += std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count();
	return retsts;
}  // end write_register_checked()

void mu2edev::meta_dump()
{
	TRACE(TLVL_META_DUMP, UID_ + " - mu2edev::meta_dump");
	auto start = std::chrono::steady_clock::now();
	if (simulator_ == nullptr)
	{
		int retsts = 0;
		for (int chn = 0; chn < MU2E_MAX_CHANNELS; ++chn)
			for (int dir = 0; dir < 2; ++dir)
				if ((mu2e_mmap_ptrs_[activeDeviceIndex_][0][0][0] != NULL) ||
					((retsts = init(DTCLib::DTC_SimMode_Disabled, 0)) == 0))  // Default-init mu2edev if not given guidance
				{
					for (unsigned buf = 0; buf < mu2e_channel_info_[activeDeviceIndex_][chn][dir].num_buffs; ++buf)
					{
						int* BC_p = (int*)mu2e_mmap_ptrs_[activeDeviceIndex_][chn][dir][MU2E_MAP_META];
						printf("buf_%02d: %u\n", buf, BC_p[buf]);
					}
				}
		retsts = ioctl(devfd_, M_IOC_DUMP);
	}
	deviceTime_ += std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count();
}

int mu2edev::write_data(DTC_DMA_Engine const& chn, void* buffer, size_t bytes)
{
	if (chn == DTC_DMA_Engine_DCS && !thread_owns_dcs_lock())
	{
		__SS__ << "write_data failed - dcs lock not held!" << __E__;
		__SS_THROW__;
		// return -2;
	}

	auto start = std::chrono::steady_clock::now();
	auto retsts = -1;
	if (simulator_ != nullptr)
	{
		retsts = simulator_->write_data(chn, buffer, bytes);
	}
	else
	{
		int dir = S2C;
		retsts = 0;
		unsigned delta = mu2e_chn_info_delta_(activeDeviceIndex_, chn, dir, &mu2e_channel_info_);  // check cached info
		TRACE(TLVL_WRITE_DATA, UID_ + " - write_data delta=%u chn=%d dir=S2C, sz=%zu", delta, chn, bytes);
		while (delta <= 1 &&
			   std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count() <
				   1000)
		{
			m_ioc_get_info_t get_info;
			get_info.chn = chn;
			get_info.dir = dir;
			get_info.tmo_ms = 1000;
			int sts = ioctl(devfd_, M_IOC_GET_INFO, &get_info);
			if (sts != 0)
			{
				__SS__ << "Failed mu2edev::write_data with M_IOC_GET_INFO... return not 0." << __E__;
				perror(ss.str().c_str());
				__SS_THROW__;
				// exit(1);
			}
			mu2e_channel_info_[activeDeviceIndex_][chn][dir] = get_info;  // copy info struct
			delta = mu2e_chn_info_delta_(activeDeviceIndex_, chn, dir, &mu2e_channel_info_);
			usleep(1000);
		}

		if (delta <= 1)
		{
			__SS__ << "Failed mu2edev::write_data with HW_NOT_READING_BUFS." << __E__;
			perror(ss.str().c_str());
			__SS_THROW__;
			// kill(0, SIGUSR2);
			// exit(2);
		}

		unsigned idx = mu2e_channel_info_[activeDeviceIndex_][chn][dir].swIdx;
		void* data = ((mu2e_databuff_t*)(mu2e_mmap_ptrs_[activeDeviceIndex_][chn][dir][MU2E_MAP_BUFF]))[idx];
		memcpy(data, buffer, bytes);
		unsigned long arg = (chn << 24) | (bytes & 0xffffff);  // THIS OBIVOUSLY SHOULD BE A MACRO

		int retry = 15;
		do
		{
			retsts = ioctl(devfd_, M_IOC_BUF_XMIT, arg);
			if (retsts != 0)
			{
				TRACE(TLVL_WRITE_DATA, UID_ + " - write_data ioctl returned %d, errno=%d (%s), retrying.", retsts, errno, strerror(errno));
				// perror("M_IOC_BUF_XMIT");
				usleep(50000);
			}  // exit(1); } // Take out the exit call for now
			retry--;
		} while (retry > 0 && retsts != 0);
		// increment our cached info
		if (retsts == 0)
		{
			mu2e_channel_info_[activeDeviceIndex_][chn][dir].swIdx =
				idx_add(mu2e_channel_info_[activeDeviceIndex_][chn][dir].swIdx, 1, activeDeviceIndex_, chn, dir);
		}
	}
	deviceTime_ += std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count();
	if (retsts >= 0) writeSize_ += bytes;
	return retsts;
}  // end write_data()

int mu2edev::release_all(DTC_DMA_Engine const& chn)
{
	TLOG_DEBUG(TLVL_RELEASE_ALL) << __PRETTY_FUNCTION__ << " called from\n"
								 << otsStyleStackTrace();
	auto retsts = 0;
	TRACE_EXIT { TLOG_DEBUG(TLVL_RELEASE_ALL) << "Exit - retsts=" << retsts; };
	if (chn == DTC_DMA_Engine_DCS && !thread_owns_dcs_lock())
	{
		TRACE(TLVL_WARN, UID_ + " - release_all dcs lock not held!");
		retsts = -2;
		return retsts;
	}
	auto start = std::chrono::steady_clock::now();
	auto time_last_data = start;

	if (simulator_ != nullptr)
	{
		TRACE(TLVL_RELEASE_ALL, UID_ + " - release all!");
		retsts = simulator_->release_all(chn);
	}
	else
	{
		constexpr size_t kTimeCheckIntervalLoops = 100;  // Recheck elapsed time every 100 loop iterations to avoid hot-spin clock polling.
		size_t time_check_counter = 0;
		while (1)
		{
			auto _tmo_ms = mu2e_channel_info_[activeDeviceIndex_][chn][C2S].tmo_ms;
			mu2e_channel_info_[activeDeviceIndex_][chn][C2S].tmo_ms = 0;
			int sts = ioctl(devfd_, M_IOC_GET_INFO, &mu2e_channel_info_[activeDeviceIndex_][chn][C2S]);
			mu2e_channel_info_[activeDeviceIndex_][chn][C2S].tmo_ms = _tmo_ms;  // restore
			if (sts != 0)
			{
				__SS__ << "Failed mu2edev::release_all of chn=" << chn << " with M_IOC_GET_INFO... return " << sts << " which is not 0. " << strerror(errno) << __E__;
				perror(ss.str().c_str());
				__SS_THROW__;
			}
			auto has_recv_data = mu2e_chn_info_delta_(activeDeviceIndex_, chn, C2S, &mu2e_channel_info_);  // reads cached value, need M_IOC_GET_INFO before to update

			if (has_recv_data)
			{
				TRACE(TLVL_RELEASE_ALL, UID_ + " - release_all calling read_release(chn=%d has_recv_data=%d", chn, has_recv_data);
				read_release(chn, has_recv_data);
				time_last_data = std::chrono::steady_clock::now();
			}

			// do not worry about silence on release_all for DCS (since we have lock)
			if (chn == DTC_DMA_Engine_DCS)
			{
				if (has_recv_data) continue;

				TRACE(TLVL_RELEASE_ALL, UID_ + " - release_all no data to release for chn=%d", chn);
				break;
			}

			if (++time_check_counter >= kTimeCheckIntervalLoops)
			{
				time_check_counter = 0;
				auto now = std::chrono::steady_clock::now();
				auto nano_since_last_data = std::chrono::duration_cast<std::chrono::nanoseconds>(now - time_last_data).count();
				if (!has_recv_data && nano_since_last_data > 1000000000LL)  // require a full 1 second of buffer idle before declaring release_all complete
				{
					TRACE(TLVL_RELEASE_ALL, UID_ + " - release_all done after buffers idle for >= 1s...");
					break;
				}

				auto nano_since_start = std::chrono::duration_cast<std::chrono::nanoseconds>(now - start).count();
				if (nano_since_start > 5000000000LL)  // 5 seconds overall cap
				{
					__SS__ << "mu2edev::release_all of chn=" << chn << " timed out after 5 seconds while attempting to release buffers (data never went idle for the required 1 second)." << __E__;
					__SS_THROW__;
				}
			}
		}

		// releaseBuffersHeld if allowing multiple buffers to be held by user space (e.g. for Data DMA channel, which is different for CFO vs DTC)
		if (chn == DTC_DMA_Engine_DAQ)
			buffers_held_ = 0;

		// Stamp first qword of every receive buffer with 0xdeadbeef so the next
		// consumer can tell which buffers have been freshly filled by hardware
		// (anything still reading 0xdeadbeef at qword[0] has not been touched
		// since this release_all completed).
		{
			auto* bufArray = (mu2e_databuff_t*)(mu2e_mmap_ptrs_[activeDeviceIndex_][chn][C2S][MU2E_MAP_BUFF]);
			for (auto bufIdx = 0; bufIdx < MU2E_NUM_RECV_BUFFS; ++bufIdx)
			{
				*reinterpret_cast<uint64_t*>(bufArray[bufIdx]) = 0xdeadbeefULL;
			}
			unsigned lastReleasedIdx = idx_add(mu2e_channel_info_[activeDeviceIndex_][chn][C2S].swIdx, -1, activeDeviceIndex_, chn, C2S);
			TRACE(TLVL_RELEASE_ALL, UID_ + " - release_all stamped 0xdeadbeef on first qword of all %d receive buffers (chn=%d) last cleared buffer index #%02u",
				  MU2E_NUM_RECV_BUFFS, chn, lastReleasedIdx);
		}
	}
	deviceTime_ += std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count();
	return retsts;
}  // end release_all()

void mu2edev::close()
{
	if (simulator_ != nullptr)
	{
		delete simulator_;
		simulator_ = nullptr;
		TLOG(TLVL_INFO) << UID_ << " " << __PRETTY_FUNCTION__ << " - mu2edev::close() closed simulated device";
	}
	else if (devfd_ != -1)
	{
		// mmap also keeps handle to the device! so must unmap
		for (int chn = 0; chn < MU2E_MAX_CHANNELS; ++chn)
			for (unsigned dir = 0; dir < 2; ++dir)
				for (unsigned map = 0; map < 2; ++map)
				{
					if (mu2e_mmap_ptrs_[activeDeviceIndex_][chn][dir][map] &&
						mu2e_mmap_ptrs_[activeDeviceIndex_][chn][dir][map] != MAP_FAILED)
					{
						size_t length = mu2e_channel_info_[activeDeviceIndex_][chn][dir].num_buffs *
										((map == MU2E_MAP_BUFF) ? mu2e_channel_info_[activeDeviceIndex_][chn][dir].buff_size : sizeof(int));

						TRACE(TLVL_INIT_DMA_ENGINE, UID_ + " - mu2edev::init chnDirMap2offset=%lu mu2e_mmap_ptrs_[offset][%d][%d][%d]=%p p=prot l=%lu",
							  // offset,
							  activeDeviceIndex_, chn,
							  dir, map, mu2e_mmap_ptrs_[activeDeviceIndex_][chn][dir][map],
							  // prot == PROT_READ ? 'R' : 'W',
							  length);

						munmap((void*)mu2e_mmap_ptrs_[activeDeviceIndex_][chn][dir][map],
							   length);
						mu2e_mmap_ptrs_[activeDeviceIndex_][chn][dir][map] = nullptr;
					}
				}

		// after unmap, then close device
		::close(devfd_);
		devfd_ = -1;
		TLOG(TLVL_INFO) << UID_ << " " << __PRETTY_FUNCTION__ << " - mu2edev::close() closed device file descriptor";
	}
	TLOG(TLVL_DEBUG) << UID_ << " " << __PRETTY_FUNCTION__ << " - mu2edev::close() done: " << otsStyleStackTrace() << __E__;
}  // end close()

void mu2edev::begin_dcs_transaction()
{
	TLOG_DEBUG(TLVL_BEGIN_DCS_TRANSACTION) << UID_ << " " << __PRETTY_FUNCTION__ << " called from\n"
										   << otsStyleStackTrace();
	TRACE(TLVL_BEGIN_DCS_TRANSACTION, UID_ + " begin_dcs_transation: who has lock? dcs_lock_held_=%llu, threadid=%llu", dcs_locks_[activeDeviceIndex_].lock_count.load(), std::this_thread::get_id());
	if (thread_owns_dcs_lock())
	{
		TRACE(TLVL_BEGIN_DCS_TRANSACTION, UID_ + " begin_dcs_transation: device lock already held by this thread. counter=%d, threadid=%llu", dcs_locks_[activeDeviceIndex_].lock_count.load(), std::this_thread::get_id());
		dcs_locks_[activeDeviceIndex_].lock_count++;
		return;
	}
	if (!dcs_lock_free())
		TRACE(TLVL_BEGIN_DCS_TRANSACTION, UID_ + " begin_dcs_transaction: device lock for this instance held by another thread! Waiting... counter=%d, threadid=%llu", dcs_locks_[activeDeviceIndex_].lock_count.load(), std::this_thread::get_id());

	int tmo_ms = 1000;  // 1s timeout
	auto start = std::chrono::steady_clock::now();
	TRACE(TLVL_BEGIN_DCS_TRANSACTION, UID_ + " begin_dcs_transaction: waiting for library thread lock");
	while (!dcs_lock_free() && !thread_owns_dcs_lock() && (tmo_ms <= 0 || std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count() < tmo_ms))
	{
		std::this_thread::sleep_for(std::chrono::microseconds(100));
	}

	if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count() >= tmo_ms)
	{
		TRACE(TLVL_ERROR, UID_ + " begin_dcs_transaction: timed out waiting for library thread lock");
		std::string file = __FILE__;
		throw std::runtime_error(
			std::string(file.find("/srcs/") != std::string::npos ? file.substr(file.find("/srcs") + 6) : file) + ":" +
			std::to_string(__LINE__) + " | " +
			UID_ + " - mu2e Device could not take lock - library-internal lock error. Throwing exception.");
	}

	if (simulator_ != nullptr)
	{
		dcs_locks_[activeDeviceIndex_].thread_id = std::this_thread::get_id();
		dcs_locks_[activeDeviceIndex_].lock_count++;
		TRACE(TLVL_BEGIN_DCS_TRANSACTION, UID_ + " begin_dcs_transaction: sim mode, taking library thread lock and returning, counter=%d, threadid=%llu", dcs_locks_[activeDeviceIndex_].lock_count.load(), std::this_thread::get_id());
		return;
	}

	TRACE(TLVL_BEGIN_DCS_TRANSACTION, UID_ + " begin_dcs_transaction: waiting for driver lock");
	int retsts = -1;
	while (retsts == -1 && (tmo_ms <= 0 || std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count() < tmo_ms))
	{
		retsts = ioctl(devfd_, M_IOC_DCS_LOCK);
		if ((retsts == -EAGAIN) || ((retsts == -1) && (errno == EAGAIN)))
		{
			TRACE(TLVL_BEGIN_DCS_TRANSACTION_VERBOSE, UID_ + " begin_dcs_transaction: ioctl returned %d (errno %d), waiting and retrying, counter=%d, threadid=%llu", retsts, errno, dcs_locks_[activeDeviceIndex_].lock_count.load(), std::this_thread::get_id());
			std::this_thread::sleep_for(std::chrono::microseconds(100));
		}
		else if (retsts != 0)
		{
			TRACE(TLVL_BEGIN_DCS_TRANSACTION, UID_ + " begin_dcs_transaction: Method not supported by driver, taking library lock and returning. ioctl returned %d, errno %d", retsts, errno);
			dcs_locks_[activeDeviceIndex_].thread_id = std::this_thread::get_id();
			dcs_locks_[activeDeviceIndex_].lock_count++;
			TRACE(TLVL_BEGIN_DCS_TRANSACTION, UID_ + " begin_dcs_transaction: Method not supported by driver, taking library lock and returning. ioctl returned %d, errno %d, counter=%d, threadid=%llu", retsts, errno, dcs_locks_[activeDeviceIndex_].lock_count.load(), std::this_thread::get_id());
			return;
		}
		else
		{
			dcs_locks_[activeDeviceIndex_].thread_id = std::this_thread::get_id();
			dcs_locks_[activeDeviceIndex_].lock_count++;
			TRACE(TLVL_BEGIN_DCS_TRANSACTION, UID_ + " begin_dcs_transaction: have driver lock, setting library lock and returning true, counter=%d, threadid=%llu", dcs_locks_[activeDeviceIndex_].lock_count.load(), std::this_thread::get_id());
			return;
		}
	}

	TRACE(TLVL_ERROR, UID_ + " begin_dcs_transaction: timed out waiting for driver lock");
	// force unlock
	end_dcs_transaction(true /* force */);
	std::string file = __FILE__;
	throw std::runtime_error(
		std::string(file.find("/srcs/") != std::string::npos ? file.substr(file.find("/srcs") + 6) : file) + ":" +
		std::to_string(__LINE__) + " | " +
		UID_ + " - mu2e Device could not take lock - M_IOC_DCS_LOCK error. Attempting to force unlock and throwing exception.");
}  // end begin_dcs_transaction()

void mu2edev::end_dcs_transaction(bool force)
{
	TLOG_DEBUG(TLVL_END_DCS_TRANSACTION) << UID_ << " " << __PRETTY_FUNCTION__ << " called from\n"
										 << otsStyleStackTrace();
	TRACE(TLVL_END_DCS_TRANSACTION, UID_ + " end_dcs_transaction: checking for ability to release lock force=%d, counter=%d", force, dcs_locks_[activeDeviceIndex_].lock_count.load());
	if (force || thread_owns_dcs_lock())
	{
		dcs_locks_[activeDeviceIndex_].lock_count--;
		TRACE(TLVL_END_DCS_TRANSACTION, UID_ + " end_dcs_transaction: after decrement lock counter, force=%d, counter=%d", force, dcs_locks_[activeDeviceIndex_].lock_count.load());
		if (dcs_locks_[activeDeviceIndex_].lock_count.load() <= 0 || force)
		{
			dcs_locks_[activeDeviceIndex_].lock_count = 0;
			if (simulator_ == nullptr)
			{
				TRACE(TLVL_END_DCS_TRANSACTION, UID_ + " end_dcs_transaction: releasing driver lock");
				int retsts = ioctl(devfd_, M_IOC_DCS_RELEASE);
				if (retsts != 0)
				{
					TRACE(TLVL_END_DCS_TRANSACTION, UID_ + " end_dcs_transaction: IOCTL returned %d!", retsts);
					perror("M_IOC_DCS_RELEASE");
				}
			}
			TRACE(TLVL_END_DCS_TRANSACTION, UID_ + " end_dcs_transaction: releasing library lock");
			dcs_locks_[activeDeviceIndex_].thread_id = NULL_TID;
		}
	}

}  // end end_dcs_transaction()

bool mu2edev::thread_owns_dcs_lock()
{
	return dcs_locks_[activeDeviceIndex_].thread_id.load() == std::this_thread::get_id();
}
bool mu2edev::dcs_lock_free()
{
	return dcs_locks_[activeDeviceIndex_].thread_id.load() == NULL_TID;
}

std::string mu2edev::get_driver_version()
{
	TRACE(TLVL_GET_DRIVER_VERSION, UID_ + " get_driver_version BEGIN");
	if (simulator_ != nullptr) { return "SIMULATED"; }
	mu2e_string_t output;
	int retsts = ioctl(devfd_, M_IOC_GET_VERSION, &output);

	if (retsts != 0)
	{
		TRACE(TLVL_GET_DRIVER_VERSION, UID_ + " get_driver_version: IOCTL returned %d!", retsts);
		perror("M_IOC_GET_VERSION");
	}

	std::string outstr = std::string(output);
	if (outstr == "")
	{
		return "UNSUPPORTED_DRIVER";
	}

	return outstr;
}  // end get_driver_version

/// @brief Dump contents of receive buffers in a loop with various options for controlling format and behavior. Intended for debugging and development use, not for production use (can cause log-file chaos if used excessively).
/// @param chn
/// @param optsmsk - bitmask of options controlling behavior and format.
///		Bit 0 (1) to suppress the initial screen clear; when this bit is not set, the screen is cleared once before the loop starts,
///		bit 1 (2) to run only one iteration,
///		bit 2 (4) reserved/legacy screen-clear option (current implementation does not clear between iterations),
///		bit 3 (8) to dump all 8192 bytes of each buffer instead of just first 4 qwords,
///		bit 4 (16) to include stack trace at end of output,
///		bit 28 (268435456) to force spy to run even if it has already been called once for this instance (which is usually enough and more can cause log-file chaos).
///			spyHasOccurred_ can also be reset to force spy to trigger once again, by calling mu2edev::resetSpyHasOccurred()
/// @param out
void mu2edev::spy(int chn, unsigned optsmsk, std::ostream& out /* = std::cout */)
{
	if (!(optsmsk & (1 << 28)) &&       // set bit 28 to force spy to happen
		spyHasOccurred_ && !TTEST(50))  // set debug+50 to re-enable if you know what you're doing and want to see more than the first spy() call for a given instance in the logs (which is usually enough and more can cause log-file chaos)
	{
		TLOG(TLVL_WARNING) << "spy() already executed for this mu2edev instance (" << UID_ << "); skipping to avoid log-file chaos. Call resetSpyHasOccurred() to re-enable.";
		return;
	}
	if (!(optsmsk & (1 << 28)))  // only flag spy occurring if not forcing it (with bit 28)
	{
		TLOG_INFO() << "Flagging that spy() has occurred.";
		spyHasOccurred_ = true;
	}

	TLOG_INFO() << "spy";
	void* buffer;
	uint64_t* datap;
	out << "optsmsk=" << optsmsk << '\n';
	if (!(optsmsk & 1)) out << "\033[0;0H\033[J";
	while (++spyIteration_)
	{  // watch out (when using integer type) for compiler error: iteration 2147483647 invokes undefined behavior [-Werror=aggressive-loop-optimizations]
		unsigned lastReleasedIdx = idx_add(mu2e_channel_info_[activeDeviceIndex_][chn][C2S].swIdx, -1, activeDeviceIndex_, chn, C2S);
		out << "spy iteration: " << std::dec << std::setw(7) << std::setfill(' ') << spyIteration_
			<< "  -- last released buffer index #" << std::setw(2) << std::setfill('0') << lastReleasedIdx << '\n';
		for (auto bufIdx = 0; bufIdx < MU2E_NUM_RECV_BUFFS;)
		{
			out << std::dec << std::setw(3) << std::setfill(' ') << bufIdx << " ";
			for (auto buf = 0; buf < ((optsmsk & 8) ? 1 : 3); ++buf)
			{
				buffer = ((mu2e_databuff_t*)(mu2e_mmap_ptrs_[activeDeviceIndex_][chn][C2S][MU2E_MAP_BUFF]))[bufIdx++];
				datap = (uint64_t*)buffer;
				out << "0x" << std::hex << std::setw(16) << std::setfill('0') << datap[0];
				for (auto dd = 1; dd < ((optsmsk & 8) ? 13 : 4); ++dd)
				{
					out << " " << std::hex << std::setw(16) << std::setfill('0') << datap[dd];
				}
				if (optsmsk & 8)
				{
					constexpr size_t totalQwords = sizeof(mu2e_databuff_t) / sizeof(uint64_t);  // 8192
					out << " ...";
					for (size_t dd = totalQwords - 13; dd < totalQwords; ++dd)
					{
						out << " " << std::hex << std::setw(16) << std::setfill('0') << datap[dd];
					}
				}
				if (!(bufIdx < MU2E_NUM_RECV_BUFFS)) break;
				if (buf < 3) out << "   ";
			}
			out << '\n';
		}
		if (optsmsk & 2) break;
		sleep(1);  // user should control-C here :)
		if (optsmsk & 4) out << "\033[0;0H";
	}
	if (optsmsk & 16)
		out << "\n\n"
			<< otsStyleStackTrace() << std::flush;
}  // end spy()
