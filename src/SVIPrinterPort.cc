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
{
	reset(getCurrentTime());
}

void SVIPrinterPort::reset(EmuTime time)
{
	writeData(0, time);    // TODO check this
	setStrobe(true, time); // TODO check this
}

uint8_t SVIPrinterPort::readIO(uint16_t port, EmuTime time)
{
	return peekIO(port, time);
}

uint8_t SVIPrinterPort::peekIO(uint16_t /*port*/, EmuTime time) const
{
	// bit 1 = status / other bits always 1
	return getPluggedPrintDev().getStatus(time) ? 0xFF : 0xFE;
}

void SVIPrinterPort::writeIO(uint16_t port, uint8_t value, EmuTime time)
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

void SVIPrinterPort::setStrobe(bool newStrobe, EmuTime time)
{
	if (newStrobe != strobe) {
		strobe = newStrobe;
		getPluggedPrintDev().setStrobe(strobe, time);
	}
}
void SVIPrinterPort::writeData(uint8_t newData, EmuTime time)
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

void SVIPrinterPort::plug(Pluggable& dev, EmuTime time)
{
	Connector::plug(dev, time);
	getPluggedPrintDev().writeData(data, time);
	getPluggedPrintDev().setStrobe(strobe, time);
}

PrinterPortDevice& SVIPrinterPort::getPluggedPrintDev() const
{
	return *checked_cast<PrinterPortDevice*>(&getPlugged());
}

template<typename Archive>
void SVIPrinterPort::serialize(Archive& ar, unsigned /*version*/)
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
