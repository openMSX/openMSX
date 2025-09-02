#pragma once

#include "MSXPiDevice.hh"
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>

namespace openmsx {

MSXPiDevice::MSXPiDevice(const DeviceConfig& config)
    : MSXDevice(config)
{
    running = true;
    worker = std::thread(&MSXPiDevice::serverThread, this);
    std::cout << "[MSXPi] Device initialized\n";

}

MSXPiDevice::~MSXPiDevice()
{
    running = false;
    if (worker.joinable()) worker.join();
    if (sockfd >= 0) close(sockfd);
    std::cout << "[MSXPi] Device destroyed\n";
}

void MSXPiDevice::reset(EmuTime /*time*/)
{
    std::lock_guard<std::mutex> lock(mtx);
    rxQueue = std::queue<byte>();
    txQueue = std::queue<byte>();
    readRequested = false;
    std::cout << "[MSXPi] Device reset\n";
}

byte MSXPiDevice::readIO(uint16_t port, EmuTime /*time*/)
{
	std::cout << "[MSXPi] readIO triggered on port 0x" << std::hex << port << "\n";
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
            std::cout << "[MSXPi] Read STATUS: " << int(status) << "\n";
            return status;
        }

        case 0x5A: // DATA
        {
            if (readRequested && !rxQueue.empty()) {
                byte val = rxQueue.front();
                rxQueue.pop();
                readRequested = false;
                std::cout << "[MSXPi] Read DATA: 0x" << std::hex << int(val) << "\n";
                return val;
            } else {
                std::cout << "[MSXPi] Read DATA: Empty Queue\n";
                return 0xFF; // No data ready
            }
        }

        default:
            return 0xFF;
    }
}

void MSXPiDevice::writeIO(uint16_t port, byte value, EmuTime /*time*/)
{
	std::cout << "[MSXPi] writeIO triggered on port 0x" << std::hex << port << "\n";
    switch (port & 0xFF) {
        case 0x56: // CONTROL
            if (serverAvailable) {
                std::cout << "[MSXPi] Write CONTROL (0x56): prepare next byte for DATA\n";
				std::lock_guard<std::mutex> lock(mtx);
				//if (!readRequested && !rxQueue.empty()) {
				//	// Only flush if a read wasn't already requested
				//	rxQueue.pop(); // discard one stale byte
				//}
				readRequested = true;
            } else {
                std::cout << "[MSXPi] Write CONTROL (0x56) ignored, server not connected\n";
            }
            break;

        case 0x5A: // DATA
            if (serverAvailable && sockfd >= 0) {
                std::lock_guard<std::mutex> lock(mtx);
                txQueue.push(value);
                std::cout << "[MSXPi] Write DATA (0x5A): 0x" << std::hex << int(value)
                          << " queued for sending to Python server\n";
            } else {
                std::cout << "[MSXPi] Write DATA (0x5A) ignored, server not connected\n";
            }
            break;

        default:
            break;
    }
}

byte MSXPiDevice::peekIO(uint16_t port, EmuTime /*time*/) const
{
	std::cout << "[MSXPi] peekIO triggered on port 0x" << std::hex << port << "\n";
    std::lock_guard<std::mutex> lock(mtx);

    switch (port & 0xFF) {
		case 0x56: // STATUS
		{
			if (!serverAvailable) return 0x01;
			if (!rxQueue.empty()) return 0x02;
			return 0x00;
		}

        case 0x5A: // DATA
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
    while (running) {
        if (sockfd < 0) {
            sockfd = socket(AF_INET, SOCK_STREAM, 0);
            if (sockfd < 0) { sleep(1); continue; }

            sockaddr_in addr{};
            addr.sin_family = AF_INET;
            addr.sin_port = htons(serverPort);
            inet_pton(AF_INET, serverIP.c_str(), &addr.sin_addr);

            if (connect(sockfd, (sockaddr*)&addr, sizeof(addr)) == 0) {
                serverAvailable = true;
                std::cout << "[MSXPi] Python server connected at " << serverIP << ":" << serverPort << "\n";
            } else {
                close(sockfd);
                sockfd = -1;
                serverAvailable = false;
                std::cout << "[MSXPi] Failed to connect, retrying...\n";
                sleep(1);
                continue;
            }
        }

        // Send queued DATA bytes
        while (serverAvailable && !txQueue.empty()) {
            byte val;
            {
                std::lock_guard<std::mutex> lock(mtx);
                val = txQueue.front();
                txQueue.pop();
            }
            ssize_t sent = send(sockfd, &val, 1, 0);
            if (sent <= 0) {
                std::cout << "[MSXPi] Connection lost while sending\n";
                close(sockfd);
                sockfd = -1;
                serverAvailable = false;
                break;
            }
            std::cout << "[MSXPi] Sent byte 0x" << std::hex << int(val) << " to Python server\n";
        }

        // Receive data from Python server
        byte buf[64];
        ssize_t rec = recv(sockfd, buf, sizeof(buf), MSG_DONTWAIT);
        if (rec > 0) {
            std::lock_guard<std::mutex> lock(mtx);
            for (ssize_t i = 0; i < rec; i++) {
                rxQueue.push(buf[i]);
                std::cout << "[MSXPi] Received byte 0x" << std::hex << int(buf[i]) << " from Python server\n";
            }
        }

        if (rec == 0) {
            std::cout << "[MSXPi] Python server closed connection\n";
            close(sockfd);
            sockfd = -1;
            serverAvailable = false;
        }

        usleep(1000); // 1 ms
    }

    if (sockfd >= 0) close(sockfd);
    serverAvailable = false;
    std::cout << "[MSXPi] Server thread exiting\n";
}

REGISTER_MSXDEVICE(MSXPiDeviceSimple, "MSXPiDeviceSimple");

} // namespace openmsx
