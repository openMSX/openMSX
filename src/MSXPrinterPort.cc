// $Id$

#include <cassert>
#include "MSXPrinterPort.hh"
#include "MSXMotherBoard.hh"
#include "PrinterPortDevice.hh"
#include "PluggingController.hh"
#include "PrinterPortSimple.hh"


MSXPrinterPort::MSXPrinterPort(MSXConfig::Device *config, const EmuTime &time)
	: MSXDevice(config, time)
{
	PRT_DEBUG("Creating a MSXPrinterPort");
	MSXMotherBoard::instance()->register_IO_In (0x90, this);
	MSXMotherBoard::instance()->register_IO_Out(0x90, this);
	MSXMotherBoard::instance()->register_IO_Out(0x91, this);

	dummy = new DummyPrinterPortDevice();
	PluggingController::instance()->registerConnector(this);
	unplug(time);	// TODO plug device as specified in config file
	reset(time);
	
	logger = new LoggingPrinterPortDevice();
	simple = new PrinterPortSimple();
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
	data = 0;	// TODO check this
	strobe = true;	// TODO 
	((PrinterPortDevice*)pluggable)->writeData(data, time);
	((PrinterPortDevice*)pluggable)->setStrobe(strobe, time);
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
		strobe = value&1;	// bit 0 = strobe
		((PrinterPortDevice*)pluggable)->setStrobe(strobe, time);
		break;
	case 0x91:
		data = value;
		((PrinterPortDevice*)pluggable)->writeData(data, time);
		break;
	default:
		assert(false);
	}
}


const std::string &MSXPrinterPort::getName()
{
	return name;
}
const std::string MSXPrinterPort::name("printerport");

const std::string &MSXPrinterPort::getClass()
{
	return className;
}
const std::string MSXPrinterPort::className("Printer Port");

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

const std::string &DummyPrinterPortDevice::getName()
{
	return name;
}
const std::string DummyPrinterPortDevice::name("dummy");


// --- LoggingPrinterPortDevice ---

LoggingPrinterPortDevice::LoggingPrinterPortDevice()
{
	PluggingController::instance()->registerPluggable(this);
}

LoggingPrinterPortDevice::~LoggingPrinterPortDevice()
{
	PluggingController::instance()->unregisterPluggable(this);
}

bool LoggingPrinterPortDevice::getStatus(const EmuTime &time)
{
	return false;	// false = low = ready
}

void LoggingPrinterPortDevice::setStrobe(bool strobe, const EmuTime &time)
{
	if (strobe) {
		PRT_DEBUG("PRINTER: save in printlog file "<<toPrint);
	} else {
		PRT_DEBUG("PRINTER: strobe off");
	}
}

void LoggingPrinterPortDevice::writeData(byte data, const EmuTime &time)
{
	toPrint=data;
	PRT_DEBUG("PRINTER: setting data "<<data);
}

const std::string &LoggingPrinterPortDevice::getName()
{
	return name;
}
const std::string LoggingPrinterPortDevice::name("logger");

