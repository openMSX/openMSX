// $Id$

#include "Keyboard.hh"
#include "MSXPPI.hh"
#include "Leds.hh"
#include <cassert>


// MSXDevice

MSXPPI::MSXPPI()
{
	PRT_DEBUG("Creating an MSXPPI object");
	keyboard = new Keyboard(true); // TODO make configurable
	i8255 = new I8255(*this);
	click = new KeyClick();	// not too early
}

MSXPPI::~MSXPPI()
{
	PRT_DEBUG("Destroying an MSXPPI object");
	delete keyboard;
	delete i8255;
	delete click;
}

MSXPPI* MSXPPI::instance(void)
{
	if (oneInstance == NULL ) {
		oneInstance = new MSXPPI();
	}
	return oneInstance;
}
MSXPPI *MSXPPI::oneInstance = NULL;


void MSXPPI::init()
{
	MSXDevice::init();
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
	
	click->init();
}

void MSXPPI::reset()
{
	MSXDevice::reset();
	i8255->reset();
	click->reset();
}

byte MSXPPI::readIO(byte port, Emutime &time)
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

void MSXPPI::writeIO(byte port, byte value, Emutime &time)
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

byte MSXPPI::readA(const Emutime &time) {
	// port A is normally an output on MSX, reading from an output port
	// is handled internally in the 8255
	return 255;	//TODO check this
}
void MSXPPI::writeA(byte value, const Emutime &time) {
	MSXMotherBoard::instance()->set_A8_Register(value);
}

byte MSXPPI::readB(const Emutime &time) {
	const byte* src = keyboard->getKeys(); //reading the keyboard events into the matrix
	byte* dst = MSXKeyMatrix;
	for (int i=0; i<Keyboard::NR_KEYROWS; i++) {
		*dst++ = *src++;
	}
	return MSXKeyMatrix[selectedRow];
}
void MSXPPI::writeB(byte value, const Emutime &time) {
	// probably nothing happens on a real MSX
}

nibble MSXPPI::readC1(const Emutime &time) {
	return 15;	// TODO check this
}
nibble MSXPPI::readC0(const Emutime &time) {
	return 15;	// TODO check this
}
void MSXPPI::writeC1(nibble value, const Emutime &time) {
	//TODO use these bits
	//  4    CASON  Cassette motor relay        (0=On, 1=Off)
	//  5    CASW   Cassette audio out          (Pulse)
	Leds::LEDCommand caps = (value&4) ? Leds::CAPS_OFF : Leds::CAPS_ON;
	Leds::instance()->setLed(caps);

	click->setClick(value&8, time);
}
void MSXPPI::writeC0(nibble value, const Emutime &time) {
	selectedRow = value;
}
