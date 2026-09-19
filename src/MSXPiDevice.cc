#include "MSXPiDevice.hh"

#include "DeviceConfig.hh"
#include "MSXCPU.hh"
#include "MSXMotherBoard.hh"
#include "RealTime.hh"
#include "Timer.hh"

#include <algorithm>
#include <array>
#include <chrono>

namespace openmsx {

// virtual-SPI frame opcodes, see MSXPiDevice.hh
static constexpr byte OP_OFFER  = 0x01;
static constexpr byte OP_HOLD   = 0x02;
static constexpr byte OP_CANCEL = 0x03;
static constexpr byte OP_HELLO  = 0x7E;
static constexpr byte PROTOCOL_VERSION = 1;

static constexpr byte MSXPI_VERSION = 0x0E; // firmware version, port 0x57
static constexpr uint16_t SERVER_PORT = 5000;

// How long the Pi takes to clock one byte, and so how long a wait-mode stall
// lasts. In the order of what the Pi's native GPIO engine needs.
static constexpr auto TRANSFER_TIME = EmuDuration::usec(20);

MSXPiDevice::MSXPiDevice(const DeviceConfig& config)
	: MSXDevice(config)
{
	thread = std::thread(&MSXPiDevice::readLoop, this);
}

MSXPiDevice::~MSXPiDevice()
{
	shouldStop = true;
	closeSocket();
	if (thread.joinable()) {
		poller.abort();
		thread.join();
	}
}

void MSXPiDevice::closeSocket()
{
	auto oldSock = sock.exchange(OPENMSX_INVALID_SOCKET);
	if (oldSock != OPENMSX_INVALID_SOCKET) {
		sock_close(oldSock);
	}
}

// ---------------------------------------------------------------------------
// Interface model
// ---------------------------------------------------------------------------

void MSXPiDevice::interfaceReset()
{
	// What OUT (0x56),0xFF does. The Pi does not see it: an offer it made
	// still waits to be clocked, and a transfer it is already clocking just
	// finishes on its side.
	busy = false;
	bound.reset();
	srValue = 0x00;
	waitMode = false;
}

void MSXPiDevice::powerUp(EmuTime /*time*/)
{
	interfaceReset();
	latch = 0xFF;
}

bool MSXPiDevice::ready() const
{
	// The Pi's READY line.
	if (link != Link::FRAMED) return false; // no Pi: pulled low
	return bound || offerCount != 0; // up while clocking or offering a byte
}

void MSXPiDevice::startTransfer(EmuTime time)
{
	busy = true;
	bound.reset();
	tryBind(time);
}

void MSXPiDevice::tryBind(EmuTime time)
{
	// A started transfer gets clocked as soon as the Pi offers a byte.
	if (!busy || bound || link != Link::FRAMED || offerCount == 0) return;
	std::lock_guard lock(mtx);
	if (offers.empty()) return; // withdrawn by a cancel meanwhile
	bound = offers.front();
	offers.pop_front();
	offerCount = offers.size();
	boundEpoch = epoch;
	// The Pi starts clocking now, and copies the latch as it does.
	startTime = time;
	doneTime = time + TRANSFER_TIME;
	sendFrame(OP_OFFER, latch);
}

void MSXPiDevice::complete()
{
	srValue = bound->miso;
	bound.reset();
	busy = false;
}

void MSXPiDevice::update(EmuTime time)
{
	if (bound && boundEpoch != epoch) {
		// The server went away mid-byte. Nobody clocks this transfer any
		// more: it stays started until reset, or until a new server's first
		// offer picks it up - exactly what the hardware does.
		bound.reset();
	}
	if (busy && !bound) tryBind(time);
	if (bound && time >= doneTime) complete();
}

byte MSXPiDevice::shiftRegister(EmuTime time) const
{
	if (!busy) return srValue;
	// A marker bit is loaded at bit 0 when a transfer starts; the Pi's bits
	// then shift in below it, one per ninth of the transfer time.
	if (!bound || time <= startTime) return 0x01;
	auto elapsed = (time - startTime).toUint64();
	auto total = std::max<uint64_t>(TRANSFER_TIME.toUint64(), 1);
	auto bits = unsigned(std::min<uint64_t>(8, (elapsed * 9) / total));
	unsigned sr = (1u << bits) | (unsigned(bound->miso) >> (8 - bits));
	return byte(sr); // after 8 bits the marker is in bit 8, not visible
}

void MSXPiDevice::waitForPeer(EmuTime time)
{
	// Emulation runs ahead of real time in slices and then sleeps to throttle.
	// A byte the server offers during that sleep would only be seen at the
	// start of the next slice, so every request/response exchange with the
	// server would cost a whole slice of emulated time - far more than the
	// real Pi needs. Instead, spend the time emulation is ahead of real time
	// here, waiting for the offer. That is time RealTime would have slept
	// anyway, so this never makes emulation fall behind real time.
	static constexpr uint64_t STEP_US = 1000;
	static constexpr uint64_t ALLOWED_LAG_US = 20000; // RealTime's own slack
	if (link != Link::FRAMED) return;
	auto& realTime = getMotherBoard().getRealTime();
	std::unique_lock lock(mtx);
	while (offers.empty() && link == Link::FRAMED &&
	       realTime.timeLeft(STEP_US + ALLOWED_LAG_US, time)) {
		offerCv.wait_for(lock, std::chrono::microseconds(STEP_US));
	}
}

EmuTime MSXPiDevice::stall(EmuTime time)
{
	// Wait mode holds /WAIT while a transfer runs and the Pi's READY is up;
	// the stretched I/O cycle ends when the Pi has clocked the byte. Returns
	// the time the I/O cycle ends.
	if (!waitMode || !busy || !bound) return time;
	if (doneTime > time) {
		getCPU().wait(doneTime);
		update(doneTime);
		return doneTime;
	}
	return time;
}

byte MSXPiDevice::readIO(uint16_t port, EmuTime time)
{
	switch (port & 0xff) {
	case 0x56:
		update(time);
		if (!bound && offerCount == 0) {
			// The MSX is polling for a byte the Pi has not offered yet.
			waitForPeer(time);
			update(time);
		}
		return peekIO(port, time);
	case 0x57:
		update(time);
		return peekIO(port, time);
	case 0x5A:
		update(time);
		if (waitMode) {
			// a wait-mode read starts a transfer, but only when the Pi is ready
			if (!busy && ready()) startTransfer(time);
			return shiftRegister(stall(time));
		}
		return shiftRegister(time);
	default:
		return 0xFF;
	}
}

byte MSXPiDevice::peekIO(uint16_t port, EmuTime time) const
{
	switch (port & 0xff) {
	case 0x56: // bit 0: transfer running or Pi not ready
		return (busy || !ready()) ? 0x01 : 0x00;
	case 0x57:
		return byte((waitMode ? 0x80 : 0x00) | MSXPI_VERSION);
	case 0x5A:
		return shiftRegister(time);
	default:
		return 0xFF;
	}
}

void MSXPiDevice::writeIO(uint16_t port, byte value, EmuTime time)
{
	switch (port & 0xff) {
	case 0x56:
		if (value == 0xFF) {
			latch = value;
			interfaceReset();
			break;
		}
		[[fallthrough]];
	case 0x5A:
		update(time);
		latch = value;
		// a write starts a transfer, whether the Pi is ready or not
		if (!busy) startTransfer(time);
		stall(time);
		break;
	case 0x57:
		// only 0x01 sets and only 0x00 clears wait mode
		if (value == 0x01) {
			waitMode = true;
		} else if (value == 0x00) {
			waitMode = false;
		}
		break;
	default:
		break;
	}
}

// ---------------------------------------------------------------------------
// Transport
// ---------------------------------------------------------------------------

void MSXPiDevice::sendFrame(byte op, byte arg)
{
	auto s = sock.load();
	if (s == OPENMSX_INVALID_SOCKET) return;
	std::array<char, 2> frame = {char(op), char(arg)};
	(void)sock_send(s, frame.data(), frame.size());
}

void MSXPiDevice::handleFrame(byte op, byte arg)
{
	switch (op) {
	case OP_HOLD:
		pendingHold.emplace_back(arg, true);
		break;
	case OP_OFFER: {
		// a run of holds becomes visible together with its closing offer
		std::lock_guard lock(mtx);
		offers.insert(offers.end(), pendingHold.begin(), pendingHold.end());
		offers.emplace_back(arg, false);
		offerCount = offers.size();
		pendingHold.clear();
		offerCv.notify_all();
		break;
	}
	case OP_CANCEL: {
		// Withdraw every offer not clocked yet. Replies for clocked offers
		// are sent under this same mutex, so they all precede the
		// acknowledgement on the wire.
		std::lock_guard lock(mtx);
		pendingHold.clear();
		offers.clear();
		offerCount = 0;
		sendFrame(OP_CANCEL, 0);
		break;
	}
	default:
		break; // unknown frame
	}
}

bool MSXPiDevice::connectSocket()
{
	SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
	if (s == OPENMSX_INVALID_SOCKET) return false;
	auto addr = sock_makeIPv4(INADDR_LOOPBACK, SERVER_PORT); // 127.0.0.1
	// Every transfer is a small request/response; Nagle plus the server's
	// delayed ACK would hold each frame back ~40 ms.
	sock_setNoDelay(s);
	if (connect(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
		sock_close(s);
		return false;
	}
	sock = s;
	return true;
}

void MSXPiDevice::readLoop()
{
	std::array<char, 4096> buf;
	while (!shouldStop) {
		if (sock == OPENMSX_INVALID_SOCKET) {
			link = Link::DOWN;
			if (!connectSocket()) {
				Timer::sleep(1'000'000); // retry once per second
				continue;
			}
			{
				std::lock_guard lock(mtx);
				offers.clear();
				offerCount = 0;
			}
			pendingHold.clear();
			partial.reset();
			++epoch;
			link = Link::PROBING; // READY stays low until the server greets
		}

#ifndef _WIN32
		if (poller.poll(sock)) {
			continue; // error or abort
		}
#endif
		auto n = sock_recv(sock, buf.data(), buf.size());
		if (n <= 0) { // closed or error
			closeSocket();
			continue;
		}

		size_t i = 0;
		if (link == Link::PROBING) {
			if (!partial) {
				if (byte(buf[0]) != OP_HELLO) {
					// not a virtual-SPI server: it never offers, so
					// READY stays low, as with no Pi at all
					link = Link::INCOMPATIBLE;
				} else {
					partial = byte(buf[i++]);
				}
			}
			if (partial && i < size_t(n)) {
				partial.reset();
				++i; // protocol version: 1 is the only one so far
				std::lock_guard lock(mtx);
				sendFrame(OP_HELLO, PROTOCOL_VERSION);
				link = Link::FRAMED;
			}
		}
		if (link == Link::FRAMED) {
			for (; i < size_t(n); ++i) {
				auto b = byte(buf[i]);
				if (!partial) {
					partial = b;
				} else {
					handleFrame(*partial, b);
					partial.reset();
				}
			}
		}
	}
}

REGISTER_MSXDEVICE(MSXPiDevice, "MSXPiDevice");

} // namespace openmsx
