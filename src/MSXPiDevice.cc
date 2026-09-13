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

static constexpr byte MSXPI_VERSION = 0x0E; // MSXPIVer "1110"
static constexpr uint16_t SERVER_PORT = 5000;

MSXPiDevice::MSXPiDevice(const DeviceConfig& config)
	: MSXDevice(config)
	, transferTime(EmuDuration::usec(config.getChildDataAsInt("transfer_time", 20)))
	, readyTail(EmuDuration::usec(config.getChildDataAsInt("ready_tail", 0)))
	, readyGap(EmuDuration::usec(config.getChildDataAsInt("ready_gap", 0)))
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
// CPLD model
// ---------------------------------------------------------------------------

void MSXPiDevice::cpldReset()
{
	// RESET (OUT ($56),$FF): SPI_en_s <= '0', pi_sr <= 0, wait_mode <= '0'.
	// The Pi does not see it: an offer it made still waits for SPI_CS, and a
	// transfer it already clocks just finishes on its side.
	busy = false;
	bound.reset();
	srValue = 0x00;
	waitMode = false;
}

void MSXPiDevice::reset(EmuTime /*time*/)
{
	// The CPLD does not see the MSX reset line, but the device registers
	// clear at power-up, and a machine reset in openMSX must not leave a
	// stretched transfer behind.
	cpldReset();
	latch = 0xFF;
	haveLast = false;
}

bool MSXPiDevice::ready(EmuTime time) const
{
	// SPI_RDY as the Pi drives it
	if (link != Link::FRAMED) return false; // pull-down R8: no Pi
	if (bound) return true;             // up until E10
	if (inReleaseTail(time)) return true;
	// an offer is waiting for SPI_CS, unless the Pi is still between bytes
	return offerCount != 0 && !inReleaseGap(time);
}

bool MSXPiDevice::inReleaseTail(EmuTime time) const
{
	// E10 of a releasing offer until the Pi drops READY
	return haveLast && !lastHold && time < lastDone + readyTail;
}

bool MSXPiDevice::inReleaseGap(EmuTime time) const
{
	// After a releasing offer READY stays low this long (after the tail)
	// before the Pi can offer the next byte, even when that offer is
	// already queued here.
	return haveLast && !lastHold && time < lastDone + readyTail + readyGap;
}

void MSXPiDevice::startTransfer(EmuTime time)
{
	// SPI_en_s <= '1', pi_sr <= "0000000001", SPI_CS low
	busy = true;
	bound.reset();
	tryBind(time);
}

void MSXPiDevice::tryBind(EmuTime time)
{
	if (!busy || bound || link != Link::FRAMED || offerCount == 0) return;
	if (inReleaseGap(time)) return; // the Pi has not raised READY again yet
	std::lock_guard lock(mtx);
	if (offers.empty()) return; // withdrawn by a cancel meanwhile
	bound = offers.front();
	offers.pop_front();
	offerCount = offers.size();
	boundEpoch = epoch;
	// E1: the Pi starts clocking and samples the write latch
	startTime = time;
	doneTime = time + transferTime;
	sendFrame(OP_OFFER, latch);
}

void MSXPiDevice::complete()
{
	// E10: SPI_en_s <= '0'; pi_sr(7 downto 0) holds the received byte
	srValue = bound->miso;
	lastHold = bound->hold;
	lastDone = doneTime;
	haveLast = true;
	bound.reset();
	busy = false;
}

void MSXPiDevice::update(EmuTime time)
{
	if (bound && boundEpoch != epoch) {
		// The server went away mid-byte. Nobody clocks this transfer any
		// more: it stays armed until RESET, or until a new server's first
		// offer picks it up - exactly what the hardware does.
		bound.reset();
	}
	if (busy && !bound) tryBind(time);
	if (bound && time >= doneTime) complete();
}

byte MSXPiDevice::shiftRegister(EmuTime time) const
{
	if (!busy) return srValue;
	if (!bound || time <= startTime) return 0x01; // sentinel only
	// E2..E9 shift MISO in below the sentinel, one bit per ninth of the
	// transfer (E10 does not shift)
	auto elapsed = (time - startTime).toUint64();
	auto total = std::max<uint64_t>(transferTime.toUint64(), 1);
	auto bits = unsigned(std::min<uint64_t>(8, (elapsed * 9) / total));
	unsigned sr = (1u << bits) | (unsigned(bound->miso) >> (8 - bits));
	return byte(sr); // after 8 bits the sentinel sits in bit 8, not visible
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
	// wait_assert <= wait_mode and SPI_RDY and spi_en and
	//                (SPI_en_s or not started)
	// The stretched cycle ends at E10 of a transfer the Pi clocks, or when
	// READY drops under a transfer it never took.
	if (!waitMode || !busy || !ready(time)) return time;
	EmuTime until = time;
	if (bound) {
		until = doneTime;
	} else if (inReleaseTail(time)) {
		until = lastDone + readyTail;
	}
	if (until > time) {
		getCPU().wait(until);
		update(until);
	}
	return until;
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
			// spi_en read term: wait_mode and readoper and SPI_RDY
			if (!busy && ready(time)) startTransfer(time);
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
	case 0x56: // "0000000" & (SPI_en_s or not SPI_RDY)
		return (busy || !ready(time)) ? 0x01 : 0x00;
	case 0x57: // wait_mode & '0' & "00" & MSXPIVer
		return byte((waitMode ? 0x80 : 0x00) | MSXPI_VERSION);
	case 0x5A: // pi_sr(7 downto 0)
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
			cpldReset();
			break;
		}
		[[fallthrough]];
	case 0x5A:
		update(time);
		latch = value; // D_buff_msx follows D on every $56/$5A write
		// spi_en write term: a write starts a transfer, READY or not
		if (!busy) startTransfer(time);
		(void)stall(time);
		break;
	case 0x57:
		// mode register: only $01 sets and only $00 clears it
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
		pendingHold.push_back({arg, true});
		break;
	case OP_OFFER: {
		// a run of holds becomes visible together with its closing offer
		std::lock_guard lock(mtx);
		for (const auto& o : pendingHold) offers.push_back(o);
		offers.push_back({arg, false});
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
