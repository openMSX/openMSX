// $Id$

#include <cassert>
#include "MSXPrinterPort.hh"
#include "MSXMotherBoard.hh"
#include "PrinterPortDevice.hh"
#include "PluggingController.hh"
#include "PrinterPortSimpl.hh"
#include "PrinterPortLogger.hh"


MSXPrinterPort::MSXPrinterPort(MSXConfig::Device *config, const EmuTime &time)
	: MSXDevice(config, time), MSXIODevice(config, time)
{
	PRT_DEBUG("Creating a MSXPrinterPort");
	MSXMotherBoard::instance()->register_IO_In (0x90, this);
	MSXMotherBoard::instance()->register_IO_Out(0x90, this);
	MSXMotherBoard::instance()->register_IO_Out(0x91, this);

	dummy = new DummyPrinterPortDevice();
	PluggingController::instance()->registerConnector(this);
	unplug(time);	// TODO plug device as specified in config file
	reset(time);
	
	logger = new PrinterPortLogger();
	simple = new PrinterPortSimpl();
}

MSXPrinterPort::~MSXPrinterPort()
{
	PRT_DEBUG("Destroying a MSXPrinterPort");
	//unplug(time)
	PluggingController::instance()->unregisterConnector(this);
	delete dummy;
	delete logger;
	delete simple;
}


void MSXPrinterPort::reset(const EmuTime &time)
{
	writeData(0, time);	// TODO check this
	setStrobe(true, time);	// TODO check this
}


byte MSXPrinterPort::readIO(byte port, const EmuTime &time)
{
	assert(port == 0x90);
	return ((PrinterPortDevice*)pluggable)->getStatus(time) ? 0xff : 0xfd;	// bit 1 = status / other bits always 1
}

void MSXPrinterPort::writeIO(byte port, byte value, const EmuTime &time)
{
	switch (port) {
	case 0x90:
		setStrobe(value&1, time);	// bit 0 = strobe
		break;
	case 0x91:
		writeData(value, time);
		break;
	default:
		assert(false);
	}
}

void MSXPrinterPort::setStrobe(bool newStrobe, const EmuTime &time)
{
	if (newStrobe != strobe) {
		strobe = newStrobe;
		((PrinterPortDevice*)pluggable)->setStrobe(strobe, time);
	}
}
void MSXPrinterPort::writeData(byte newData, const EmuTime &time)
{
	if (newData != data) {
		data = newData;
		((PrinterPortDevice*)pluggable)->writeData(data, time);
	}
}

const std::string &MSXPrinterPort::getName()
{
	static const std::string name("printerport");
	return name;
}

const std::string &MSXPrinterPort::getClass()
{
	static const std::string className("Printer Port");
	return className;
}

void MSXPrinterPort::plug(Pluggable *dev, const EmuTime &time)
{
	Connector::plug(dev, time);
	((PrinterPortDevice*)pluggable)->writeData(data, time);
	((PrinterPortDevice*)pluggable)->setStrobe(strobe, time);
}

void MSXPrinterPort::unplug(const EmuTime &time)
{
	Connector::unplug(time);
	plug(dummy, time);
}


// --- DummyPrinterPortDevice ---

bool DummyPrinterPortDevice::getStatus(const EmuTime &time)
{
	return true;	// true = high = not ready
}

void DummyPrinterPortDevice::setStrobe(bool strobe, const EmuTime &time)
{
	// ignore strobe
}

void DummyPrinterPortDevice::writeData(byte data, const EmuTime &time)
{
	// ignore data
}
