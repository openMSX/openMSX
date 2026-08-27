#include "SMXWiFi.hh"

#include "Connector.hh"
#include "EmuDuration.hh"
#include "MSXCliComm.hh"
#include "MSXException.hh"
#include "RS232Device.hh"
#include "serialize.hh"

#include <array>
#include <cassert>

namespace openmsx {

static constexpr auto baudRates = std::to_array<unsigned>({
	859372, 346520, 231014, 115200, 57600, 38400, 31250, 19200, 9600, 4800
});

SMXWiFi::SMXWiFi(DeviceConfig& config)
	: MSXDevice(config)
	, RS232Connector(MSXDevice::getPluggingController(), "smxwifi")
	, rom(MSXDevice::getName() + " ROM", "rom", config)
	, cpu(getCPU())
{
}

SMXWiFi::~SMXWiFi() = default;

// ====================================================================
//  Memory-mapped ROM
// ====================================================================

uint8_t SMXWiFi::readMem(uint16_t address, EmuTime /*time*/)
{
	if (0x4000 <= address && address < 0x8000) {
		return rom[address & 0x3FFF];
	}
	return 0xFF;
}

const uint8_t* SMXWiFi::getReadCacheLine(uint16_t start) const
{
	if (0x4000 <= start && start < 0x8000) {
		return &rom[start & 0x3FFF];
	}
	return unmappedRead.data();
}

void SMXWiFi::powerUp(EmuTime time)
{
	reset(time);
}

void SMXWiFi::reset(EmuTime time)
{
	uartSpeed = 0; // 859372
	resetFIFO();
	underrun = false;
	auto& dev = getPluggedRS232Dev();
	dev.setBaudRate(baudRates[uartSpeed]);
	dev.setDataBits(DataBits::D8);
	dev.setStopBits(StopBits::S1);
	dev.setParityBit(false, Parity::EVEN);
	dev.setDTR(false, time);
	dev.setRTS(false, time);
}

uint8_t SMXWiFi::readIO(uint16_t port, EmuTime time)
{
	switch (port & 0x01) {
	case 0: // UART data
		return readFIFO(time);
	case 1: // UART status
		return readStatus();
	default:
		return 0xFF;
	}
}

uint8_t SMXWiFi::peekIO(uint16_t port, EmuTime /*time*/) const
{
	switch (port & 0x01) {
	case 0: // UART data
		return peekFIFO();
	case 1: // UART status
		return peekStatus();
	default:
		return 0xFF;
	}
}

void SMXWiFi::writeIO(uint16_t port, uint8_t value, EmuTime time)
{
	switch (port & 0x01) {
	case 0: // UART command
		writeCommand(value);
		break;
	case 1: // UART data
		getPluggedRS232Dev().recvByte(value, time);
		break;
	}
}

void SMXWiFi::plug(Pluggable& device, EmuTime time)
{
	Connector::plug(device, time);
	auto& dev = getPluggedRS232Dev();
	dev.setBaudRate(baudRates[uartSpeed]);
	dev.setDataBits(DataBits::D8);
	dev.setStopBits(StopBits::S1);
	dev.setParityBit(false, Parity::EVEN);
	dev.setDTR(false, time);
	dev.setRTS(false, time);
}

void SMXWiFi::setDataBits(DataBits /*bits*/)
{
}

void SMXWiFi::setStopBits(StopBits /*bits*/)
{
}

void SMXWiFi::setParityBit(bool /*enable*/, Parity /*parity*/)
{
}

void SMXWiFi::recvByte(uint8_t value, EmuTime /*time*/)
{
	std::scoped_lock lock(fifoMutex);
	fifo.push_back(value);
}

bool SMXWiFi::ready()
{
	return true;
}

bool SMXWiFi::acceptsData()
{
	return true;
}

bool SMXWiFi::directByteDelivery() const
{
	return true;
}

// Emulated time to stall the CPU (in slices) before declaring a FIFO
// underrun, and the slice size while waiting.
//
// This mirrors the real SM-X WiFi hardware: on an IN of the data port
// while the UART FIFO is empty, the device asserts WAIT, stalling the
// Z80 until a byte arrives or a timeout (25 ms on the real device)
// elapses, after which 0xFF is returned and the underrun bit is set.
// The driver (ESPUNAPI.asm) reads the response header status-guarded and
// then fires a blind INI/INIR for the announced payload size, which
// always outruns the UART stream; it detects the resulting underrun
// (status bit 4) after the loop and recovers via the 'r' retry command,
// which re-sends the whole response. So underruns are harmless by design
// and never corrupt the received data.
//
// The stall is done with cpu.wait() in small emulated-time slices: only
// the CPU is stopped, the rest of the machine (VDP, sound, timers, ...)
// keeps running, so other devices' emulation is not affected. The slice
// must be no larger than the fastest UART char time (11.6 us @ 859372
// baud) because cpu.wait() cannot be woken early: an arriving byte is
// only noticed at the next slice boundary.
//
// FIFO_TIMEOUT is 50 ms: more than the real device's 25 ms, because in
// the emulated setup the producer chain goes through a host USB-serial
// converter (adapter latency timer, driver buffering and packing),
// which adds gaps the real device (direct 3.3 V serial, no converter)
// does not have. 50 ms was found to cover that extra latency and keep
// the driver's retry path quiet during normal operation.
static constexpr auto FIFO_TIMEOUT = EmuDuration::msec(50);
static constexpr auto FIFO_POLL = EmuDuration::usec(12);

uint8_t SMXWiFi::readFIFO(EmuTime time)
{
	// Pop one byte if available (mutex-protected).
	auto tryPop = [this](uint8_t& v) {
		std::scoped_lock lock(fifoMutex);
		if (fifo.empty()) return false;
		v = fifo.front();
		fifo.pop_front();
		return true;
	};
	uint8_t v;
	if (tryPop(v)) return v;

	// FIFO empty: give the producer (a host thread, e.g. a real ESP over
	// the serial port) a short chance to provide data by stalling only the
	// CPU in emulated-time slices — the rest of the machine keeps running.
	auto deadline = time + FIFO_TIMEOUT;
	while (time < deadline) {
		cpu.wait(time + FIFO_POLL);
		if (tryPop(v)) return v;
		time = getCurrentTime();
	}

	underrun = true;
	getCliComm().printInfo(
		"SMXWiFi: FIFO underrun: no data received within 50 ms");
	return 0xFF;
}

uint8_t SMXWiFi::peekFIFO() const
{
	std::scoped_lock lock(fifoMutex);
	return fifo.empty() ? 0xFF : fifo.front();
}

uint8_t SMXWiFi::readStatus()
{
	std::scoped_lock lock(fifoMutex);
	uint8_t status = peekStatusLocked();
	underrun = false;
	return status;
}

uint8_t SMXWiFi::peekStatusLocked() const
{
	uint8_t status = 0;
	if (!fifo.empty()) status |= 0x01;
	status |= 0x08; // Quick receive supported
	if (underrun) status |= 0x10;
	return status;
}

uint8_t SMXWiFi::peekStatus() const
{
	std::scoped_lock lock(fifoMutex);
	return peekStatusLocked();
}

void SMXWiFi::writeCommand(uint8_t value)
{
	if (value <= 9) {
		uartSpeed = value;
		getPluggedRS232Dev().setBaudRate(baudRates[value]);
	} else if (value == 20) {
		resetFIFO();
	}
}

void SMXWiFi::resetFIFO()
{
	std::scoped_lock lock(fifoMutex);
	fifo.clear();
	underrun = false;
}

template<typename Archive>
void SMXWiFi::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
	ar.template serializeBase<RS232Connector>(*this);
	ar.serialize("uartSpeed", uartSpeed);
	if constexpr (Archive::IS_LOADER) {
		resetFIFO();
		auto& dev = getPluggedRS232Dev();
		dev.setBaudRate(baudRates[uartSpeed]);
		dev.setDataBits(DataBits::D8);
		dev.setStopBits(StopBits::S1);
		dev.setParityBit(false, Parity::EVEN);
		dev.setDTR(false, EmuTime::zero());
		dev.setRTS(false, EmuTime::zero());
	}
}
INSTANTIATE_SERIALIZE_METHODS(SMXWiFi);
REGISTER_MSXDEVICE(SMXWiFi, "SMXWiFi");

} // namespace openmsx
