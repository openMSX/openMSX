// $Id$

#include "MSXPrinterPort.hh"
#include "PrinterPortDevice.hh"
#include "PluggingController.hh"
#include <cassert>


namespace openmsx {

MSXPrinterPort::MSXPrinterPort(Config* config, const EmuTime& time)
	: MSXDevice(config, time)
	, MSXIODevice(config, time)
	, Connector("printerport", new DummyPrinterPortDevice())
{
	data = 255;	// != 0;
	strobe = false;	// != true;
	reset(time);

	PluggingController::instance().registerConnector(this);
}

MSXPrinterPort::~MSXPrinterPort()
{
	PluggingController::instance().unregisterConnector(this);
}

void MSXPrinterPort::reset(const EmuTime& time)
{
	writeData(0, time);	// TODO check this
	setStrobe(true, time);	// TODO check this
}


byte MSXPrinterPort::readIO(byte port, const EmuTime& time)
{
	// bit 1 = status / other bits always 1
	return ((PrinterPortDevice*)pluggable)->getStatus(time) ? 0xff : 0xfd;
}

void MSXPrinterPort::writeIO(byte port, byte value, const EmuTime& time)
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
		((PrinterPortDevice*)pluggable)->setStrobe(strobe, time);
	}
}
void MSXPrinterPort::writeData(byte newData, const EmuTime& time)
{
	if (newData != data) {
		data = newData;
		((PrinterPortDevice*)pluggable)->writeData(data, time);
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

void MSXPrinterPort::plug(Pluggable* dev, const EmuTime& time)
	throw(PlugException)
{
	Connector::plug(dev, time);
	((PrinterPortDevice *)pluggable)->writeData(data, time);
	((PrinterPortDevice *)pluggable)->setStrobe(strobe, time);
}


// --- DummyPrinterPortDevice ---

bool DummyPrinterPortDevice::getStatus(const EmuTime& time)
{
	return true;	// true = high = not ready
}

void DummyPrinterPortDevice::setStrobe(bool strobe, const EmuTime& time)
{
	// ignore strobe
}

void DummyPrinterPortDevice::writeData(byte data, const EmuTime& time)
{
	// ignore data
}

const string& DummyPrinterPortDevice::getDescription() const
{
	static const string EMPTY;
	return EMPTY;
}

void DummyPrinterPortDevice::plug(Connector* connector, const EmuTime& time)
	throw ()
{
}

void DummyPrinterPortDevice::unplug(const EmuTime& time)
{
}

} // namespace openmsx
