#include "MSXPrinterPort.hh"
#include "DummyPrinterPortDevice.hh"
#include "checked_cast.hh"
#include "narrow.hh"
#include "outer.hh"
#include "serialize.hh"
#include "unreachable.hh"
#include <cstdint>
#include <memory>

namespace openmsx {

MSXPrinterPort::MSXPrinterPort(const DeviceConfig& config)
	: MSXDevice(config)
	, Connector(MSXDevice::getPluggingController(), "printerport",
	            std::make_unique<DummyPrinterPortDevice>())
	, debuggable(getMotherBoard(), MSXDevice::getName())
        , writePortMask(config.getChildDataAsBool("bidirectional", false) ? 0x03 : 0x01)
        , readPortMask(config.getChildDataAsBool("status_readable_on_all_ports", false) ? 0 : writePortMask)
	, unusedBits(uint8_t(config.getChildDataAsInt("unused_bits", 0xFF))) // TODO: some machines use the port number as unused bits (e.g. lintweaker's Sanyo PHC-23)
{
	reset(getCurrentTime());
}

void MSXPrinterPort::reset(EmuTime::param time)
{
	writeData(0, time);    // TODO check this
	setStrobe(true, time); // TODO check this
}

uint8_t MSXPrinterPort::readIO(uint16_t port, EmuTime::param time)
{
	return peekIO(port, time);
}

uint8_t MSXPrinterPort::peekIO(uint16_t port, EmuTime::param time) const
{
	bool showStatus = (port & readPortMask) == 0;
	if (!showStatus) return 0xFF;
	// bit 1 = status / other bits depend on something unknown, specified
	// in the XML file
	return getPluggedPrintDev().getStatus(time)
		       ? (unusedBits | 0b10) : (unusedBits & ~0b10);
}

void MSXPrinterPort::writeIO(uint16_t port, uint8_t value, EmuTime::param time)
{
	switch (port & writePortMask) {
	case 0:
		setStrobe(value & 1, time); // bit 0 = strobe
		break;
	case 1:
		writeData(value, time);
		break;
	case 2: // nothing here
		break;
	case 3: // 0x93 PDIR (BiDi) is not implemented.
		break;
	default:
		UNREACHABLE;
	}
}

void MSXPrinterPort::setStrobe(bool newStrobe, EmuTime::param time)
{
	if (newStrobe != strobe) {
		strobe = newStrobe;
		getPluggedPrintDev().setStrobe(strobe, time);
	}
}
void MSXPrinterPort::writeData(uint8_t newData, EmuTime::param time)
{
	if (newData != data) {
		data = newData;
		getPluggedPrintDev().writeData(data, time);
	}
}

std::string_view MSXPrinterPort::getDescription() const
{
	return "MSX Printer port";
}

std::string_view MSXPrinterPort::getClass() const
{
	return "Printer Port";
}

void MSXPrinterPort::plug(Pluggable& dev, EmuTime::param time)
{
	Connector::plug(dev, time);
	getPluggedPrintDev().writeData(data, time);
	getPluggedPrintDev().setStrobe(strobe, time);
}

PrinterPortDevice& MSXPrinterPort::getPluggedPrintDev() const
{
	return *checked_cast<PrinterPortDevice*>(&getPlugged());
}


MSXPrinterPort::Debuggable::Debuggable(MSXMotherBoard& motherBoard_, const std::string& name_)
	: SimpleDebuggable(motherBoard_, name_, "Printer Port", 2)
{
}

uint8_t MSXPrinterPort::Debuggable::read(unsigned address)
{
	auto& pPort = OUTER(MSXPrinterPort, debuggable);
	return (address == 0) ? pPort.strobe : pPort.data;
}

void MSXPrinterPort::Debuggable::write(unsigned address, uint8_t value)
{
	auto& pPort = OUTER(MSXPrinterPort, debuggable);
	pPort.writeIO(narrow<uint16_t>(address), value, pPort.getCurrentTime());
}


template<typename Archive>
void MSXPrinterPort::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
	ar.template serializeBase<Connector>(*this);
	ar.serialize("strobe", strobe,
	             "data",   data);
	// TODO force writing data to port??
}
INSTANTIATE_SERIALIZE_METHODS(MSXPrinterPort);
REGISTER_MSXDEVICE(MSXPrinterPort, "PrinterPort");

} // namespace openmsx
