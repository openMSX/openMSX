// $Id$

#include "I8255.hh"
#include <assert.h>


I8255::I8255(I8255Interface &interf) : interface(interf)
{
	reset();
}

I8255::~I8255()
{
}

void I8255::reset()
{
	latchPortA = 0;
	latchPortB = 0;
	latchPortC = 0;
	writeControlPort(SET_MODE | DIRECTION_A | DIRECTION_B | 
	                            DIRECTION_C0 | DIRECTION_C1); // all input
}

byte I8255::readPortA() {
	switch (control & MODE_A) {
	case MODEA_0:
		if (control & DIRECTION_A) {
			//input
			return interface.readA();	// input not latched
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

byte I8255::readPortB() {
	switch (control & MODE_B) {
	case MODEB_0:
		if (control & DIRECTION_B) {
			//input
			return interface.readB();	// input not latched
		} else {
			//output
			return latchPortB;		// output is latched
		}
	case MODEB_1:		// TODO but not relevant for MSX
	default:
		assert (false);
	}
}

byte I8255::readPortC() {
	byte tmp = readC1() & readC0();
	switch (control & MODE_A) {
	case MODEA_0:
		// do nothing
		break;
	case MODEA_1:		// TODO but not relevant for MSX
	case MODEA_2: case MODEA_2_:
		assert (false);
	}
	switch (control & MODE_B) {
	case MODEB_0:
		// do nothing
		break;
	case MODEB_1:		// TODO but not relevant for MSX
		assert (false);
	}
	return tmp;
}

byte I8255::readC1() {
	if (control & DIRECTION_C1) {
		//input
		return interface.readC1() << 4;	// input not latched
	} else {
		//output
		return latchPortC & 0xf0;	// output is latched
	}
}

byte I8255::readC0() {
	if (control & DIRECTION_C0) {
		//input
		return interface.readC0();		// input not latched
	} else {
		//output
		return latchPortC & 0x0f;		// output is latched
	}
}

byte I8255::readControlPort() {
	return control;
}

void I8255::writePortA(byte value) {
	switch (control & MODE_A) {
	case MODEA_0:
		// do nothing
		break;
	case MODEA_1:		// TODO but not relevant for MSX
	case MODEA_2: case MODEA_2_:
		assert (false);
	}
	outputPortA(value);
}

void I8255::writePortB(byte value) {
	switch (control & MODE_B) {
	case MODEB_0:
		// do nothing
		break;
	case MODEB_1:		// TODO but not relevant for MSX
		assert (false);
	}
	outputPortB(value);
}

void I8255::writePortC(byte value) {
	switch (control & MODE_A) {
	case MODEA_0:
		// do nothing
		break;
	case MODEA_1:		// TODO but not relevant for MSX
	case MODEA_2: case MODEA_2_:
		assert (false);
	}
	switch (control & MODE_B) {
	case MODEB_0:
		// do nothing
		break;
	case MODEB_1:		// TODO but not relevant for MSX
		assert (false);
	}
	outputPortC(value);
}

void I8255::outputPortA(byte value) {
	latchPortA = value;
	if (!(control & DIRECTION_A)) {
		//output
		interface.writeA(value);
	}
}
	
void I8255::outputPortB(byte value) {
	latchPortB = value;
	if (!(control & DIRECTION_B)) {
		//output
		interface.writeB(value);
	}
}
	
void I8255::outputPortC(byte value) {
	latchPortC = value;
	if (!(control & DIRECTION_C1)) {
		//output
		interface.writeC1(latchPortC >> 4);
	}
	if (!(control & DIRECTION_C0)) {
		//output
		interface.writeC0(latchPortC & 15);
	}
}

void I8255::writeControlPort(byte value) {
	if (value & SET_MODE) {
		// set new control mode
		control = value;
		outputPortA(latchPortA);
		outputPortB(latchPortB);
		outputPortC(latchPortC);
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
		outputPortC(latchPortC);
		// check for special (re)set commands
		// not releant for mode 0
		switch (control & MODE_A) {
		case MODEA_0:
			// do nothing
			break;
		case MODEA_1:		// TODO but not relevant for MSX
		case MODEA_2: case MODEA_2_:
			assert (false);
		}
		switch (control & MODE_B) {
		case MODEB_0:
			// do nothing
			break;
		case MODEB_1:		// TODO but not relevant for MSX
			assert (false);
		}
	}
}
	
