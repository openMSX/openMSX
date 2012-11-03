// $Id$

#include "MSXPrinterPort.hh"
#include "DummyPrinterPortDevice.hh"
#include "checked_cast.hh"
#include "serialize.hh"
#include "unreachable.hh"

using std::string;

namespace openmsx {

MSXPrinterPort::MSXPrinterPort(const DeviceConfig& config)
	: MSXDevice(config)
	, Connector(MSXDevice::getPluggingController(), "printerport",
	            std::unique_ptr<Pluggable>(new DummyPrinterPortDevice()))
{
	data = 255;     // != 0;
	strobe = false; // != true;
	reset(getCurrentTime());
}

MSXPrinterPort::~MSXPrinterPort()
{
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

const string MSXPrinterPort::getDescription() const
{
	return "MSX Printer port";
}

string_ref MSXPrinterPort::getClass() const
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

template<typename Archive>
void MSXPrinterPort::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
	ar.template serializeBase<Connector>(*this);
	ar.serialize("strobe", strobe);
	ar.serialize("data", data);
	// TODO force writing data to port??
}
INSTANTIATE_SERIALIZE_METHODS(MSXPrinterPort);
REGISTER_MSXDEVICE(MSXPrinterPort, "PrinterPort");

} // namespace openmsx
