// $Id$

#include <cassert>
#include "MSXE6Timer.hh"

namespace openmsx {

MSXE6Timer::MSXE6Timer(const XMLElement& config, const EmuTime& time)
	: MSXDevice(config, time)
{
	reset(time);
}

MSXE6Timer::~MSXE6Timer()
{
}

void MSXE6Timer::reset(const EmuTime& time)
{
	reference.advance(time);
}

void MSXE6Timer::writeIO(byte /*port*/, byte /*value*/, const EmuTime& time)
{
	/*
	The Clock class rounds down time to its clock resolution.
	This is correct for the E6 timer, verified by running the following
	program in R800 mode:
		org &HC000
		di
		ld c,&HE6
	loop:
		out (&HE6),a
		in d,(c)
		jp z,loop
		ei
		ret
	It returns with D=1.
	*/
	reference.advance(time);
}

byte MSXE6Timer::readIO(byte port, const EmuTime& time)
{
	int counter = reference.getTicksTill(time);
	switch (port & 0x01) {
	case 0:
		return counter & 0xFF;
	case 1:
		return (counter >> 8) & 0xFF;
	default: // unreachable, avoid warning
		assert(false);
		return 0;
	}
}

} // namespace openmsx

