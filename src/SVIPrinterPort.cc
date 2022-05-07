#include "SVIPrinterPort.hh"
#include "DummyPrinterPortDevice.hh"
#include "checked_cast.hh"
#include "serialize.hh"
#include "unreachable.hh"
#include <memory>

// Centronics interface
//
// DAT0-DAT7   Printer data write port, output (10H)
// /STB        Printer strobe port,     output (11H)
// BUSY        Printer status port,     input  (12H)

namespace openmsx {

SVIPrinterPort::SVIPrinterPort(const DeviceConfig& config)
	: MSXDevice(config)
	, Connector(MSXDevice::getPluggingController(), "printerport",
	            std::make_unique<DummyPrinterPortDevice>())
	, strobe(false) // != true
	, data(255)     // != 0
{
	reset(getCurrentTime());
}

void SVIPrinterPort::reset(EmuTime::param time)
{
	writeData(0, time);    // TODO check this
	setStrobe(true, time); // TODO check this
}

byte SVIPrinterPort::readIO(word port, EmuTime::param time)
{
	return peekIO(port, time);
}

byte SVIPrinterPort::peekIO(word /*port*/, EmuTime::param time) const
{
	// bit 1 = status / other bits always 1
	return getPluggedPrintDev().getStatus(time) ? 0xFF : 0xFE;
}

void SVIPrinterPort::writeIO(word port, byte value, EmuTime::param time)
{
	switch (port & 0x01) {
	case 0:
		writeData(value, time);
		break;
	case 1:
		setStrobe(value & 1, time); // bit 0 = strobe
		break;
	default:
		UNREACHABLE;
	}
}

void SVIPrinterPort::setStrobe(bool newStrobe, EmuTime::param time)
{
	if (newStrobe != strobe) {
		strobe = newStrobe;
		getPluggedPrintDev().setStrobe(strobe, time);
	}
}
void SVIPrinterPort::writeData(byte newData, EmuTime::param time)
{
	if (newData != data) {
		data = newData;
		getPluggedPrintDev().writeData(data, time);
	}
}

std::string_view SVIPrinterPort::getDescription() const
{
	return "Spectravideo SVI-328 Printer port";
}

std::string_view SVIPrinterPort::getClass() const
{
	return "Printer Port";
}

void SVIPrinterPort::plug(Pluggable& dev, EmuTime::param time)
{
	Connector::plug(dev, time);
	getPluggedPrintDev().writeData(data, time);
	getPluggedPrintDev().setStrobe(strobe, time);
}

PrinterPortDevice& SVIPrinterPort::getPluggedPrintDev() const
{
	return *checked_cast<PrinterPortDevice*>(&getPlugged());
}

void SVIPrinterPort::serialize(Archive auto& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
	ar.template serializeBase<Connector>(*this);
	ar.serialize("strobe", strobe,
	             "data",   data);
	// TODO force writing data to port??
}
INSTANTIATE_SERIALIZE_METHODS(SVIPrinterPort);
REGISTER_MSXDEVICE(SVIPrinterPort, "SVI-328 PrinterPort");

} // namespace openmsx
