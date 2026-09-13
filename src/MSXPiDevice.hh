#ifndef MSXPIDEVICE_HH
#define MSXPIDEVICE_HH

#include "MSXDevice.hh"
#include "Poller.hh"
#include "Socket.hh"

#include "circular_buffer.hh"

#include <atomic>
#include <condition_variable>
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
	std::condition_variable rxCv; // wakes a wait-mode read when a byte lands
	cb_queue<byte> rxQueue;

	// MSXPi logic
	bool readRequested = false;

	// Hardware /WAIT flow control, matching CPLD v1.6.  Off at power-up, set
	// by OUT ($57),$01 and cleared by OUT ($57),$00 or the $56 reset.  While
	// it is on, an IN ($5A) both requests a byte and stalls the Z80 until it
	// arrives, which is what lets a driver use INIR/OTIR.
	bool waitMode = false;

	// Emulated cycles charged to the Z80 for each byte moved while wait mode
	// is on, via MSXCPU::waitCyclesZ80 - the same mechanism VDP.cc uses for
	// its fixed T9769 I/O delay.  Blocking this thread until the byte arrives
	// (see readIO) makes the DATA correct, but on its own the guest sees no
	// delay at all; this is what makes the emulated machine actually
	// experience the /WAIT stall.
	//
	// Default 0: the real per-byte stall has not been measured on hardware
	// yet, and inventing a number would make timing results look meaningful
	// when they are not.  Set <wait_cycles> in the extension XML once it has.
	unsigned waitCycles = 0;

	// Fault injection: make every Nth wait-mode read behave as it does on
	// real hardware when RPI_READY happens to be low - return whatever is on
	// the bus instead of stalling.  0 disables it.
	//
	// This exists because the emulation cannot otherwise produce that case at
	// all: a wait-mode read here always blocks until a byte arrives, so there
	// is no window for a stale read.  On real hardware the Pi drops RDY
	// between bytes while it runs its Python dispatch, and a read landing in
	// that gap silently returns garbage - which is how /WAIT passed in
	// emulation while being broken on hardware, and later how a transaction
	// failing once in ~1500 calls went unnoticed until it killed the link.
	unsigned rdyFailEvery = 0;
	unsigned rdyCounter = 0;

};

} // namespace openmsx

#endif
