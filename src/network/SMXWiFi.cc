#include "SMXWiFi.hh"

#include "Connector.hh"
#include "MSXException.hh"
#include "RS232Device.hh"
#include "serialize.hh"

#include <cassert>

namespace openmsx {

static constexpr unsigned baudRates[] = {
	859372, 346520, 231014, 115200, 57600, 38400, 31250, 19200, 9600, 4800
};

SMXWiFi::SMXWiFi(DeviceConfig& config)
	: MSXDevice(config)
	, RS232Connector(MSXDevice::getPluggingController(), "smxwifi")
{
}

SMXWiFi::~SMXWiFi() = default;

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

uint8_t SMXWiFi::readIO(uint16_t port, EmuTime /*time*/)
{
	switch (port & 0x01) {
	case 0: // UART data
		return readFIFO();
	case 1: // UART status
		return readStatus();
	default:
		return 0xFF;
	}
}

uint8_t SMXWiFi::peekIO(uint16_t port, EmuTime /*time*/) const
{
	switch (port & 0x01) {
	case 1: {
		std::scoped_lock lock(fifoMutex);
		uint8_t status = 0;
		if (!fifo.empty()) status |= 0x01;
		status |= 0x08;
		if (underrun) status |= 0x10;
		return status;
	}
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
	dataReady.notify_one();
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

uint8_t SMXWiFi::readFIFO()
{
	std::unique_lock lock(fifoMutex);
	if (!fifo.empty()) {
		uint8_t v = fifo.front();
		fifo.pop_front();
		return v;
	}
	dataReady.wait_for(lock, std::chrono::milliseconds(30));
	if (!fifo.empty()) {
		uint8_t v = fifo.front();
		fifo.pop_front();
		return v;
	}
	underrun = true;
	return 0xFF;
}

uint8_t SMXWiFi::readStatus()
{
	std::scoped_lock lock(fifoMutex);
	uint8_t status = 0;
	if (!fifo.empty()) status |= 0x01;
	status |= 0x08; // Quick receive supported
	if (underrun) {
		status |= 0x10;
		underrun = false;
	}
	return status;
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
