// $Id$

#include "KeyClick.hh"
#include "openmsx.hh"
#include "Mixer.hh"
#include "EmuTime.hh"
#include "MSXCPU.hh"


KeyClick::KeyClick(const EmuTime &time)
{
	dac = new DACSound(15000, time);	// TODO find a good value and put it in config file
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
