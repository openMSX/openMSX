// $Id$

#include "Inputs.hh"
#include "MSXPPI.hh"
#include <SDL/SDL.h>
#include "assert.h"


MSXPPI *volatile MSXPPI::oneInstance;

MSXPPI::MSXPPI()
{
	cout << "Creating an MSXPPI object \n";
	keyboardGhosting = true;
}

MSXPPI::~MSXPPI()
{
	cout << "Destroying an MSXPPI object \n";
}

MSXDevice* MSXPPI::instance(void)
{
	if (oneInstance == NULL ) {
		oneInstance = new MSXPPI();
	}
	return oneInstance;
}

void MSXPPI::init()
{	
	// in most cases just one PPI so we register permenantly with 
	// Inputs object for now.
	//
	// The Sunrise Dual Headed MSX experiment had two!
	// Recreating this would mean making a second PPI instance by means
	// of a derived class (MSXPPI is singleton !!)
	Inputs::instance()->setPPI(this);

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
}

void MSXPPI::reset()
{
	//TODO
}

byte MSXPPI::readIO(byte port, Emutime &time)
{
	//Note Emutime argument is ignored, I think that's ok
	switch (port) {
	case 0xA8:
		return PPIReadPortA();
	case 0xA9:
		return PPIReadPortB();
	case 0xAA:
		return PPIReadPortC();
	case 0xAB:
		return PPIReadControlPort();
	default:
		assert (false); // code should never be reached
	}
}

void MSXPPI::writeIO(byte port, byte value, Emutime &time)
{
	//Note Emutime argument is ignored, I think that's ok
	switch (port) {
	case 0xA8:
		PPIWritePortA(value);
		break;
	case 0xA9:
		PPIWritePortB(value);
		break;
	case 0xAA:
		PPIWritePortC(value);
		break;
	case 0xAB:
		PPIWriteControlPort(value);
		break;
	default:
		assert (false); // code should never be reached
	}
}


byte MSXPPI::PPIReadPortA() {
	switch (control & MODE_A) {
	case MODEA_0:
		if (control & DIRECTION_A) {
			//input
			return PPIInterfaceReadA();	// input not latched
		} else {
			//output
			return latchPortA;		// output is latched
		}
	case MODEA_1:		//TODO but not relevant for MSX
	case MODEA_2: case MODEA_2_:
	default:
		assert (false);
	}
}

byte MSXPPI::PPIReadPortB() {
	switch (control & MODE_B) {
	case MODEB_0:
		if (control & DIRECTION_B) {
			//input
			return PPIInterfaceReadB();	// input not latched
		} else {
			//output
			return latchPortB;		// output is latched
		}
	case MODEB_1:		// TODO but not relevant for MSX
	default:
		assert (false);
	}
}

byte MSXPPI::PPIReadPortC() {
	byte tmp = PPIReadC1() & PPIReadC0();
	switch (control & MODE_A) {
	case MODEA_0:
		// do nothing
		break;
	case MODEA_1:		// TODO but not relevant for MSX
	case MODEA_2: case MODEA_2_:
	default:
		assert (false);
	}
	switch (control & MODE_B) {
	case MODEB_0:
		// do nothing
		break;
	case MODEB_1:		// TODO but not relevant for MSX
	default:
		assert (false);
	}
	return tmp;
}

byte MSXPPI::PPIReadC1() {
	if (control & DIRECTION_C1) {
		//input
		return PPIInterfaceReadC1() << 4;	// input not latched
	} else {
		//output
		return latchPortC & 0xf0;		// output is latched
	}
}

byte MSXPPI::PPIReadC0() {
	if (control & DIRECTION_C0) {
		//input
		return PPIInterfaceReadC0();		// input not latched
	} else {
		//output
		return latchPortC & 0x0f;			// output is latched
	}
}

byte MSXPPI::PPIReadControlPort() {
	return control;
}

void MSXPPI::PPIWritePortA(byte value) {
	switch (control & MODE_A) {
	case MODEA_0:
		// do nothing
		break;
	case MODEA_1:		// TODO but not relevant for MSX
	case MODEA_2: case MODEA_2_:
	default:
		assert (false);
	}
	PPIOutputPortA(value);
}

void MSXPPI::PPIWritePortB(byte value) {
	switch (control & MODE_B) {
	case MODEB_0:
		// do nothing
		break;
	case MODEB_1:		// TODO but not relevant for MSX
	default:
		assert (false);
	}
	PPIOutputPortB(value);
}

void MSXPPI::PPIWritePortC(byte value) {
	switch (control & MODE_A) {
	case MODEA_0:
		// do nothing
		break;
	case MODEA_1:		// TODO but not relevant for MSX
	case MODEA_2: case MODEA_2_:
	default:
		assert (false);
	}
	switch (control & MODE_B) {
	case MODEB_0:
		// do nothing
		break;
	case MODEB_1:		// TODO but not relevant for MSX
	default:
		assert (false);
	}
	PPIOutputPortC(value);
}

void MSXPPI::PPIOutputPortA(byte value) {
	latchPortA = value;
	if (!(control & DIRECTION_A)) {
		//output
		PPIInterfaceWriteA(value);
	}
}
	
void MSXPPI::PPIOutputPortB(byte value) {
	latchPortB = value;
	if (!(control & DIRECTION_B)) {
		//output
		PPIInterfaceWriteB(value);
	}
}
	
void MSXPPI::PPIOutputPortC(byte value) {
	latchPortC = value;
	if (!(control & DIRECTION_C1)) {
		//output
		PPIInterfaceWriteC1(latchPortC >> 4);
	}
	if (!(control & DIRECTION_C0)) {
		//output
		PPIInterfaceWriteC0(latchPortC & 15);
	}
}

void MSXPPI::PPIWriteControlPort(byte value) {
	if (value & SET_MODE) {
		// set new control mode
		control = value;
		// TODO check this behavior, see tech docs
		PPIOutputPortA(latchPortA);
		PPIOutputPortB(latchPortB);
		PPIOutputPortC(latchPortC);
	} else {
		// (re)set bit of port C
		byte bitmask = 1 << ((value & BIT_NR) >> 1);
		if (value & SET_RESET) {
			// set
			latchPortC |= bitmask;
		} else {
			// reset
			latchPortC &= ~bitmask;
		}
		PPIOutputPortC(latchPortC);
		switch (control & MODE_A) {
		case MODEA_0:
			// do nothing
			break;
		case MODEA_1:		// TODO but not relevant for MSX
		case MODEA_2: case MODEA_2_:
		default:
			assert (false);
		}
		switch (control & MODE_B) {
		case MODEB_0:
			// do nothing
			break;
		case MODEB_1:		// TODO but not relevant for MSX
		default:
			assert (false);
		}
	}
}
	
byte MSXPPI::PPIInterfaceReadA() {
	// port A is normally an output on MSX, reading from an output port
	// is handled internally in the PPI
	return 255;	//TODO check this
}
void MSXPPI::PPIInterfaceWriteA(byte value) {
	MSXMotherBoard::instance()->set_A8_Register(value);
}

byte MSXPPI::PPIInterfaceReadB() {
	Inputs::instance()->getKeys(); //reading the keyboard events into the matrix
	if (keyboardGhosting) {
		keyGhosting();
	}
	return MSXKeyMatrix[selectedRow];
}
void MSXPPI::PPIInterfaceWriteB(byte value) {
	// probably nothing happens on a real MSX
}

byte MSXPPI::PPIInterfaceReadC1() {
	return 15;	// TODO check this
}
byte MSXPPI::PPIInterfaceReadC0() {
	return 15;	// TODO check this
}
void MSXPPI::PPIInterfaceWriteC1(byte value) {
	//TODO use these bits
	//  4    CASON  Cassette motor relay        (0=On, 1=Off)
	//  5    CASW   Cassette audio out          (Pulse)
	//  6    CAPS   CAPS-LOCK lamp              (0=On, 1=Off)
	//  7    SOUND  Keyboard klick bit          (Pulse)
}
void MSXPPI::PPIInterfaceWriteC0(byte value) {
	selectedRow = value;
}


void MSXPPI::keyGhosting(void)
{
// This routine enables keyghosting as seen on a real MSX
//
// If on a real MSX in the keyboardmatrix the
// real buttons are pressed as in the left matrix
//           then the matrix to the
// 10111111  right will be read by   10110101
// 11110101  because of the simple   10110101
// 10111101  electrical connections  10110101
//           that are established  by
// the closed switches 

#define NR_KEYROWS 15

	bool changed_something = false;
	do {
		for (int i=0; i<=NR_KEYROWS; i++) {
			for (int j=0; j<=NR_KEYROWS; j++) {
				if ((MSXKeyMatrix[i]|MSXKeyMatrix[j]) != 255) {
					int rowanded=MSXKeyMatrix[i]&MSXKeyMatrix[j];
					if (rowanded != MSXKeyMatrix[i]) {
						MSXKeyMatrix[i]=rowanded;
						changed_something = true;
					}
				}
			}
		}
	} while (changed_something);
}

