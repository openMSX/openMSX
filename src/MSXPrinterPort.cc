// $Id$

#include "MSXPrinterPort.hh"
#include "DummyPrinterPortDevice.hh"
#include "PluggingController.hh"
#include "MSXMotherBoard.hh"
#include "checked_cast.hh"
#include "serialize.hh"
#include <cassert>

using std::string;

namespace openmsx {

MSXPrinterPort::MSXPrinterPort(MSXMotherBoard& motherBoard, const XMLElement& config)
	: MSXDevice(motherBoard, config)
	, Connector("printerport", std::auto_ptr<Pluggable>(
	                                       new DummyPrinterPortDevice()))
{
	data = 255;	// != 0;
	strobe = false;	// != true;
	reset(getCurrentTime());

	getMotherBoard().getPluggingController().registerConnector(*this);
}

MSXPrinterPort::~MSXPrinterPort()
{
	getMotherBoard().getPluggingController().unregisterConnector(*this);
}

void MSXPrinterPort::reset(const EmuTime& time)
{
	writeData(0, time);	// TODO check this
	setStrobe(true, time);	// TODO check this
}

byte MSXPrinterPort::readIO(word port, const EmuTime& time)
{
	return peekIO(port, time);
}

byte MSXPrinterPort::peekIO(word /*port*/, const EmuTime& time) const
{
	// bit 1 = status / other bits always 1
	return getPluggedPrintDev().getStatus(time)
	       ? 0xFF : 0xFD;
}

void MSXPrinterPort::writeIO(word port, byte value, const EmuTime& time)
{
	switch (port & 0x01) {
	case 0:
		setStrobe(value & 1, time);	// bit 0 = strobe
		break;
	case 1:
		writeData(value, time);
		break;
	default:
		assert(false);
	}
}

void MSXPrinterPort::setStrobe(bool newStrobe, const EmuTime& time)
{
	if (newStrobe != strobe) {
		strobe = newStrobe;
		getPluggedPrintDev().setStrobe(strobe, time);
	}
}
void MSXPrinterPort::writeData(byte newData, const EmuTime& time)
{
	if (newData != data) {
		data = newData;
		getPluggedPrintDev().writeData(data, time);
	}
}

const string& MSXPrinterPort::getDescription() const
{
	static const string desc("MSX Printer port.");
	return desc;
}

const string& MSXPrinterPort::getClass() const
{
	static const string className("Printer Port");
	return className;
}

void MSXPrinterPort::plug(Pluggable& dev, const EmuTime& time)
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
	ar.serialize("strobe", strobe);
	ar.serialize("data", data);
	// TODO force writing data to port??
}
INSTANTIATE_SERIALIZE_METHODS(MSXPrinterPort);

} // namespace openmsx
