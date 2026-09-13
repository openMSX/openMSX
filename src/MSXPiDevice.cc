#include "MSXPiDevice.hh"
#include "MSXCPU.hh"
#include "Timer.hh"
#include <chrono>
#ifndef _WIN32
#include <netinet/tcp.h> // TCP_NODELAY (winsock2.h provides it on Windows)
#endif
#include "xrange.hh"
#include <algorithm>
#include <vector>

namespace openmsx {

MSXPiDevice::MSXPiDevice(const DeviceConfig& config)
	: MSXDevice(config)
	, waitCycles(unsigned(config.getChildDataAsInt("wait_cycles", 0)))
	, rdyFailEvery(unsigned(config.getChildDataAsInt("rdy_fail_every", 0)))
{
	thread = std::thread(&MSXPiDevice::readLoop, this);
	reset(EmuTime::dummy());
}

MSXPiDevice::~MSXPiDevice()
{
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
	std::lock_guard lock(mtx);
	rxQueue.clear();
	readRequested = false;
	waitMode = false;
}

byte MSXPiDevice::readIO(uint16_t port, EmuTime time)
{
	switch (port & 0xff) {
	case 0x56: // Status
	case 0x57: // Version
		return peekIO(port, time);
	case 0x5A: // Data
		if (waitMode) {
			// Hardware /WAIT: the read itself starts the transfer and
			// stalls the Z80 until the byte is available.  Blocking the
			// emulation thread here IS the stall - that is exactly what
			// the real CPLD does to the CPU.
			//
			// The wait is bounded so that a dead or absent server cannot
			// freeze openMSX.  Real hardware degrades the same way: with
			// SPI_RDY low the CPLD never starts a transfer, never
			// asserts /WAIT, and the read returns a stale byte.
			// NOT named WAIT_TIMEOUT: that is a Win32 macro (258L from
			// winbase.h) and the name would silently expand to a constant.
			// Charge the guest for the stall.  This is the idiomatic
			// openMSX way to model wait states (MSXCPU::waitCyclesZ80,
			// as used by VDP.cc and TurboRFDC.cc) and it advances
			// EMULATED time, which blocking this thread does not.
			//
			// It cannot replace the block below: waitCycles takes a
			// count known in advance, whereas the byte itself arrives
			// from a socket whenever the server gets round to it.
			if (waitCycles > 0) {
				time = getCPU().waitCyclesZ80(time, waitCycles);
			}

			// Fault injection: emulate RPI_READY being low for this read.
			if (rdyFailEvery && (++rdyCounter % rdyFailEvery) == 0) {
				return 0xff; // stale bus, exactly as hardware does
			}

			static constexpr auto RX_STALL_TIMEOUT = std::chrono::milliseconds(250);
			std::unique_lock lock(mtx);
			readRequested = false;
			if (rxQueue.empty()) {
				rxCv.wait_for(lock, RX_STALL_TIMEOUT, [&] {
					return !rxQueue.empty() || shouldStop.load();
				});
			}
			if (!rxQueue.empty()) {
				return rxQueue.pop_front();
			}
			return 0xff;
		}
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
	switch (port & 0xff) {
	case 0x56: // status
		if (sock == OPENMSX_INVALID_SOCKET) {
			return 0x01; // server not available
		}
		{
			std::lock_guard lock(mtx);
			if (!rxQueue.empty()) return 0x02; // byte available
		}
		return 0x00;
	case 0x57: // Version + wait-mode read-back
		// This port has to answer TWO questions at once: "is wait mode on?"
		// and "am I openMSX or real hardware?".  msxpi_bios.asm:97 asks the
		// second one on EVERY BYTE:
		//
		//     in a,($57) / cp $FE / jr c,physical_path
		//
		// so anything below $FE means "real hardware" to every existing
		// binary.  Real CPLD v1.6 answers $0E and $8E; returning $8E here
		// would therefore make stock MSXPi code take the physical-hardware
		// path under emulation and read stale bytes.
		//
		// Hence $FE / $FF rather than $0E / $8E: both stay at or above $FE,
		// so openMSX keeps identifying itself correctly in both states,
		// while bit 0 still reports the mode.  Real hardware can never
		// collide with these - the CPLD pins bit 6 low, capping $57 at $BF.
		return waitMode ? 0xFF : 0xFE;
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

void MSXPiDevice::writeIO(uint16_t port, byte value, EmuTime time)
{
	switch (port & 0xff) {
	case 0x56: // control
		if (value == 0xFF) {
			reset(time); // also clears waitMode, as the CPLD does
			break;
		}
		if (sock != OPENMSX_INVALID_SOCKET) {
			readRequested = true;
		}
		break;
	case 0x57: // wait-mode register (CPLD v1.6)
		// Only $01 sets it and only $00 clears it; every other value is a
		// no-op, matching the CPLD's mode_reg process.
		if (value == 0x01) {
			std::lock_guard lock(mtx);
			waitMode = true;
		} else if (value == 0x00) {
			std::lock_guard lock(mtx);
			waitMode = false;
		}
		break;
	case 0x5A: // data
		// A write stalls the Z80 on real hardware exactly as a read does
		// (the CPLD asserts /WAIT on spi_en, which a write also sets).
		if (waitMode && waitCycles > 0) {
			time = getCPU().waitCyclesZ80(time, waitCycles);
		}
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
	// 64 KB: at or above the usual socket receive buffer, so a burst is taken
	// in about one syscall.  Allocated once here rather than per iteration,
	// and on the heap rather than the stack - a buffer this size is not
	// something to put on a thread stack.
	//
	// MAX_QUEUE_SIZE is deliberately far larger than any single block: the
	// queue only ever grows to what is actually used (cb_queue starts at zero
	// capacity and doubles), so the ceiling costs nothing until a transfer
	// needs it.  Now that nothing is discarded it is a throughput knob rather
	// than a correctness limit: too small merely stalls, it cannot lose data.
	static constexpr size_t MAX_QUEUE_SIZE = 64 * 1024;
	std::vector<char> buf(64 * 1024);

	while (!shouldStop) {
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
			// Every OUT to $5A is sent as its own one-byte write. With
			// Nagle enabled those are held back until the previous one is
			// acknowledged, and the server's delayed ACK makes that ~40 ms
			// EACH: a 512-byte /WAIT burst write then takes minutes instead
			// of milliseconds, which looks exactly like a hung transfer.
			// The protocol is request/response, so there is nothing to
			// coalesce anyway.
			{
				int one = 1;
				setsockopt(sock, IPPROTO_TCP, TCP_NODELAY,
				           reinterpret_cast<const char*>(&one), sizeof(one));
			}
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
		// Check for room BEFORE reading, and read only that much, rather than
		// reading blindly and discarding what will not fit.  The old code capped the queue at 16 KB and threw
		// the rest away - "skip excess bytes" - which meant the device silently
		// lied about what it had received: the server had sent the bytes, the
		// MSX never saw them, and nothing anywhere reported an error.  A block
		// of 16384 plus its 4-byte header and checksum came to 16389, five over
		// the cap, so every large pcopy download lost its tail - including the
		// checksum byte the MSX then waited for for ever.
		//
		// Not reading is all that is needed: the socket buffer fills, TCP
		// closes its window, and the server's sendall() blocks until the MSX
		// catches up.  That is also how real hardware behaves, where the GPIO
		// transport is synchronous and the server can never run ahead.
		//
		size_t room;
		{
			std::lock_guard lock(mtx);
			room = MAX_QUEUE_SIZE - std::min(rxQueue.size(), MAX_QUEUE_SIZE);
		}
		if (room == 0) {
			// Full: let the MSX drain and look again.  A short sleep rather
			// than a condition variable signalled from readIO, because that
			// signal would fire on EVERY byte the MSX takes - and precisely
			// when it matters, with the queue full, each one would wake this
			// thread to read a single byte.  A syscall per byte is the
			// opposite of what the back-pressure is for.  The MSX drains at a
			// few hundred bytes a second, so polling costs nothing and the
			// queue only fills in the first place if the server has run far
			// ahead.
			Timer::sleep(1'000); // 1 ms
			continue;
		}

		auto n = sock_recv(sock, buf.data(), std::min(buf.size(), room));
		if (n < 0) { // error
			close();
			continue;
		}
		// Hand the bytes over in small batches rather than under one lock.
		// The emulated Z80 takes this same mutex on EVERY status poll, so a
		// reader holding it across tens of thousands of push_back calls - plus
		// cb_queue's doubling reallocations, which move the whole buffer -
		// stalls the CPU thread for exactly that long.  Emulation is throttled
		// to 3.58 MHz, so every cycle lost waiting on the lock is real time the
		// transfer never gets back.
		//
		// The socket read stays large: syscalls are worth batching, holding a
		// mutex is not.
		static constexpr size_t BATCH = 512;
		for (size_t off = 0; off < size_t(n); off += BATCH) {
			auto end = std::min(off + BATCH, size_t(n));
			std::lock_guard lock(mtx);
			for (auto i : xrange(off, end)) {
				rxQueue.push_back(buf[i]);
			}
			rxCv.notify_one(); // release a wait-mode read blocked in readIO
		}
	}
}

REGISTER_MSXDEVICE(MSXPiDevice, "MSXPiDevice");

} // namespace openmsx
