#include "I8255.hh"
#include "I8255Interface.hh"
#include "serialize.hh"
#include "unreachable.hh"

namespace openmsx {

const int MODE_A       = 0x60;
const int MODEA_0      = 0x00;
const int MODEA_1      = 0x20;
const int MODEA_2      = 0x40;
const int MODEA_2_     = 0x60;
const int MODE_B       = 0x04;
const int MODEB_0      = 0x00;
const int MODEB_1      = 0x04;
const int DIRECTION_A  = 0x10;
const int DIRECTION_B  = 0x02;
const int DIRECTION_C0 = 0x01;
const int DIRECTION_C1 = 0x08;
const int SET_MODE     = 0x80;
const int BIT_NR       = 0x0E;
const int SET_RESET    = 0x01;


I8255::I8255(I8255Interface& interf, EmuTime::param time,
             StringSetting& invalidPpiModeSetting)
	: interface(interf)
	, ppiModeCallback(invalidPpiModeSetting)
{
	reset(time);
}

void I8255::reset(EmuTime::param time)
{
	latchPortA = 0;
	latchPortB = 0;
	latchPortC = 0;
	writeControlPort(SET_MODE | DIRECTION_A | DIRECTION_B |
	                            DIRECTION_C0 | DIRECTION_C1, time); // all input
}

byte I8255::read(byte port, EmuTime::param time)
{
	switch (port) {
	case 0:
		return readPortA(time);
	case 1:
		return readPortB(time);
	case 2:
		return readPortC(time);
	case 3:
		return readControlPort(time);
	default:
		UNREACHABLE;
		return 0; // avoid warning
	}
}

byte I8255::peek(byte port, EmuTime::param time) const
{
	switch (port) {
	case 0:
		return peekPortA(time);
	case 1:
		return peekPortB(time);
	case 2:
		return peekPortC(time);
	case 3:
		return readControlPort(time);
	default:
		UNREACHABLE;
		return 0; // avoid warning
	}
}

void I8255::write(byte port, byte value, EmuTime::param time)
{
	switch (port) {
	case 0:
		writePortA(value, time);
		break;
	case 1:
		writePortB(value, time);
		break;
	case 2:
		writePortC(value, time);
		break;
	case 3:
		writeControlPort(value, time);
		break;
	default:
		UNREACHABLE;
	}
}

byte I8255::readPortA(EmuTime::param time)
{
	switch (control & MODE_A) {
	case MODEA_0:
		if (control & DIRECTION_A) {
			// input
			return interface.readA(time);	// input not latched
		} else {
			// output
			return latchPortA;		// output is latched
		}
	case MODEA_1: // TODO but not relevant for MSX
	case MODEA_2: case MODEA_2_:
	default:
		return 255; // avoid warning
	}
}

byte I8255::peekPortA(EmuTime::param time) const
{
	switch (control & MODE_A) {
	case MODEA_0:
		if (control & DIRECTION_A) {
			return interface.peekA(time);	// input not latched
		} else {
			return latchPortA;		// output is latched
		}
	case MODEA_1: // TODO but not relevant for MSX
	case MODEA_2: case MODEA_2_:
	default:
		return 255;
	}
}

byte I8255::readPortB(EmuTime::param time)
{
	switch (control & MODE_B) {
	case MODEB_0:
		if (control & DIRECTION_B) {
			// input
			return interface.readB(time);	// input not latched
		} else {
			// output
			return latchPortB;		// output is latched
		}
	case MODEB_1: // TODO but not relevant for MSX
	default:
		return 255; // avoid warning
	}
}

byte I8255::peekPortB(EmuTime::param time) const
{
	switch (control & MODE_B) {
	case MODEB_0:
		if (control & DIRECTION_B) {
			return interface.peekB(time);	// input not latched
		} else {
			return latchPortB;		// output is latched
		}
	case MODEB_1: // TODO but not relevant for MSX
	default:
		return 255;
	}
}

byte I8255::readPortC(EmuTime::param time)
{
	byte tmp = readC1(time) | readC0(time);
	switch (control & MODE_A) {
	case MODEA_0:
		// do nothing
		break;
	case MODEA_1:
	case MODEA_2: case MODEA_2_:
		// TODO but not relevant for MSX
		break;
	}
	switch (control & MODE_B) {
	case MODEB_0:
		// do nothing
		break;
	case MODEB_1:
		// TODO but not relevant for MSX
		break;
	}
	return tmp;
}

byte I8255::peekPortC(EmuTime::param time) const
{
	return peekC1(time) | peekC0(time);
}

byte I8255::readC1(EmuTime::param time)
{
	if (control & DIRECTION_C1) {
		// input
		return interface.readC1(time) << 4;	// input not latched
	} else {
		// output
		return latchPortC & 0xf0;		// output is latched
	}
}

byte I8255::peekC1(EmuTime::param time) const
{
	if (control & DIRECTION_C1) {
		return interface.peekC1(time) << 4;	// input not latched
	} else {
		return latchPortC & 0xf0;		// output is latched
	}
}

byte I8255::readC0(EmuTime::param time)
{
	if (control & DIRECTION_C0) {
		// input
		return interface.readC0(time);		// input not latched
	} else {
		// output
		return latchPortC & 0x0f;		// output is latched
	}
}

byte I8255::peekC0(EmuTime::param time) const
{
	if (control & DIRECTION_C0) {
		return interface.peekC0(time);		// input not latched
	} else {
		return latchPortC & 0x0f;		// output is latched
	}
}

byte I8255::readControlPort(EmuTime::param /*time*/) const
{
	return control;
}

void I8255::writePortA(byte value, EmuTime::param time)
{
	switch (control & MODE_A) {
	case MODEA_0:
		// do nothing
		break;
	case MODEA_1:
	case MODEA_2: case MODEA_2_:
		// TODO but not relevant for MSX
		break;
	}
	outputPortA(value, time);
}

void I8255::writePortB(byte value, EmuTime::param time)
{
	switch (control & MODE_B) {
	case MODEB_0:
		// do nothing
		break;
	case MODEB_1:
		// TODO but not relevant for MSX
		break;
	}
	outputPortB(value, time);
}

void I8255::writePortC(byte value, EmuTime::param time)
{
	switch (control & MODE_A) {
	case MODEA_0:
		// do nothing
		break;
	case MODEA_1:
	case MODEA_2: case MODEA_2_:
		// TODO but not relevant for MSX
		break;
	}
	switch (control & MODE_B) {
	case MODEB_0:
		// do nothing
		break;
	case MODEB_1:
		// TODO but not relevant for MSX
		break;
	}
	outputPortC(value, time);
}

void I8255::outputPortA(byte value, EmuTime::param time)
{
	latchPortA = value;
	if (!(control & DIRECTION_A)) {
		// output
		interface.writeA(value, time);
	}
}

void I8255::outputPortB(byte value, EmuTime::param time)
{
	latchPortB = value;
	if (!(control & DIRECTION_B)) {
		// output
		interface.writeB(value, time);
	}
}

void I8255::outputPortC(byte value, EmuTime::param time)
{
	latchPortC = value;
	if (!(control & DIRECTION_C1)) {
		// output
		interface.writeC1(latchPortC >> 4, time);
	}
	if (!(control & DIRECTION_C0)) {
		// output
		interface.writeC0(latchPortC & 15, time);
	}
}

void I8255::writeControlPort(byte value, EmuTime::param time)
{
	if (value & SET_MODE) {
		// set new control mode
		control = value;
		if ((control & (MODE_A | MODE_B))) {
			ppiModeCallback.execute();
		}
		// Some PPI datasheets state that port A and C (and sometimes
		// also B) are reset to zero on a mode change. But the
		// documentation is not consistent.
		// TODO investigate this further.
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
		// not relevant for mode 0
		switch (control & MODE_A) {
		case MODEA_0:
			// do nothing
			break;
		case MODEA_1:
		case MODEA_2: case MODEA_2_:
			// TODO but not relevant for MSX
			break;
		}
		switch (control & MODE_B) {
		case MODEB_0:
			// do nothing
			break;
		case MODEB_1:
			// TODO but not relevant for MSX
			break;
		}
	}
}

// Returns the value that's current being output on port A.
// Or a floating value if port A is actually programmed as input.
byte I8255::getPortA() const
{
	byte result = latchPortA;
	if (control & DIRECTION_A) {
		// actually set as input -> return floating value
		result = 255; // real floating value not yet supported
	}
	return result;
}

byte I8255::getPortB() const
{
	byte result = latchPortB;
	if (control & DIRECTION_B) {
		// actually set as input -> return floating value
		result = 255; // real floating value not yet supported
	}
	return result;
}

byte I8255::getPortC() const
{
	byte result = latchPortC;
	if (control & DIRECTION_C0) {
		// actually set as input -> return floating value
		result |= 0x0f; // real floating value not yet supported
	}
	if (control & DIRECTION_C1) {
		// actually set as input -> return floating value
		result |= 0xf0; // real floating value not yet supported
	}
	return result;
}

template<typename Archive>
void I8255::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("latchPortA", latchPortA,
	             "latchPortB", latchPortB,
	             "latchPortC", latchPortC,
	             "control",    control);

	// note: don't write to any output ports (is handled elsewhere)
	//       don't serialize 'warningPrinted'
}
INSTANTIATE_SERIALIZE_METHODS(I8255);

} // namespace openmsx
