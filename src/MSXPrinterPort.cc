// $Id$

#include <cassert>
#include "MSXPrinterPort.hh"
#include "MSXMotherBoard.hh"
#include "PrinterPortDevice.hh"
#include "ConsoleSource/Console.hh"

MSXPrinterPort::MSXPrinterPort(MSXConfig::Device *config, const EmuTime &time)
	: MSXDevice(config, time)
{
	PRT_DEBUG("Creating an MSXPrinterPort");
	MSXMotherBoard::instance()->register_IO_In (0x90, this);
	MSXMotherBoard::instance()->register_IO_Out(0x90, this);
	MSXMotherBoard::instance()->register_IO_Out(0x91, this);

	dummy = new DummyPrinterPortDevice();
	Console::instance()->registerCommand(printPortCmd, "printerport");
	// TODO plug device as specified in config file
	unplug();

	//temporary hack :-) until devicefactory is fixed
	oneInstance=this;
	logger=new LoggingPrinterPortDevice();
	plug(logger);

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
	device->writeData(data);
	device->setStrobe(strobe);
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
		device->setStrobe(strobe);
		break;
	case 0x91:
		data = value;
		device->writeData(data);
		break;
	default:
		assert(false);
	}
}

void MSXPrinterPort::plug(PrinterPortDevice *dev)
{
	device = dev;
	device->writeData(data);
	device->setStrobe(strobe);
}

void MSXPrinterPort::unplug()
{
	plug(dummy);
}



bool DummyPrinterPortDevice::getStatus()
{
	return true;	// true = high = not ready	TODO check this
}

void DummyPrinterPortDevice::setStrobe(bool strobe)
{
	// ignore strobe
}

void DummyPrinterPortDevice::writeData(byte data)
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

void MSXPrinterPort::printPortCmd::execute(const char* commandLine)
{
	bool error = false;
	if (0 == strncmp(&commandLine[12], "unplug", 6)) {
		MSXPrinterPort::instance()->unplug();
	} else if (0 == strncmp(&commandLine[12], "log", 3)) {
		MSXPrinterPort::instance()->plug(logger);
	} else {
		error = true;
	};
	if (error)
		Console::instance()->print("syntax error");
}

void MSXPrinterPort::printPortCmd::help(const char *commandLine)
{
	Console::instance()->print("printerport unplug         unplugs device from port");
	Console::instance()->print("printerport log <filename> log printerdata to file");
}

bool LoggingPrinterPortDevice::getStatus()
{
	return false;	// true = high = not ready	TODO check this
}

void LoggingPrinterPortDevice::setStrobe(bool strobe)
{
  // ignore strobe
  if (strobe){
    PRT_DEBUG("PRINTER: save in printlog file "<<toPrint);
  } else {
    PRT_DEBUG("PRINTER: strobe off");
  }
}

void LoggingPrinterPortDevice::writeData(byte data)
{
	toPrint=data;
	PRT_DEBUG("PRINTER: setting data "<<data);
}

