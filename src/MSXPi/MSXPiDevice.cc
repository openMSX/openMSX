#include "MSXPi/MSXPiDevice.hh"
#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #pragma comment(lib, "ws2_32.lib")
#else
  #include <arpa/inet.h>
  #include <unistd.h>
#endif

#include <iostream>
#include <thread>
#include <mutex>
#include <queue>

namespace openmsx {

MSXPiDevice::MSXPiDevice(const DeviceConfig& config)
    : MSXDevice(config)
{
    running = true;
    worker = std::thread(&MSXPiDevice::serverThread, this);
    // std::cout << "[MSXPi] Device initialized\n";

}

MSXPiDevice::~MSXPiDevice()
{
    running = false;
    if (worker.joinable()) worker.join();
#ifdef _WIN32
    if (sockfd >= 0) closesocket(sockfd);
#else
    if (sockfd >= 0) close(sockfd);
#endif
}

void MSXPiDevice::reset(EmuTime /*time*/)
{
    std::lock_guard<std::mutex> lock(mtx);
    rxQueue = std::queue<byte>();
    txQueue = std::queue<byte>();
    readRequested = false;
    // std::cout << "[MSXPi] Device reset\n";
}

byte MSXPiDevice::readIO(uint16_t port, EmuTime /*time*/)
{
	// std::cout << "[MSXPi] readIO triggered on port 0x" << std::hex << port << "\n";
    std::lock_guard<std::mutex> lock(mtx);
    switch (port & 0xFF) {
        case 0x56: // STATUS
        {
            byte status;
            if (!serverAvailable) {
                status = 0x01; // Offline
            } else if (!rxQueue.empty()) {
                status = 0x02; // Data available
            } else {
                status = 0x00; // Online, no data
            }
            // std::cout << "[MSXPi] Read STATUS: " << int(status) << "\n";
            return status;
        }

        case 0x57: // DATA
        {
            if (readRequested && !rxQueue.empty()) {
                byte val = rxQueue.front();
                rxQueue.pop();
                readRequested = false;
                // std::cout << "[MSXPi] Read DATA: 0x" << std::hex << int(val) << "\n";
                return val;
            } else {
                // std::cout << "[MSXPi] Read DATA: Empty Queue\n";
                return 0xFF; // No data ready
            }
        }

        default:
            return 0xFF;
    }
}

void MSXPiDevice::writeIO(uint16_t port, byte value, EmuTime /*time*/)
{
	// std::cout << "[MSXPi] writeIO triggered on port 0x" << std::hex << port << "\n";
    switch (port & 0xFF) {
        case 0x56: // CONTROL
            if (serverAvailable) {
                // std::cout << "[MSXPi] Write CONTROL (0x56): prepare next byte for DATA\n";
				std::lock_guard<std::mutex> lock(mtx);
				//if (!readRequested && !rxQueue.empty()) {
				//	// Only flush if a read wasn't already requested
				//	rxQueue.pop(); // discard one stale byte
				//}
				readRequested = true;
            } else {
                // std::cout << "[MSXPi] Write CONTROL (0x56) ignored, server not connected\n";
            }
            break;

        case 0x57: // DATA
			if (serverAvailable && sockfd >= 0) {
				send(sockfd, reinterpret_cast<const char*>(&value), 1, 0);
			}
            break;
			
        default:
            break;
    }
}

byte MSXPiDevice::peekIO(uint16_t port, EmuTime /*time*/) const
{
	// std::cout << "[MSXPi] peekIO triggered on port 0x" << std::hex << port << "\n";
    std::lock_guard<std::mutex> lock(mtx);

    switch (port & 0xFF) {
		case 0x56: // STATUS
		{
			if (!serverAvailable) return 0x01;
			if (!rxQueue.empty()) return 0x02;
			return 0x00;
		}

        case 0x57: // DATA
            if (readRequested && !rxQueue.empty()) {
                return rxQueue.front();
            } else {
                return 0xFF;
            }

        default:
            return 0xFF;
    }
}

void MSXPiDevice::serverThread()
{
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        // std::cerr << "[MSXPi] WSAStartup failed\n";
        return;
    }
#endif

    // std::cout << "[MSXPi] Server thread started\n";

    while (running) {
        if (sockfd < 0) {
            // std::cout << "[MSXPi] Attempting to create socket...\n";
            sockfd = socket(AF_INET, SOCK_STREAM, 0);
            if (sockfd < 0) {
                // std::cerr << "[MSXPi] Failed to create socket\n";
#ifdef _WIN32
                Sleep(1000);
#else
                sleep(1);
#endif
                continue;
            }

            sockaddr_in addr{};
            addr.sin_family = AF_INET;
            addr.sin_port = htons(serverPort);
            inet_pton(AF_INET, serverIP.c_str(), &addr.sin_addr);

            std::cout << "[MSXPi] Attempting to connect to " << serverIP << ":" << serverPort << "...\n";
            if (connect(sockfd, (sockaddr*)&addr, sizeof(addr)) == 0) {
                serverAvailable = true;
                // std::cout << "[MSXPi] Connected to MSXPi Server server\n";
            } else {
                // std::cerr << "[MSXPi] Connection failed\n";
#ifdef _WIN32
                closesocket(sockfd);
#else
                close(sockfd);
#endif
                sockfd = -1;
                serverAvailable = false;
#ifdef _WIN32
                Sleep(1000);
#else
                sleep(1);
#endif
                continue;
            }
        }

		fd_set readfds;
		FD_ZERO(&readfds);
		FD_SET(sockfd, &readfds);
		
		timeval timeout{};
		timeout.tv_sec = 0;
		timeout.tv_usec = 500000; // 0.5 seconds
		
		int activity = select(sockfd + 1, &readfds, nullptr, nullptr, &timeout);
		if (activity > 0 && FD_ISSET(sockfd, &readfds)) {
			byte buf[64];
			ssize_t rec = recv(sockfd, reinterpret_cast<char*>(buf), sizeof(buf), 0);
			if (rec > 0) {
				std::lock_guard<std::mutex> lock(mtx);
				for (ssize_t i = 0; i < rec; ++i) {
					rxQueue.push(buf[i]);
					// std::cout << "[MSXPi] Queued byte: 0x" << std::hex << int(buf[i]) << "\n";
				}
			} else {
				// std::cerr << "[MSXPi] Connection closed or error\n";
		#ifdef _WIN32
				closesocket(sockfd);
		#else
				close(sockfd);
		#endif
				sockfd = -1;
				serverAvailable = false;
			}
		}
    }

#ifdef _WIN32
    WSACleanup();
#endif

    // std::cout << "[MSXPi] Server thread exiting\n";
}

REGISTER_MSXDEVICE(MSXPiDevice, "MSXPiDevice");

} // namespace openmsx
