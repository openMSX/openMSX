// $Id$

#include <cassert>
#include "MSXPrinterPort.hh"
#include "MSXMotherBoard.hh"
#include "PrinterPortDevice.hh"
#include "ConsoleSource/Console.hh"
#include "ConsoleSource/CommandController.hh"
#include "MSXCPU.hh"


MSXPrinterPort::MSXPrinterPort(MSXConfig::Device *config, const EmuTime &time)
	: MSXDevice(config, time)
{
	PRT_DEBUG("Creating an MSXPrinterPort");
	MSXMotherBoard::instance()->register_IO_In (0x90, this);
	MSXMotherBoard::instance()->register_IO_Out(0x90, this);
	MSXMotherBoard::instance()->register_IO_Out(0x91, this);

	dummy = new DummyPrinterPortDevice();
	CommandController::instance()->registerCommand(printPortCmd, "printerport");
	// TODO plug device as specified in config file
	unplug(time);

	//temporary hack :-) until devicefactory is fixed
	{
		oneInstance=this;
		logger=new LoggingPrinterPortDevice();
		plug(logger, time);
	}

	reset(time);
}

MSXPrinterPort::~MSXPrinterPort()
{
	delete dummy;
	delete logger;
}

MSXPrinterPort* MSXPrinterPort::instance()
{
/*
  if (oneInstance == 0 ) {
    oneInstance = new MSXPrinterPort();
  }
*/
  return oneInstance;
}
MSXPrinterPort* MSXPrinterPort::oneInstance = 0;
LoggingPrinterPortDevice* MSXPrinterPort::logger = 0;


void MSXPrinterPort::reset(const EmuTime &time)
{
	data = 0;	// TODO check this
	strobe = true;	// TODO 
	device->writeData(data, time);
	device->setStrobe(strobe, time);
}


byte MSXPrinterPort::readIO(byte port, const EmuTime &time)
{
	assert(port == 0x90);
	return device->getStatus() ? 0xff : 0xfd;	// bit 1 = status / other bits always 1
}

void MSXPrinterPort::writeIO(byte port, byte value, const EmuTime &time)
{
	switch (port) {
	case 0x90:
		strobe = value&1;	// bit 0 = strobe
		device->setStrobe(strobe, time);
		break;
	case 0x91:
		data = value;
		device->writeData(data, time);
		break;
	default:
		assert(false);
	}
}

void MSXPrinterPort::plug(PrinterPortDevice *dev, const EmuTime &time)
{
	device = dev;
	device->writeData(data, time);
	device->setStrobe(strobe, time);
}

void MSXPrinterPort::unplug(const EmuTime &time)
{
	plug(dummy, time);
}



bool DummyPrinterPortDevice::getStatus()
{
	return true;	// true = high = not ready	TODO check this
}

void DummyPrinterPortDevice::setStrobe(bool strobe, const EmuTime &time)
{
	// ignore strobe
}

void DummyPrinterPortDevice::writeData(byte data, const EmuTime &time)
{
	// ignore data
}

MSXPrinterPort::printPortCmd::printPortCmd()
{
   PRT_DEBUG("Creating printPortCmd");
}


MSXPrinterPort::printPortCmd::~printPortCmd()
{
   PRT_DEBUG("Destructing printPortCmd");
}

void MSXPrinterPort::printPortCmd::execute(const std::vector<std::string> &tokens)
{
	const EmuTime &time = MSXCPU::instance()->getCurrentTime();
	bool error = false;
	if        (tokens[1]=="unplug") {
		MSXPrinterPort::instance()->unplug(time);
	} else if (tokens[1]=="log") {
		MSXPrinterPort::instance()->plug(logger, time);
	} else {
		error = true;
	}
	if (error)
		Console::instance()->print("syntax error");
}

void MSXPrinterPort::printPortCmd::help   (const std::vector<std::string> &tokens)
{
	Console::instance()->print("printerport unplug         unplugs device from port");
	Console::instance()->print("printerport log <filename> log printerdata to file");
}

bool LoggingPrinterPortDevice::getStatus()
{
	return false;	// false = low = ready	TODO check this
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

