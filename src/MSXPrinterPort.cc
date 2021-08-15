#include "MSXPrinterPort.hh"
#include "DummyPrinterPortDevice.hh"
#include "checked_cast.hh"
#include "outer.hh"
#include "serialize.hh"
#include "unreachable.hh"
#include <memory>

namespace openmsx {

MSXPrinterPort::MSXPrinterPort(const DeviceConfig& config)
	: MSXDevice(config)
	, Connector(MSXDevice::getPluggingController(), "printerport",
	            std::make_unique<DummyPrinterPortDevice>())
	, debuggable(getMotherBoard(), MSXDevice::getName())
{
	data = 255;     // != 0;
	strobe = false; // != true;
	reset(getCurrentTime());
}

void MSXPrinterPort::reset(EmuTime::param time)
{
	writeData(0, time);    // TODO check this
	setStrobe(true, time); // TODO check this
}

byte MSXPrinterPort::readIO(word port, EmuTime::param time)
{
	return peekIO(port, time);
}

byte MSXPrinterPort::peekIO(word /*port*/, EmuTime::param time) const
{
	// bit 1 = status / other bits always 1
	return getPluggedPrintDev().getStatus(time)
	       ? 0xFF : 0xFD;
}

void MSXPrinterPort::writeIO(word port, byte value, EmuTime::param time)
{
	switch (port & 0x01) {
	case 0:
		setStrobe(value & 1, time); // bit 0 = strobe
		break;
	case 1:
		writeData(value, time);
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
void MSXPrinterPort::writeData(byte newData, EmuTime::param time)
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

byte MSXPrinterPort::Debuggable::read(unsigned address)
{
	auto& pPort = OUTER(MSXPrinterPort, debuggable);
	return (address == 0) ? pPort.strobe : pPort.data;
}

void MSXPrinterPort::Debuggable::write(unsigned address, byte value)
{
	auto& pPort = OUTER(MSXPrinterPort, debuggable);
	pPort.writeIO(address, value, pPort.getCurrentTime());
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
