// $Id$

#include "KeyClick.hh"
#include "DACSound.hh"


KeyClick::KeyClick(short volume, const EmuTime &time)
{
	dac = new DACSound(volume, 51000, time);	// ~ 70 cycles ~ 51kHz
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
		PRT_DEBUG("KeyClick: " << status << " time: " << time);
		status = newStatus;
		dac->writeDAC((status ? 0xff : 0x80), time);
	}
}
