
#include "MSXPSG.hh"
#include "MSXMotherBoard.hh"


// MSXDevice
MSXPSG::MSXPSG()
{
	PRT_DEBUG("Creating an MSXPSG object");
	ay8910 = new AY8910(*this);
}

MSXPSG::~MSXPSG()
{
	PRT_DEBUG("Destroying an MSXPSG object");
	delete ay8910;
}

void MSXPSG::init()
{
	MSXDevice::init();
	MSXMotherBoard::instance()->register_IO_Out(0xA0,this);
	MSXMotherBoard::instance()->register_IO_Out(0xA1,this);
	MSXMotherBoard::instance()->register_IO_In (0xA2,this);
	ay8910->init();
}

void MSXPSG::reset()
{
	MSXDevice::reset();
	registerLatch = 0;
	ay8910->reset();
}

byte MSXPSG::readIO(byte port, Emutime &time)
{
	//Note Emutime argument is ignored, I think that's ok
	assert (port == 0xA2);
	return ay8910->readRegister(registerLatch);
}

void MSXPSG::writeIO(byte port, byte value, Emutime &time)
{
	//Note Emutime argument is ignored, I think that's ok
	switch (port) {
	case 0xA0:
		registerLatch = value & 0x0f;
		break;
	case 0xA1:
		ay8910->writeRegister(registerLatch, value);
		break;
	default:
		assert(false);	//code should never be reached
	}
}


// AY8910Interface
byte MSXPSG::readA()
{
	//TODO
	return 255;
}

byte MSXPSG::readB()
{
	// port B is normally an output on MSX
	return 255;
}

void MSXPSG::writeA(byte value)
{
	// port A is normally an input on MSX
	// ignore write
	return;
}

void MSXPSG::writeB(byte value)
{
	//TODO
	return;
}
