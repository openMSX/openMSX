
#include <assert.h>
#include "MSXRTC.hh"
#include "MSXMotherBoard.hh"


MSXRTC::MSXRTC()
{
	rp5c01 = new RP5C01();
}

MSXRTC::~MSXRTC()
{
	delete rp5c01;
}

void MSXRTC::init()
{
	MSXDevice::init();
	MSXMotherBoard::instance()->register_IO_Out(0xB4,this);
	MSXMotherBoard::instance()->register_IO_Out(0xB5,this);
	MSXMotherBoard::instance()->register_IO_In (0xB5,this);
}

void MSXRTC::reset()
{
	rp5c01->reset();
}

byte MSXRTC::readIO(byte port, Emutime &time)
{
	assert(port==0xB5);
	return rp5c01->readPort(registerLatch);
}

void MSXRTC::writeIO(byte port, byte value, Emutime &time)
{
	switch (port) {
	case 0xB4:
		registerLatch = value&0x0f;
		break;
	case 0xB5:
		rp5c01->writePort(registerLatch, value&0x0f);
		break;
	default:
		assert(false);
	}
}

