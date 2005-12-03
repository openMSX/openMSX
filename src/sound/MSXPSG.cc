// $Id$

#include "MSXPSG.hh"
#include "AY8910.hh"
#include "LedEvent.hh"
#include "EventDistributor.hh"
#include "CassettePort.hh"
#include "MSXMotherBoard.hh"
#include "JoystickPort.hh"
#include "XMLElement.hh"
#include "RenShaTurbo.hh"

namespace openmsx {

// MSXDevice
MSXPSG::MSXPSG(MSXMotherBoard& motherBoard, const XMLElement& config,
               const EmuTime& time)
	: MSXDevice(motherBoard, config, time)
	, cassette(motherBoard.getCassettePort())
	, renShaTurbo(motherBoard.getRenShaTurbo())
	, prev(255)
{
	keyLayoutBit = deviceConfig.getChildData("keyboardlayout", "") == "JIS";
	ay8910.reset(new AY8910(motherBoard, *this, config, time));

	selectedPort = 0;
	PluggingController& controller = motherBoard.getPluggingController();
	ports[0].reset(new JoystickPort(controller, "joyporta"));
	ports[1].reset(new JoystickPort(controller, "joyportb"));

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

void MSXPSG::powerDown(const EmuTime& /*time*/)
{
	getMotherBoard().getEventDistributor().distributeEvent(
		new LedEvent(LedEvent::KANA, false));
}

byte MSXPSG::readIO(word /*port*/, const EmuTime& time)
{
	byte result = ay8910->readRegister(registerLatch, time);
	//PRT_DEBUG("PSG read R#"<<(int)registerLatch<<" = "<<(int)result);
	return result;
}

byte MSXPSG::peekIO(word /*port*/, const EmuTime& time) const
{
	return ay8910->peekRegister(registerLatch, time);
}

void MSXPSG::writeIO(word port, byte value, const EmuTime& time)
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


// AY8910Periphery
byte MSXPSG::readA(const EmuTime& time)
{
	byte joystick = ports[selectedPort]->read(time) |
	                ((renShaTurbo.getSignal(time)) << 4);
	byte cassetteInput = cassette.cassetteIn(time) ? 0x80 : 0x00;
	byte keyLayout = keyLayoutBit ? 0x40 : 0x00;
	return joystick | keyLayout | cassetteInput;
}

void MSXPSG::writeB(byte value, const EmuTime& time)
{
	byte val0 =  (value & 0x03)       | ((value & 0x10) >> 2);
	byte val1 = ((value & 0x0C) >> 2) | ((value & 0x20) >> 3);
	ports[0]->write(val0, time);
	ports[1]->write(val1, time);
	selectedPort = (value & 0x40) >> 6;

	if ((prev ^ value) & 0x80) {
		getMotherBoard().getEventDistributor().distributeEvent(
			new LedEvent(LedEvent::KANA, !(value & 0x80)));
	}
	prev = value;
}

} // namespace openmsx
