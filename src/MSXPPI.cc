// $Id$

#include "Keyboard.hh"
#include "MSXPPI.hh"
#include "Leds.hh"
#include <cassert>


// MSXDevice

MSXPPI::MSXPPI(MSXConfig::Device *config, const EmuTime &time)
	: MSXDevice(config, time)
{
	PRT_DEBUG("Creating an MSXPPI object");
	
	short volume = (short)deviceConfig->getParameterAsInt("volume");
	bool keyGhosting = deviceConfig->getParameterAsBool("key_ghosting");
	keyboard = new Keyboard(keyGhosting);
	i8255 = new I8255(*this, time);
	click = new KeyClick(volume, time);
	
	// Register I/O ports A8..AB for reading
	MSXMotherBoard::instance()->register_IO_In(0xA8,this);
	MSXMotherBoard::instance()->register_IO_In(0xA9,this);
	MSXMotherBoard::instance()->register_IO_In(0xAA,this);
	MSXMotherBoard::instance()->register_IO_In(0xAB,this);
	// Register I/O ports A8..AB for writing
	MSXMotherBoard::instance()->register_IO_Out(0xA8,this);
	MSXMotherBoard::instance()->register_IO_Out(0xA9,this); 
	MSXMotherBoard::instance()->register_IO_Out(0xAA,this);
	MSXMotherBoard::instance()->register_IO_Out(0xAB,this);

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
	MSXDevice::reset(time);
	i8255->reset(time);
	click->reset(time);
}

byte MSXPPI::readIO(byte port, const EmuTime &time)
{
	switch (port) {
	case 0xA8:
		return i8255->readPortA(time);
	case 0xA9:
		return i8255->readPortB(time);
	case 0xAA:
		return i8255->readPortC(time);
	case 0xAB:
		return i8255->readControlPort(time);
	default:
		assert (false); // code should never be reached
		return 255;	// avoid warning
	}
}

void MSXPPI::writeIO(byte port, byte value, const EmuTime &time)
{
	switch (port) {
	case 0xA8:
		i8255->writePortA(value, time);
		break;
	case 0xA9:
		i8255->writePortB(value, time);
		break;
	case 0xAA:
		i8255->writePortC(value, time);
		break;
	case 0xAB:
		i8255->writeControlPort(value, time);
		break;
	default:
		assert (false); // code should never be reached
	}
}


// I8255Interface

byte MSXPPI::readA(const EmuTime &time) {
	// port A is normally an output on MSX, reading from an output port
	// is handled internally in the 8255
	return 255;	//TODO check this
}
void MSXPPI::writeA(byte value, const EmuTime &time) {
	MSXMotherBoard::instance()->set_A8_Register(value);
}

byte MSXPPI::readB(const EmuTime &time) {
	const byte* src = keyboard->getKeys(); //reading the keyboard events into the matrix
	byte* dst = MSXKeyMatrix;
	for (int i=0; i<Keyboard::NR_KEYROWS; i++) {
		*dst++ = *src++;
	}
	return MSXKeyMatrix[selectedRow];
}
void MSXPPI::writeB(byte value, const EmuTime &time) {
	// probably nothing happens on a real MSX
}

nibble MSXPPI::readC1(const EmuTime &time) {
	return 15;	// TODO check this
}
nibble MSXPPI::readC0(const EmuTime &time) {
	return 15;	// TODO check this
}
void MSXPPI::writeC1(nibble value, const EmuTime &time) {
	MSXCassettePort::instance()->setMotor(!(value&1), time);	// 0=0n, 1=Off
	MSXCassettePort::instance()->cassetteOut(value&2, time);
	
	Leds::LEDCommand caps = (value&4) ? Leds::CAPS_OFF : Leds::CAPS_ON;
	Leds::instance()->setLed(caps);

	click->setClick(value&8, time);
}
void MSXPPI::writeC0(nibble value, const EmuTime &time) {
	selectedRow = value;
}
