// $Id$

#include <cassert>
#include "MSXPrinterPort.hh"
#include "MSXMotherBoard.hh"

MSXPrinterPort::MSXPrinterPort(MSXConfig::Device *config) : MSXDevice(config)
{
}

MSXPrinterPort::~MSXPrinterPort()
{
	delete dummy;
}


void MSXPrinterPort::init()
{
	MSXMotherBoard::instance()->register_IO_In (0x90, this);
	MSXMotherBoard::instance()->register_IO_Out(0x90, this);
	MSXMotherBoard::instance()->register_IO_Out(0x91, this);

	dummy = new DummyPrinterPortDevice();
	// TODO plug device as specified in config file
	unplug();
}

byte MSXPrinterPort::readIO(byte port, EmuTime &time)
{
	assert(port == 0x90);
	return device->getStatus() ? 0xff : 0xfd;	// bit 1 = status / other bits always 1
}

void MSXPrinterPort::writeIO(byte port, byte value, EmuTime &time)
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
