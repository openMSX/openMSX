// $Id$

#include "MSXPSG.hh"
#include "LedEvent.hh"
#include "EventDistributor.hh"
#include "CassettePort.hh"
#include "JoystickPort.hh"
#include "XMLElement.hh"
#include "RenShaTurbo.hh"

namespace openmsx {

// MSXDevice
MSXPSG::MSXPSG(const XMLElement& config, const EmuTime& time)
	: MSXDevice(config, time)
	, cassette(CassettePortFactory::instance())
{
	keyLayoutBit = deviceConfig.getChildData("keyboardlayout", "") == "JIS";
	ay8910.reset(new AY8910(*this, config, time));

	selectedPort = 0;
	ports[0].reset(new JoystickPort("joyporta"));
	ports[1].reset(new JoystickPort("joyportb"));

	reset(time);
}

MSXPSG::~MSXPSG()
{
}

void MSXPSG::reset(const EmuTime& time)
{
	registerLatch = 0;
	ay8910->reset(time);
}

void MSXPSG::powerDown(const EmuTime& time)
{
	EventDistributor::instance().distributeEvent(
		new LedEvent(LedEvent::KANA, false));
}

byte MSXPSG::readIO(byte /*port*/, const EmuTime& time)
{
	byte result = ay8910->readRegister(registerLatch, time);
	//PRT_DEBUG("PSG read R#"<<(int)registerLatch<<" = "<<(int)result);
	return result;
}

byte MSXPSG::peekIO(byte /*port*/, const EmuTime& time) const
{
	return ay8910->peekRegister(registerLatch, time);
}

void MSXPSG::writeIO(byte port, byte value, const EmuTime& time)
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
byte MSXPSG::readA(const EmuTime& time)
{
	return peekA(time);
}
byte MSXPSG::peekA(const EmuTime& time) const
{
	byte joystick = ports[selectedPort]->read(time) | 
	                ((RenShaTurbo::instance().getSignal(time)) << 4);
	byte cassetteInput = cassette.cassetteIn(time) ? 0x80 : 0x00;
	byte keyLayout = keyLayoutBit ? 0x40 : 0x00;
	return joystick | keyLayout | cassetteInput;
}

byte MSXPSG::readB(const EmuTime& time)
{
	return peekB(time);
}
byte MSXPSG::peekB(const EmuTime& /*time*/) const
{
	// port B is normally an output on MSX
	return 255;
}

void MSXPSG::writeA(byte /*value*/, const EmuTime& /*time*/)
{
	// port A is normally an input on MSX
	// ignore write
	return;
}

void MSXPSG::writeB(byte value, const EmuTime& time)
{
	byte val0 =  (value & 0x03)       | ((value & 0x10) >> 2);
	byte val1 = ((value & 0x0C) >> 2) | ((value & 0x20) >> 3);
	ports[0]->write(val0, time);
	ports[1]->write(val1, time);
	selectedPort = (value & 0x40) >> 6;

	EventDistributor::instance().distributeEvent(
		new LedEvent(LedEvent::KANA, !(value & 0x80)));
}

} // namespace openmsx
