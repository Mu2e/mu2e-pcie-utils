#ifndef MU2EDEV_H
#define MU2EDEV_H

// This file (mu2edev.hh) was created by Ron Rechenmacher <ron@fnal.gov> on
// Feb 13, 2014. "TERMS AND CONDITIONS" governing this file are in the README
// or COPYING file. If you do not have such a file, one can be obtained by
// contacting Ron or Fermi Lab in Batavia IL, 60510, phone: 630-840-3000.
// $RCSfile: .emacs.gnu,v $
// rev="$Revision: 1.23 $$Date: 2012/01/23 15:32:40 $";

#include "mu2e_driver/mu2e_mmap_ioctl.h"  //

#include "mu2esim.h"

#include <atomic>
#include <chrono>
#include <thread>

/// <summary>
/// This class handles the raw interaction with the mu2e device driver
/// It also will pass through device commands to the mu2esim class if it is active.
/// </summary>
class mu2edev
{
public:
	/// <summary>
	/// Initialize counters and other data needed by the mu2edev class.
	/// Does not initialize the DTC, call init(DTC_SimMode) to do that.
	/// </summary>
	mu2edev();
	virtual ~mu2edev();

	/// <summary>
	/// Get the amount of time spent by the hardware executing requests, in seconds.
	/// Time counter is measured in nanoseconds, but this precision should not be relied upon, it's measured by the CPU
	/// not the hardware.
	/// </summary>
	/// <returns>Current amount of time spent in hardware calls, in seconds</returns>
	double GetDeviceTime() const { return deviceTime_ / 1000000000.0; }

	/// <summary>
	/// Reset the device time counter
	/// </summary>
	void ResetDeviceTime() { deviceTime_ = 0; }

	/// <summary>
	/// Get the current value of the write size counter
	/// </summary>
	/// <returns>Value of the write size counter</returns>
	size_t GetWriteSize() const { return writeSize_; }

	/// <summary>
	/// Reset the write size counter
	/// </summary>
	void ResetWriteSize() { writeSize_ = 0; }

	/// <summary>
	/// Gets the current value of the read size counter
	/// </summary>
	/// <returns>Value of the read size counter</returns>
	size_t GetReadSize() const { return readSize_; }

	/// <summary>
	/// Reset the read size counter
	/// </summary>
	void ResetReadSize() { readSize_ = 0; }

	/// <summary>
	/// Initialize the simulator if simMode requires it, otherwise set up DMA engines
	/// </summary>
	/// <param name="simMode">Desired simulation mode</param>
	/// <param name="dtc">Desired DTC card to use (/dev/mu2eX)</param>
	/// <param name="simMemoryFileName">If using simulated DTC, name of the memory file ("mu2esim.bin")</param>
	/// <returns>0 on success</returns>
	int init(DTCLib::DTC_SimMode simMode, int deviceIndex, std::string simMemoryFileName = "mu2esim.bin", const std::string& uid = "");
	void initDMAEngine();

	/// <summary>
	/// Reads data from the DTC.
	/// Returns the number of bytes read. Negative values indicate errors.
	/// </summary>
	/// <param name="chn">Channel to read</param>
	/// <param name="buffer">Pointer to output buffer</param>
	/// <param name="tmo_ms">Timeout for read</param>
	/// <returns>Byte count of data read into buffer. Negative value indicates error.</returns>
	int read_data(DTC_DMA_Engine const& chn, void** buffer, int tmo_ms);
	/// <summary>
	/// Release a number of buffers held by the software on the given channel
	/// </summary>
	/// <param name="chn">Channel to release</param>
	/// <param name="num">Number of buffers to release</param>
	/// <returns>0 when successful (always)</returns>
	int read_release(DTC_DMA_Engine const& chn, unsigned num);
	/// <summary>
	/// Release all buffers held by software on the given channel
	/// </summary>
	/// <param name="chn">Channel to release (DAQ or DCS)</param>
	/// <returns>0 on success</returns>
	int release_all(DTC_DMA_Engine const& chn);
	/// <summary>
	/// Read a DTC register
	/// </summary>
	/// <param name="address">Address to read</param>
	/// <param name="tmo_ms">Timeout for read</param>
	/// <param name="output">Pointer to output word</param>
	/// <returns>0 on success</returns>
	int read_register(uint16_t address, int tmo_ms, uint32_t* output);
	/// <summary>
	/// Write to a DTC register
	/// </summary>
	/// <param name="address">Address to write</param>
	/// <param name="tmo_ms">Timeout for write</param>
	/// <param name="data">Data to write</param>
	/// <returns>0 on success</returns>
	int write_register(uint16_t address, int tmo_ms, uint32_t data);
	/// <summary>
	/// Write to a DTC register
	/// </summary>
	/// <param name="address">Address to write</param>
	/// <param name="tmo_ms">Timeout for write</param>
	/// <param name="data">Data to write</param>
	/// <param name="output">Pointer to output word</param>
	/// <returns>0 on success</returns>
	int write_register_checked(uint16_t address, int tmo_ms, uint32_t data, uint32_t* output);
	/// <summary>
	/// Write out the DMA metadata to screen
	/// </summary>
	void meta_dump();
	/// <summary>
	/// Write data to the DTC
	/// </summary>
	/// <param name="chn">Channel to write</param>
	/// <param name="buffer">Buffer containing data to write</param>
	/// <param name="bytes">Size of the buffer, in bytes</param>
	/// <returns>0 on success</returns>
	int write_data(DTC_DMA_Engine const& chn, void* buffer, size_t bytes);
	/// <summary>
	/// Close the connection to the DTC
	/// </summary>
	void close();

	/// <summary>
	/// Gets the file descriptor for the mu2e block device (/dev/mu2eX)
	/// </summary>
	/// <returns>File descriptor for the mu2e block device</returns>
	int get_devfd_() const { return devfd_; }

	/// <summary>
	/// Get the current Device Index for this instance
	/// </summary>
	/// <returns>The current Device Index for this instance</returns>
	int getDeviceIndex() { return activeDeviceIndex_; }
	// int  read_pcie_state(m_ioc_pcistate_t *output);
	// int  read_dma_state(int chn, int dir, m_ioc_engstate_t *output);
	// int  read_dma_stats(m_ioc_engstats_t *output);
	// int  read_trn_stats(TRNStatsArray *output);
	// int  read_test_command(m_ioc_cmd_t *output);
	// int  write_test_command(m_ioc_cmd_t input, bool start);

	void begin_dcs_transaction();
	void end_dcs_transaction(bool force = false);
	bool thread_owns_dcs_lock();
	bool dcs_lock_free();
	std::string get_driver_version();

	/// <summary>
	/// Get the current DTC UID for this instance
	/// </summary>
	/// <returns>The current DTC UID for this instance</returns>
	std::string getDeviceUID() { return UID_; }

	/// <summary>
	/// For this DTC, "spy" on C2S buffers associated with chn.
	/// Output a small protion of each buffer to stdout until ^C.
	/// </summary>
	/// <returns>No value is returned.</returns>
	void spy(int chn, unsigned flags);

private:
	// unsigned delta_(int chn, int dir);

	int devfd_;
	volatile void* mu2e_mmap_ptrs_[MU2E_MAX_NUM_DTCS][MU2E_MAX_CHANNELS][2][2];
	m_ioc_get_info_t mu2e_channel_info_[MU2E_MAX_NUM_DTCS][MU2E_MAX_CHANNELS][2];
	unsigned buffers_held_;
	mu2esim* simulator_;
	int activeDeviceIndex_;
    struct DCSLock {
		std::atomic<std::thread::id> thread_id{std::thread::id()};
		std::atomic<int> lock_count{0};
    };
	static std::array<DCSLock, MU2E_MAX_NUM_DTCS> dcs_locks_;

	std::atomic<long long> deviceTime_;
	std::atomic<size_t> writeSize_;
	std::atomic<size_t> readSize_;

	std::string UID_;
	FILE* debugFp_ = 0;
	std::chrono::steady_clock::time_point lastWriteTime_;
};

#endif
