// $Id$

#include "KeyClick.hh"
#include "DACSound8U.hh"


KeyClick::KeyClick(short volume, const EmuTime &time)
{
	dac = new DACSound8U(volume, time);
	status = false;
}

KeyClick::~KeyClick()
{
	delete dac;
}

void KeyClick::reset(const EmuTime &time)
{
	setClick(false, time);
}

void KeyClick::setClick(bool newStatus, const EmuTime &time)
{
	if (newStatus != status) {
		status = newStatus;
		dac->writeDAC((status ? 0xff : 0x80), time);
	}
}
