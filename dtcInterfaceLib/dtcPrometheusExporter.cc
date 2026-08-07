// dtcPrometheusExporter.cc
// Prometheus metrics HTTP exporter for Mu2e DTC/CFO register monitoring.

#include <arpa/inet.h>
#include <dirent.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <csignal>
#include <cctype>
#include <cstring>
#include <iostream>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "cfoInterfaceLib/CFO_Registers.h"
#include "dtcInterfaceLib/DTC_Registers.h"

namespace {
volatile sig_atomic_t g_running = 1;

struct ExporterConfig
{
	std::optional<int> dtcFilter;
	std::string memFileName;
};

void signalHandler(int /*sig*/) { g_running = 0; }

std::string escapeLabelValue(const std::string& in)
{
	std::string out;
	out.reserve(in.size());
	for (char c : in)
	{
		if (c == '\\' || c == '"') out.push_back('\\');
		out.push_back(c);
	}
	return out;
}

std::vector<int> discoverMu2eDeviceIndices()
{
	std::vector<int> ids;
	DIR* dir = opendir("/dev");
	if (!dir) return ids;

	while (auto* entry = readdir(dir))
	{
		const std::string name(entry->d_name);
		if (name.rfind("mu2e", 0) != 0 || name.size() <= 4) continue;
		const std::string suffix = name.substr(4);
		if (!std::all_of(suffix.begin(), suffix.end(), [](unsigned char ch) { return std::isdigit(ch) != 0; })) continue;
		ids.push_back(std::stoi(suffix));
	}
	closedir(dir);
	std::sort(ids.begin(), ids.end());
	ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
	return ids;
}

void writeGaugeSample(std::ostringstream& oss, const std::string& labels, uint32_t value)
{
	oss << "dtc_register_value{" << labels << "} " << value << "\n";
}

void appendRegisterMetrics(std::ostringstream& oss, int deviceIndex, const std::string& deviceType,
						   const std::vector<std::function<DTCLib::RegisterFormatter()>>& functions,
						   std::set<uint16_t>& seenAddresses)
{
	for (const auto& fn : functions)
	{
		try
		{
			auto reg = fn();
			if (!seenAddresses.insert(reg.address).second) continue;

			std::ostringstream addr;
			addr << "0x" << std::hex << reg.address;
			const std::string regName = reg.description.empty() ? addr.str() : reg.description;
			const std::string labels =
				"dtc=\"" + std::to_string(deviceIndex) + "\",device_type=\"" + deviceType +
				"\",address=\"" + addr.str() + "\",register=\"" + escapeLabelValue(regName) + "\"";
			writeGaugeSample(oss, labels, reg.value);
		}
		catch (...)
		{}
	}
}

std::string collectMetrics(const ExporterConfig& config)
{
	std::ostringstream oss;
	oss << "# HELP dtc_register_value Raw register value for Mu2e DTC/CFO devices\n";
	oss << "# TYPE dtc_register_value gauge\n";

	std::vector<int> deviceIds;
	if (config.dtcFilter.has_value())
	{
		deviceIds.push_back(*config.dtcFilter);
	}
	else
	{
		deviceIds = discoverMu2eDeviceIndices();
	}

	for (int deviceIndex : deviceIds)
	{
		try
		{
			{
				DTCLib::DTC_Registers regs(DTCLib::DTC_SimMode_Disabled, deviceIndex, config.memFileName, 0x1, "", true);
				if (!regs.isCFODesignFlavour())
				{
					std::set<uint16_t> seenAddresses;
					appendRegisterMetrics(oss, deviceIndex, "dtc", regs.getFormattedSimpleDumpFunctions(), seenAddresses);
					appendRegisterMetrics(oss, deviceIndex, "dtc", regs.getFormattedDumpFunctions(), seenAddresses);
					appendRegisterMetrics(oss, deviceIndex, "dtc", regs.formattedPerformanceCounterFunctions_, seenAddresses);
					appendRegisterMetrics(oss, deviceIndex, "dtc", regs.formattedSERDESErrorFunctions_, seenAddresses);
					appendRegisterMetrics(oss, deviceIndex, "dtc", regs.formattedPacketCounterFunctions_, seenAddresses);
					continue;
				}
			}

			CFOLib::CFO_Registers cfo(DTCLib::DTC_SimMode_Disabled, deviceIndex, "", true);
			std::set<uint16_t> seenAddresses;
			appendRegisterMetrics(oss, deviceIndex, "cfo", cfo.getFormattedSimpleDumpFunctions(), seenAddresses);
			appendRegisterMetrics(oss, deviceIndex, "cfo", cfo.getFormattedDumpFunctions(), seenAddresses);
			appendRegisterMetrics(oss, deviceIndex, "cfo", cfo.formattedCounterFunctions_, seenAddresses);
		}
		catch (...)
		{}
	}

	return oss.str();
}

void writeAll(int fd, const char* buf, size_t len)
{
	while (len > 0)
	{
		const ssize_t n = send(fd, buf, len, MSG_NOSIGNAL);
		if (n < 0)
		{
			if (errno == EINTR) continue;
			break;
		}
		if (n == 0) break;
		buf += n;
		len -= static_cast<size_t>(n);
	}
}

void handleRequest(int clientFd, const ExporterConfig& config)
{
	char buf[4096] = {};
	ssize_t nread = -1;
	do
	{
		nread = read(clientFd, buf, sizeof(buf) - 1);
	} while (nread < 0 && errno == EINTR);
	if (nread <= 0)
	{
		close(clientFd);
		return;
	}

	const std::string request(buf);
	const auto firstLineEnd = request.find("\r\n");
	if (firstLineEnd == std::string::npos)
	{
		close(clientFd);
		return;
	}
	const std::string requestLine = request.substr(0, firstLineEnd);
	const bool isMetrics = requestLine == "GET /metrics HTTP/1.1" ||
						   requestLine == "GET /metrics HTTP/1.0" ||
						   requestLine == "GET /metrics";

	std::string body;
	std::string statusLine;
	std::string contentType;

	if (isMetrics)
	{
		body = collectMetrics(config);
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
			  << "    -d: Restrict to one DTC/CFO instance (default: scrape all /dev/mu2e* devices)\n"
			  << "    -m: Use <file> as the emulated DTC memory area (default: mu2esim.bin)\n"
			  << "    -p: TCP port to listen on (default: 9100)\n";
	exit(0);
}

int main(int argc, char* argv[])
{
	ExporterConfig config;
	uint16_t port = 9100;
	config.memFileName = "mu2esim.bin";

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
					config.dtcFilter = DTCLib::Utilities::getOptionValue(&optind, &argv);
					break;
				case 'm':
					config.memFileName = DTCLib::Utilities::getOptionString(&optind, &argv);
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

	std::cout << "DTC/CFO Prometheus exporter listening on :" << port << "/metrics";
	if (config.dtcFilter.has_value()) std::cout << " (device " << *config.dtcFilter << ")";
	std::cout << "\n";

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
			handleRequest(clientFd, config);
		}
	}

	close(serverFd);
	std::cout << "DTC/CFO Prometheus exporter stopped.\n";
	return 0;
}
