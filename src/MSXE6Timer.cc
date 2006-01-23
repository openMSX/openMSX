// $Id$

#include "MSXE6Timer.hh"

namespace openmsx {

MSXE6Timer::MSXE6Timer(MSXMotherBoard& motherBoard, const XMLElement& config,
                       const EmuTime& time)
	: MSXDevice(motherBoard, config, time)
	, reference(time)
{
	reset(time);
}

void MSXE6Timer::reset(const EmuTime& time)
{
	reference.advance(time);
}

void MSXE6Timer::writeIO(word /*port*/, byte /*value*/, const EmuTime& time)
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

byte MSXE6Timer::readIO(word port, const EmuTime& time)
{
	return peekIO(port, time);
}

byte MSXE6Timer::peekIO(word port, const EmuTime& time) const
{
	int counter = reference.getTicksTill(time);
	return (port & 1) ? ((counter >> 8) & 0xFF) : (counter & 0xFF);
}

} // namespace openmsx

