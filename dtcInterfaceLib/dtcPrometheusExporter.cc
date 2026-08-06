// dtcPrometheusExporter.cc
// Prometheus metrics HTTP exporter for the Mu2e DTC (Data Transfer Controller).
// Serves DTC hardware counters and status registers in Prometheus text format
// on a configurable TCP port. Intended for use with a Prometheus scraper.

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include <csignal>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>

#include "dtcInterfaceLib/DTC_Registers.h"

namespace {
volatile sig_atomic_t g_running = 1;

void signalHandler(int /*sig*/) { g_running = 0; }

// Writes a Prometheus gauge sample line (without HELP/TYPE header).
void writeGaugeSample(std::ostringstream& oss, const std::string& name, const std::string& labels, double value)
{
	oss << name << "{" << labels << "} " << value << "\n";
}

// Writes a Prometheus counter sample line (without HELP/TYPE header).
void writeCounterSample(std::ostringstream& oss, const std::string& name, const std::string& labels, uint64_t value)
{
	oss << name << "{" << labels << "} " << value << "\n";
}

std::string collectMetrics(DTCLib::DTC_Registers& dtc, int dtcId)
{
	std::ostringstream oss;
	const std::string dtcLabel = "dtc=\"" + std::to_string(dtcId) + "\"";

	// FPGA monitoring (gauges)
	oss << "# HELP dtc_fpga_temperature_celsius FPGA die temperature in Celsius\n";
	oss << "# TYPE dtc_fpga_temperature_celsius gauge\n";
	try
	{
		writeGaugeSample(oss, "dtc_fpga_temperature_celsius", dtcLabel, dtc.ReadFPGATemperature());
	}
	catch (...)
	{}

	oss << "# HELP dtc_fpga_vccint_volts FPGA VCCINT supply voltage in Volts\n";
	oss << "# TYPE dtc_fpga_vccint_volts gauge\n";
	try
	{
		writeGaugeSample(oss, "dtc_fpga_vccint_volts", dtcLabel, dtc.ReadFPGAVCCINTVoltage());
	}
	catch (...)
	{}

	oss << "# HELP dtc_fpga_vccaux_volts FPGA VCCAUX supply voltage in Volts\n";
	oss << "# TYPE dtc_fpga_vccaux_volts gauge\n";
	try
	{
		writeGaugeSample(oss, "dtc_fpga_vccaux_volts", dtcLabel, dtc.ReadFPGAVCCAUXVoltage());
	}
	catch (...)
	{}

	oss << "# HELP dtc_fpga_vccbram_volts FPGA VCCBRAM supply voltage in Volts\n";
	oss << "# TYPE dtc_fpga_vccbram_volts gauge\n";
	try
	{
		writeGaugeSample(oss, "dtc_fpga_vccbram_volts", dtcLabel, dtc.ReadFPGAVCCBRAMVoltage());
	}
	catch (...)
	{}

	// Per-link packet counters (counters)
	static const DTCLib::DTC_Link_ID kLinks[] = {
		DTCLib::DTC_Link_0, DTCLib::DTC_Link_1, DTCLib::DTC_Link_2,
		DTCLib::DTC_Link_3, DTCLib::DTC_Link_4, DTCLib::DTC_Link_5};
	static constexpr int kNumLinks = 6;

	oss << "# HELP dtc_tx_data_request_packets_total Number of TX data request packets sent per ROC link\n";
	oss << "# TYPE dtc_tx_data_request_packets_total counter\n";
	for (int i = 0; i < kNumLinks; ++i)
	{
		try
		{
			writeCounterSample(oss, "dtc_tx_data_request_packets_total",
							   dtcLabel + ",link=\"" + std::to_string(i) + "\"",
							   dtc.ReadTXDataRequestPacketCount(kLinks[i]));
		}
		catch (...)
		{}
	}

	oss << "# HELP dtc_tx_heartbeat_packets_total Number of TX heartbeat packets sent per ROC link\n";
	oss << "# TYPE dtc_tx_heartbeat_packets_total counter\n";
	for (int i = 0; i < kNumLinks; ++i)
	{
		try
		{
			writeCounterSample(oss, "dtc_tx_heartbeat_packets_total",
							   dtcLabel + ",link=\"" + std::to_string(i) + "\"",
							   dtc.ReadTXHeartbeatPacketCount(kLinks[i]));
		}
		catch (...)
		{}
	}

	oss << "# HELP dtc_tx_event_window_markers_total Number of TX event window marker packets sent per ROC link\n";
	oss << "# TYPE dtc_tx_event_window_markers_total counter\n";
	for (int i = 0; i < kNumLinks; ++i)
	{
		try
		{
			writeCounterSample(oss, "dtc_tx_event_window_markers_total",
							   dtcLabel + ",link=\"" + std::to_string(i) + "\"",
							   dtc.ReadTXEventWindowMarkerCount(kLinks[i]));
		}
		catch (...)
		{}
	}

	oss << "# HELP dtc_tx_null_heartbeat_packets_total Number of TX null heartbeat packets sent per ROC link\n";
	oss << "# TYPE dtc_tx_null_heartbeat_packets_total counter\n";
	for (int i = 0; i < kNumLinks; ++i)
	{
		try
		{
			writeCounterSample(oss, "dtc_tx_null_heartbeat_packets_total",
							   dtcLabel + ",link=\"" + std::to_string(i) + "\"",
							   dtc.ReadTXNullHeartbeatCount(kLinks[i]));
		}
		catch (...)
		{}
	}

	oss << "# HELP dtc_rx_data_header_packets_total Number of RX data header packets received per ROC link\n";
	oss << "# TYPE dtc_rx_data_header_packets_total counter\n";
	for (int i = 0; i < kNumLinks; ++i)
	{
		try
		{
			writeCounterSample(oss, "dtc_rx_data_header_packets_total",
							   dtcLabel + ",link=\"" + std::to_string(i) + "\"",
							   dtc.ReadRXDataHeaderPacketCount(kLinks[i]));
		}
		catch (...)
		{}
	}

	oss << "# HELP dtc_rx_data_packets_total Number of RX data packets received per ROC link\n";
	oss << "# TYPE dtc_rx_data_packets_total counter\n";
	for (int i = 0; i < kNumLinks; ++i)
	{
		try
		{
			writeCounterSample(oss, "dtc_rx_data_packets_total",
							   dtcLabel + ",link=\"" + std::to_string(i) + "\"",
							   dtc.ReadRXDataPacketCount(kLinks[i]));
		}
		catch (...)
		{}
	}

	oss << "# HELP dtc_retransmit_requests_total Number of retransmit requests per ROC link\n";
	oss << "# TYPE dtc_retransmit_requests_total counter\n";
	for (int i = 0; i < kNumLinks; ++i)
	{
		try
		{
			writeCounterSample(oss, "dtc_retransmit_requests_total",
							   dtcLabel + ",link=\"" + std::to_string(i) + "\"",
							   dtc.ReadRetransmitRequestCount(kLinks[i]));
		}
		catch (...)
		{}
	}

	oss << "# HELP dtc_missed_cfo_packets_total Number of missed CFO packets per ROC link\n";
	oss << "# TYPE dtc_missed_cfo_packets_total counter\n";
	for (int i = 0; i < kNumLinks; ++i)
	{
		try
		{
			writeCounterSample(oss, "dtc_missed_cfo_packets_total",
							   dtcLabel + ",link=\"" + std::to_string(i) + "\"",
							   dtc.ReadMissedCFOPacketCount(kLinks[i]));
		}
		catch (...)
		{}
	}

	// SERDES error counters — ROC links + CFO link
	static const DTCLib::DTC_Link_ID kAllLinks[] = {
		DTCLib::DTC_Link_0, DTCLib::DTC_Link_1, DTCLib::DTC_Link_2,
		DTCLib::DTC_Link_3, DTCLib::DTC_Link_4, DTCLib::DTC_Link_5,
		DTCLib::DTC_Link_CFO};
	static const char* kAllLinkNames[] = {"0", "1", "2", "3", "4", "5", "cfo"};
	static constexpr int kNumAllLinks = 7;

	oss << "# HELP dtc_serdes_character_not_in_table_errors_total SERDES character-not-in-table error count per link\n";
	oss << "# TYPE dtc_serdes_character_not_in_table_errors_total counter\n";
	for (int i = 0; i < kNumAllLinks; ++i)
	{
		try
		{
			writeCounterSample(oss, "dtc_serdes_character_not_in_table_errors_total",
							   dtcLabel + ",link=\"" + kAllLinkNames[i] + "\"",
							   dtc.ReadSERDESCharacterNotInTableErrorCount(kAllLinks[i]));
		}
		catch (...)
		{}
	}

	oss << "# HELP dtc_serdes_rx_disparity_errors_total SERDES RX disparity error count per link\n";
	oss << "# TYPE dtc_serdes_rx_disparity_errors_total counter\n";
	for (int i = 0; i < kNumAllLinks; ++i)
	{
		try
		{
			writeCounterSample(oss, "dtc_serdes_rx_disparity_errors_total",
							   dtcLabel + ",link=\"" + kAllLinkNames[i] + "\"",
							   dtc.ReadSERDESRXDisparityErrorCount(kAllLinks[i]));
		}
		catch (...)
		{}
	}

	oss << "# HELP dtc_serdes_rx_prbs_errors_total SERDES RX PRBS error count per link\n";
	oss << "# TYPE dtc_serdes_rx_prbs_errors_total counter\n";
	for (int i = 0; i < kNumAllLinks; ++i)
	{
		try
		{
			writeCounterSample(oss, "dtc_serdes_rx_prbs_errors_total",
							   dtcLabel + ",link=\"" + kAllLinkNames[i] + "\"",
							   dtc.ReadSERDESRXPRBSErrorCount(kAllLinks[i]));
		}
		catch (...)
		{}
	}

	oss << "# HELP dtc_serdes_rx_crc_errors_total SERDES RX CRC error count per link\n";
	oss << "# TYPE dtc_serdes_rx_crc_errors_total counter\n";
	for (int i = 0; i < kNumAllLinks; ++i)
	{
		try
		{
			writeCounterSample(oss, "dtc_serdes_rx_crc_errors_total",
							   dtcLabel + ",link=\"" + kAllLinkNames[i] + "\"",
							   dtc.ReadSERDESRXCRCErrorCount(kAllLinks[i]));
		}
		catch (...)
		{}
	}

	// EVB SERDES and jitter attenuator counters
	oss << "# HELP dtc_evb_serdes_rx_packet_errors_total EVB SERDES RX packet error count\n";
	oss << "# TYPE dtc_evb_serdes_rx_packet_errors_total counter\n";
	try
	{
		writeCounterSample(oss, "dtc_evb_serdes_rx_packet_errors_total", dtcLabel,
						   dtc.ReadEVBSERDESRXPacketErrorCounter());
	}
	catch (...)
	{}

	oss << "# HELP dtc_jitter_attenuator_recovered_clock_los_total Jitter attenuator recovered clock loss-of-signal event count\n";
	oss << "# TYPE dtc_jitter_attenuator_recovered_clock_los_total counter\n";
	try
	{
		writeCounterSample(oss, "dtc_jitter_attenuator_recovered_clock_los_total", dtcLabel,
						   dtc.ReadJitterAttenuatorRecoveredClockLOSCount());
	}
	catch (...)
	{}

	oss << "# HELP dtc_jitter_attenuator_external_clock_los_total Jitter attenuator external clock loss-of-signal event count\n";
	oss << "# TYPE dtc_jitter_attenuator_external_clock_los_total counter\n";
	try
	{
		writeCounterSample(oss, "dtc_jitter_attenuator_external_clock_los_total", dtcLabel,
						   dtc.ReadJitterAttenuatorExternalClockLOSCount());
	}
	catch (...)
	{}

	return oss.str();
}

// Sends all bytes in buf to fd, retrying on short writes.
void writeAll(int fd, const char* buf, size_t len)
{
	while (len > 0)
	{
		const ssize_t n = send(fd, buf, len, MSG_NOSIGNAL);
		if (n <= 0) break;
		buf += n;
		len -= static_cast<size_t>(n);
	}
}

void handleRequest(int clientFd, DTCLib::DTC_Registers& dtc, int dtcId)
{
	char buf[4096] = {};
	if (read(clientFd, buf, sizeof(buf) - 1) <= 0)
	{
		close(clientFd);
		return;
	}

	const std::string request(buf);
	const bool isMetrics = (request.find("GET /metrics") != std::string::npos);

	std::string body;
	std::string statusLine;
	std::string contentType;

	if (isMetrics)
	{
		body = collectMetrics(dtc, dtcId);
		statusLine = "HTTP/1.1 200 OK\r\n";
		contentType = "text/plain; version=0.0.4; charset=utf-8";
	}
	else
	{
		body = "Not Found. Use /metrics\n";
		statusLine = "HTTP/1.1 404 Not Found\r\n";
		contentType = "text/plain; charset=utf-8";
	}

	const std::string response = statusLine +
								 "Content-Type: " + contentType +
								 "\r\n"
								 "Content-Length: " +
								 std::to_string(body.size()) +
								 "\r\n"
								 "Connection: close\r\n\r\n" +
								 body;

	writeAll(clientFd, response.c_str(), response.size());
	close(clientFd);
}
}  // namespace

void printHelpMsg()
{
	std::cout << "Usage: dtcPrometheusExporter [options]\n"
			  << "Options:\n"
			  << "    -h: This message.\n"
			  << "    -d: DTC instance to use (defaults to DTCLIB_DTC env var if set, 0 otherwise)\n"
			  << "    -m: Use <file> as the emulated DTC memory area (default: mu2esim.bin)\n"
			  << "    -p: TCP port to listen on (default: 9100)\n";
	exit(0);
}

int main(int argc, char* argv[])
{
	int dtcId = -1;
	uint16_t port = 9100;
	std::string memFileName = "mu2esim.bin";

	for (int optind = 1; optind < argc; ++optind)
	{
		if (argv[optind][0] == '-')
		{
			switch (argv[optind][1])
			{
				case 'h':
					printHelpMsg();
					break;
				case 'd':
					dtcId = DTCLib::Utilities::getOptionValue(&optind, &argv);
					break;
				case 'm':
					memFileName = DTCLib::Utilities::getOptionString(&optind, &argv);
					break;
				case 'p':
					port = static_cast<uint16_t>(DTCLib::Utilities::getOptionValue(&optind, &argv));
					break;
				default:
					std::cerr << "Unknown option: " << argv[optind] << "\n";
					printHelpMsg();
					break;
			}
		}
	}

	signal(SIGINT, signalHandler);
	signal(SIGTERM, signalHandler);

	DTCLib::DTC_Registers dtc(DTCLib::DTC_SimMode_Disabled, dtcId, memFileName, 0x1, "", true);

	const int serverFd = socket(AF_INET, SOCK_STREAM, 0);
	if (serverFd < 0)
	{
		std::cerr << "Failed to create socket: " << strerror(errno) << "\n";
		return 1;
	}

	const int opt = 1;
	if (setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
	{
		std::cerr << "Warning: setsockopt SO_REUSEADDR failed: " << strerror(errno) << "\n";
	}

	sockaddr_in addr{};
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(port);

	if (bind(serverFd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
	{
		std::cerr << "Failed to bind to port " << port << ": " << strerror(errno) << "\n";
		close(serverFd);
		return 1;
	}

	if (listen(serverFd, 10) < 0)
	{
		std::cerr << "Failed to listen: " << strerror(errno) << "\n";
		close(serverFd);
		return 1;
	}

	std::cout << "DTC Prometheus exporter listening on :" << port << "/metrics (DTC " << dtcId << ")\n";

	while (g_running)
	{
		fd_set fds;
		struct timeval tv = {1, 0};
		FD_ZERO(&fds);
		FD_SET(serverFd, &fds);

		if (select(serverFd + 1, &fds, nullptr, nullptr, &tv) <= 0)
		{
			continue;
		}

		const int clientFd = accept(serverFd, nullptr, nullptr);
		if (clientFd >= 0)
		{
			handleRequest(clientFd, dtc, dtcId);
		}
	}

	close(serverFd);
	std::cout << "DTC Prometheus exporter stopped.\n";
	return 0;
}
