// $Id$

#include "MSXPSG.hh"
#include "MSXMotherBoard.hh"
#include "JoystickPorts.hh"
#include "Leds.hh"
#include "CassettePort.hh"

// MSXDevice
MSXPSG::MSXPSG(MSXConfig::Device *config, const EmuTime &time)
	: MSXDevice(config, time), MSXIODevice(config, time)
{
	PRT_DEBUG("Creating an MSXPSG object");
	
	short volume = (short)deviceConfig->getParameterAsInt("volume");
	ay8910 = new AY8910(*this, volume, time);
	joyPorts = new JoystickPorts(time);
	cassette = CassettePortFactory::instance(time);
	
	MSXMotherBoard::instance()->register_IO_Out(0xA0,this);
	MSXMotherBoard::instance()->register_IO_Out(0xA1,this);
	MSXMotherBoard::instance()->register_IO_In (0xA2,this);

	reset(time);
}

MSXPSG::~MSXPSG()
{
	PRT_DEBUG("Destroying an MSXPSG object");
	delete ay8910;
	delete joyPorts;
}

void MSXPSG::reset(const EmuTime &time)
{
	MSXDevice::reset(time);
	registerLatch = 0;
	ay8910->reset(time);
}

byte MSXPSG::readIO(byte port, const EmuTime &time)
{
	byte result =  ay8910->readRegister(registerLatch, time);
	//PRT_DEBUG("PSG read R#"<<(int)registerLatch<<" = "<<(int)result);
	return result;
}

void MSXPSG::writeIO(byte port, byte value, const EmuTime &time)
{
	switch (port) {
	case 0xA0:
		registerLatch = value & 0x0f;
		break;
	case 0xA1:
		//PRT_DEBUG("PSG write R#"<<(int)registerLatch<<" = "<<(int)value);
		ay8910->writeRegister(registerLatch, value, time);
		break;
	}
}


// AY8910Interface
byte MSXPSG::readA(const EmuTime &time)
{
	byte joystick = joyPorts->read(time);
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
	joyPorts->write(value, time);
	
	Leds::LEDCommand kana = (value&0x80) ? Leds::KANA_OFF : Leds::KANA_ON;
	Leds::instance()->setLed(kana);
}
