#ifndef DTCLibTest_H
#define DTCLibTest_H

#include "DTC.h"

#include <atomic>
#include <thread>

namespace DTCLib
{
/// <summary>
/// Unit tests for DTC Library
/// </summary>
class DTCLibTest
{
  public:
	/// <summary>
	/// Construct the test adapter
	/// </summary>
	DTCLibTest();
	virtual ~DTCLibTest();

	// Test Control
	/// <summary>
	/// Run the specified tests
	/// </summary>
	/// <param name="classEnabled">Run class constructor/destructor test</param>
	/// <param name="regIOEnabled">Run register I/O test</param>
	/// <param name="daqEnabled">Run DAQ data test</param>
	/// <param name="dcsEnabled">Run DCS read/write test</param>
	/// <param name="nTests">Number of times to repeat tests</param>
	/// <param name="printMessages">Print debuffing messages (Default: false)</param>
	void startTest(bool classEnabled, bool regIOEnabled, bool daqEnabled, bool dcsEnabled, int nTests, bool printMessages = false);
	/// <summary>
	/// Abort testing
	/// </summary>
	void stopTests();

	// Accessors
	/// <summary>
	/// Determine whether tests are running
	/// </summary>
	/// <returns>Whether tests are running</returns>
	bool isRunning() const { return running_; }

	/// <summary>
	/// Get the number of instances of the Class test that passed
	/// </summary>
	/// <returns>The number of instances of the Class test that passed</returns>
	int classPassed();
	/// <summary>
	/// Get the number of instances of the Class test that failed
	/// </summary>
	/// <returns>The number of instances of the Class test that failed</returns>
	int classFailed();
	/// <summary>
	/// Get the number of instances of the Register test that passed
	/// </summary>
	/// <returns>The number of instances of the Register test that passed</returns>
	int regPassed();
	/// <summary>
	/// Get the number of instances of the Register test that failed
	/// </summary>
	/// <returns>The number of instances of the Register test that failed</returns>
	int regFailed();
	/// <summary>
	/// Get the number of instances of the DAQ test that passed
	/// </summary>
	/// <returns>The number of instances of the DAQ test that passed</returns>
	int daqPassed();
	/// <summary>
	/// Get the number of instances of the DAQ test that failed
	/// </summary>
	/// <returns>The number of instances of the DAQ test that failed</returns>
	int daqFailed();
	/// <summary>
	/// Get the number of instances of the DCS test that passed
	/// </summary>
	/// <returns>The number of instances of the DCS test that passed</returns>
	int dcsPassed();
	/// <summary>
	/// Get the number of instances of the DCS test that failed
	/// </summary>
	/// <returns>The number of instances of the DCS test that failed</returns>
	int dcsFailed();

  private:
	// Test Worker
	void doTests();
	void doClassTest();
	void doRegTest();
	void doDCSTest();
	void doDAQTest();

	static bool DataPacketIntegrityCheck(DTC_DataPacket*);

	// Test Status
	std::atomic<bool> running_;

	std::atomic<int> classPassed_;
	std::atomic<int> classFailed_;
	std::atomic<int> regPassed_;
	std::atomic<int> regFailed_;
	std::atomic<int> daqPassed_;
	std::atomic<int> daqFailed_;
	std::atomic<int> dcsPassed_;
	std::atomic<int> dcsFailed_;

	int classPassedTemp_;
	int classFailedTemp_;
	int regPassedTemp_;
	int regFailedTemp_;
	int daqPassedTemp_;
	int daqFailedTemp_;
	int dcsPassedTemp_;
	int dcsFailedTemp_;

	// Test Internals
	DTC* thisDTC_;
	int  nTests_;

	bool runClassTest_;
	bool runRegTest_;
	bool runDAQTest_;
	bool runDCSTest_;

	bool        printMessages_;
	std::thread workerThread_;
};
}  // namespace DTCLib
#endif  // ifndef DTCLibTest_H
