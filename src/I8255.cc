// $Id$

#include "I8255.hh"
#include <cassert>


I8255::I8255(I8255Interface &interf, const EmuTime &time) : interface(interf)
{
	reset(time);
}

I8255::~I8255()
{
}

void I8255::reset(const EmuTime &time)
{
	latchPortA = 0;
	latchPortB = 0;
	latchPortC = 0;
	writeControlPort(SET_MODE | DIRECTION_A | DIRECTION_B | 
	                            DIRECTION_C0 | DIRECTION_C1, time); // all input
}

byte I8255::readPortA(const EmuTime &time) {
	switch (control & MODE_A) {
	case MODEA_0:
		if (control & DIRECTION_A) {
			//input
			return interface.readA(time);	// input not latched
		} else {
			//output
			return latchPortA;		// output is latched
		}
	case MODEA_1:		//TODO but not relevant for MSX
	case MODEA_2: case MODEA_2_:
	default:
		assert (false);
		return 255;	// avoid warning
	}
}

byte I8255::readPortB(const EmuTime &time) {
	switch (control & MODE_B) {
	case MODEB_0:
		if (control & DIRECTION_B) {
			//input
			return interface.readB(time);	// input not latched
		} else {
			//output
			return latchPortB;		// output is latched
		}
	case MODEB_1:		// TODO but not relevant for MSX
	default:
		assert (false);
		return 255;	// avoid warning
	}
}

byte I8255::readPortC(const EmuTime &time) {
	byte tmp = readC1(time) | readC0(time);
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

byte I8255::readC1(const EmuTime &time) {
	if (control & DIRECTION_C1) {
		//input
		return interface.readC1(time) << 4;	// input not latched
	} else {
		//output
		return latchPortC & 0xf0;	// output is latched
	}
}

byte I8255::readC0(const EmuTime &time) {
	if (control & DIRECTION_C0) {
		//input
		return interface.readC0(time);		// input not latched
	} else {
		//output
		return latchPortC & 0x0f;		// output is latched
	}
}

byte I8255::readControlPort(const EmuTime &time) {
	return control;
}

void I8255::writePortA(byte value, const EmuTime &time) {
	switch (control & MODE_A) {
	case MODEA_0:
		// do nothing
		break;
	case MODEA_1:		// TODO but not relevant for MSX
	case MODEA_2: case MODEA_2_:
		assert (false);
	}
	outputPortA(value, time);
}

void I8255::writePortB(byte value, const EmuTime &time) {
	switch (control & MODE_B) {
	case MODEB_0:
		// do nothing
		break;
	case MODEB_1:		// TODO but not relevant for MSX
		assert (false);
	}
	outputPortB(value, time);
}

void I8255::writePortC(byte value, const EmuTime &time) {
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
	outputPortC(value, time);
}

void I8255::outputPortA(byte value, const EmuTime &time) {
	latchPortA = value;
	if (!(control & DIRECTION_A)) {
		//output
		interface.writeA(value, time);
	}
}
	
void I8255::outputPortB(byte value, const EmuTime &time) {
	latchPortB = value;
	if (!(control & DIRECTION_B)) {
		//output
		interface.writeB(value, time);
	}
}
	
void I8255::outputPortC(byte value, const EmuTime &time) {
	latchPortC = value;
	if (!(control & DIRECTION_C1)) {
		//output
		interface.writeC1(latchPortC >> 4, time);
	}
	if (!(control & DIRECTION_C0)) {
		//output
		interface.writeC0(latchPortC & 15, time);
	}
}

void I8255::writeControlPort(byte value, const EmuTime &time) {
	if (value & SET_MODE) {
		// set new control mode
		control = value;
		outputPortA(latchPortA, time);
		outputPortB(latchPortB, time);
		outputPortC(latchPortC, time);
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
		outputPortC(latchPortC, time);
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
	
