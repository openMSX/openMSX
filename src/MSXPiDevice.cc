#include "MSXPiDevice.hh"

#include "Timer.hh"
#include "xrange.hh"

#include <algorithm>
#include <array>

namespace openmsx {

MSXPiDevice::MSXPiDevice(const DeviceConfig& config)
	: MSXDevice(config)
{
	thread = std::thread(&MSXPiDevice::readLoop, this);
}

MSXPiDevice::~MSXPiDevice()
{
	// std::cout << "[MSXPi] MSXPiDevice" << std::endl;
	shouldStop = true;
	close();
	if (thread.joinable()) {
		poller.abort();
		thread.join();
	}
}

void MSXPiDevice::close()
{
	auto oldSock = sock.exchange(OPENMSX_INVALID_SOCKET);
	if (oldSock != OPENMSX_INVALID_SOCKET) {
		sock_close(oldSock);
	}
}

void MSXPiDevice::reset(EmuTime /*time*/)
{
	// std::cout << "[MSXPi] reset" << std::endl;
	std::lock_guard lock(mtx);
	rxQueue.clear();
	readRequested = false;
}

byte MSXPiDevice::readIO(uint16_t port, EmuTime time)
{
	// std::cout << "[MSXPi] readIO: port=0x" << std::hex << port & 0xff << std::endl;
	switch (port & 0xff) {
	case 0x56: // status
		return peekIO(port, time);
	case 0x5A: // data
		if (readRequested) {
			readRequested = false;
			std::lock_guard lock(mtx);
			if (!rxQueue.empty()) {
				return rxQueue.pop_front();
			}
		}
		return 0xff; // No data ready
	default:
		return 0xff;
	}
}

byte MSXPiDevice::peekIO(uint16_t port, EmuTime /*time*/) const
{
	// std::cout << "[MSXPi] peekIO: port=0x" << std::hex << port & 0xff << std::endl;
	switch (port & 0xff) {
	case 0x56: // status
		if (sock == OPENMSX_INVALID_SOCKET) {
			return 0x01; // server not available
		}
		{
			std::lock_guard lock(mtx);
			if (!rxQueue.empty()) return 0x02;
		}
		return 0x00;
	case 0x5A: // data
		if (readRequested) {
			std::lock_guard lock(mtx);
			if (!rxQueue.empty()) {
				return rxQueue.front();
			}
		}
		return 0xff;
	default:
		return 0xff;
	}
}

void MSXPiDevice::writeIO(uint16_t port, byte value, EmuTime /*time*/)
{
	// std::cout << "[MSXPi] writeIO: port=0x" << std::hex << port & 0xff << std::endl;
	switch (port & 0xff) {
	case 0x56: // control
		if (sock != OPENMSX_INVALID_SOCKET) {
			readRequested = true;
		}
		break;
	case 0x5A: // data
		if (sock != OPENMSX_INVALID_SOCKET) {
			auto res = sock_send(sock, reinterpret_cast<const char*>(&value), 1);
			(void)res; // ignore error
		}
		break;
	default:
		break;
	}
}

void MSXPiDevice::readLoop()
{
	while (!shouldStop) {
		// std::cout << "[MSXPi] readLoop" << std::endl;
		if (sock == OPENMSX_INVALID_SOCKET) {
			sock = socket(AF_INET, SOCK_STREAM, 0);
			if (sock == OPENMSX_INVALID_SOCKET) {
				Timer::sleep(1'000'000); // retry once per second
				continue;
			}

			sockaddr_in addr{};
			addr.sin_family = AF_INET;
			addr.sin_port = htons(5000);
			addr.sin_addr.s_addr =
			        htonl(INADDR_LOOPBACK); // 127.0.0.1
			if (connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
				close();
				Timer::sleep(1'000'000); // retry once per second
				continue;
			}
		}

#ifndef _WIN32
		if (poller.poll(sock)) {
			continue; // error or abort
		}
#endif
		std::array<char, 64> buf;
		auto n = sock_recv(sock, buf.data(), buf.size());
		if (n < 0) { // error
			close();
			continue;
		}
		std::lock_guard lock(mtx);
		static constexpr size_t MAX_QUEUE_SIZE = 16 * 1024;
		for (auto i : xrange(std::min<size_t>(n, MAX_QUEUE_SIZE - rxQueue.size()))) {
			rxQueue.push_back(buf[i]);
		}
	}
}

REGISTER_MSXDEVICE(MSXPiDevice, "MSXPiDevice");

} // namespace openmsx
