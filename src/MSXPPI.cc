// $Id$

#include <cassert>
#include "MSXPPI.hh"
#include "Keyboard.hh"
#include "Leds.hh"
#include "MSXCPUInterface.hh"
#include "KeyClick.hh"
#include "CassettePort.hh"

// MSXDevice

MSXPPI::MSXPPI(MSXConfig::Device *config, const EmuTime &time)
	: MSXDevice(config, time), MSXIODevice(config, time)
{
	short volume = (short)deviceConfig->getParameterAsInt("volume");
	bool keyGhosting = deviceConfig->getParameterAsBool("key_ghosting");
	keyboard = new Keyboard(keyGhosting);
	i8255 = new I8255(*this, time);
	click = new KeyClick(volume, time);
	cpuInterface = MSXCPUInterface::instance();
	cassettePort = CassettePortFactory::instance(time);
	leds = Leds::instance();

	reset(time);
}

MSXPPI::~MSXPPI()
{
	PRT_DEBUG("Destroying an MSXPPI object");
	delete keyboard;
	delete i8255;
	delete click;
}


void MSXPPI::reset(const EmuTime &time)
{
	i8255->reset(time);
	click->reset(time);
}

byte MSXPPI::readIO(byte port, const EmuTime &time)
{
	switch (port & 0x03) {
	case 0:
		return i8255->readPortA(time);
	case 1:
		return i8255->readPortB(time);
	case 2:
		return i8255->readPortC(time);
	case 3:
		return i8255->readControlPort(time);
	default: // unreachable, avoid warning
		assert(false);
		return 0;
	}
}

void MSXPPI::writeIO(byte port, byte value, const EmuTime &time)
{
	switch (port & 0x03) {
	case 0:
		i8255->writePortA(value, time);
		break;
	case 1:
		i8255->writePortB(value, time);
		break;
	case 2:
		i8255->writePortC(value, time);
		break;
	case 3:
		i8255->writeControlPort(value, time);
		break;
	default:
		assert(false);
	}
}


// I8255Interface

byte MSXPPI::readA(const EmuTime &time)
{
	// port A is normally an output on MSX, reading from an output port
	// is handled internally in the 8255
	return 255;	//TODO check this
}
void MSXPPI::writeA(byte value, const EmuTime &time)
{
	cpuInterface->setPrimarySlots(value);
}

byte MSXPPI::readB(const EmuTime &time)
{
	return keyboard->getKeys()[selectedRow];
}
void MSXPPI::writeB(byte value, const EmuTime &time)
{
	// probably nothing happens on a real MSX
}

nibble MSXPPI::readC1(const EmuTime &time)
{
	return 15;	// TODO check this
}
nibble MSXPPI::readC0(const EmuTime &time)
{
	return 15;	// TODO check this
}
void MSXPPI::writeC1(nibble value, const EmuTime &time)
{
	cassettePort->setMotor(!(value & 1), time);	// 0=0n, 1=Off
	cassettePort->cassetteOut(value & 2, time);

	Leds::LEDCommand caps = (value & 4) ? Leds::CAPS_OFF : Leds::CAPS_ON;
	Leds::instance()->setLed(caps);

	click->setClick(value & 8, time);
}
void MSXPPI::writeC0(nibble value, const EmuTime &time)
{
	selectedRow = value;
}
