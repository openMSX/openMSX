// $Id$

#include "MSXPSG.hh"
#include "Leds.hh"
#include "CassettePort.hh"
#include "JoystickPort.hh"
#include "MSXConfig.hh"


namespace openmsx {

// MSXDevice
MSXPSG::MSXPSG(Device *config, const EmuTime &time)
	: MSXDevice(config, time), MSXIODevice(config, time)
{
	keyLayoutBit = deviceConfig->getParameterAsBool("keylayoutbit", false);
	short volume = (short)deviceConfig->getParameterAsInt("volume");
	ay8910 = new AY8910(*this, volume, time);
	cassette = CassettePortFactory::instance(time);

	selectedPort = 0;
	ports[0] = new JoystickPort("joyporta", time);
	ports[1] = new JoystickPort("joyportb", time);

	reset(time);
}

MSXPSG::~MSXPSG()
{
	delete ay8910;
	delete ports[0];
	delete ports[1];
}

void MSXPSG::powerOff(const EmuTime &time)
{
	ports[0]->unplug(time);
	ports[1]->unplug(time);
}

void MSXPSG::reset(const EmuTime &time)
{
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
	switch (port & 0x03) {
	case 0:
		registerLatch = value & 0x0F;
		break;
	case 1:
		//PRT_DEBUG("PSG write R#"<<(int)registerLatch<<" = "<<(int)value);
		ay8910->writeRegister(registerLatch, value, time);
		break;
	}
}


// AY8910Interface
byte MSXPSG::readA(const EmuTime &time)
{
	byte joystick = ports[selectedPort]->read(time);
	byte cassetteInput = cassette->cassetteIn(time) ? 0x80 : 0x00;
	byte keyLayout = keyLayoutBit ? 0x40 : 0x00;
	return joystick | keyLayout | cassetteInput;
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
	byte val0 =  (value & 0x03)       | ((value & 0x10) >> 2);
	byte val1 = ((value & 0x0C) >> 2) | ((value & 0x20) >> 3);
	ports[0]->write(val0, time);
	ports[1]->write(val1, time);
	selectedPort = (value & 0x40) >> 6;

	Leds::LEDCommand kana = (value & 0x80) ? Leds::KANA_OFF : Leds::KANA_ON;
	Leds::instance()->setLed(kana);
}

} // namespace openmsx
