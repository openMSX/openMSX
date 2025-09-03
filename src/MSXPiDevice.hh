#ifndef MSXPIDEVICE_HH
#define MSXPIDEVICE_HH

#include "MSXDevice.hh"
#include "Poller.hh"
#include "Socket.hh"

#include "circular_buffer.hh"

#include <atomic>
#include <mutex>
#include <thread>

namespace openmsx {

class DeviceConfig;

class MSXPiDevice final : public MSXDevice
{
public:
	explicit MSXPiDevice(const DeviceConfig& config);
	~MSXPiDevice() override;

	void reset(EmuTime time) override;
	byte readIO(uint16_t port, EmuTime time) override;
	byte peekIO(uint16_t port, EmuTime time) const override;
	void writeIO(uint16_t port, byte value, EmuTime time) override;

private:
	void close();
	void readLoop();

	// Thread & connection
	SocketActivator socketActivator; // ensure windows sockets are initialized
	std::thread thread;
	Poller poller; // to abort read-thread in a portable way
	std::atomic<SOCKET> sock = OPENMSX_INVALID_SOCKET;
	std::atomic<bool> shouldStop = false;

	// Queues & synchronization
	mutable std::mutex mtx;
	cb_queue<byte> rxQueue;

	// MSXPi logic
	bool readRequested = false;
};

} // namespace openmsx

#endif
