// $Id$

#include "MSXPSG.hh"
#include "MSXMotherBoard.hh"
#include "JoystickPorts.hh"
#include "Leds.hh"


// MSXDevice
MSXPSG::MSXPSG(MSXConfig::Device *config) : MSXDevice(config)
{
	PRT_DEBUG("Creating an MSXPSG object");
}

MSXPSG::~MSXPSG()
{
	PRT_DEBUG("Destroying an MSXPSG object");
	delete ay8910;
	//delete joyPorts;	// is singleton
}

void MSXPSG::init()
{
	MSXDevice::init();
	ay8910 = new AY8910(*this);
	joyPorts = JoystickPorts::instance();
	cassette = MSXCassettePort::instance();
	
	MSXMotherBoard::instance()->register_IO_Out(0xA0,this);
	MSXMotherBoard::instance()->register_IO_Out(0xA1,this);
	MSXMotherBoard::instance()->register_IO_In (0xA2,this);
}

void MSXPSG::reset()
{
	MSXDevice::reset();
	registerLatch = 0;
	ay8910->reset();
}

byte MSXPSG::readIO(byte port, EmuTime &time)
{
	//Note EmuTime argument is ignored, I think that's ok
	assert (port == 0xA2);
	return ay8910->readRegister(registerLatch, time);
}

void MSXPSG::writeIO(byte port, byte value, EmuTime &time)
{
	//Note EmuTime argument is ignored, I think that's ok
	switch (port) {
	case 0xA0:
		registerLatch = value & 0x0f;
		break;
	case 0xA1:
		ay8910->writeRegister(registerLatch, value, time);
		break;
	default:
		assert(false);	//code should never be reached
	}
}


// AY8910Interface
byte MSXPSG::readA(const EmuTime &time)
{
	byte joystick = joyPorts->read();
	byte keyLayout = 0;		//TODO
	byte cassetteInput = cassette->cassetteIn(time) ? 128 : 0;
	return joystick | (keyLayout<<6) | cassetteInput;
}

byte MSXPSG::readB(const EmuTime &time)
{
	// port B is normally an output on MSX
	return 255;
}

void MSXPSG::writeA(byte value, const EmuTime &time)
{
	// port A is normally an input on MSX
	// ignore write
	return;
}

void MSXPSG::writeB(byte value, const EmuTime &time)
{
	joyPorts->write(value);
	
	Leds::LEDCommand kana = (value&0x80) ? Leds::KANA_OFF : Leds::KANA_ON;
	Leds::instance()->setLed(kana);
}
