// $Id: KeyClick.cc,v 

#include "KeyClick.hh"
#include "openmsx.hh"
#include "Mixer.hh"
#include "emutime.hh"


KeyClick::KeyClick()
{
	dac = new DACSound(20000);	// TODO find a good value and put it in config file
}

KeyClick::~KeyClick()
{
	delete dac;
}

void KeyClick::init()
{
	PRT_DEBUG("Initializing a KeyClick Device");
	reset();
}

void KeyClick::reset()
{
	Emutime dummy;
	setClick(false, dummy);
}

void KeyClick::setClick(bool newStatus, const Emutime &time)
{
	PRT_DEBUG("KeyClick: " << status << " time: " << time);
	if (newStatus != status) {
		status = newStatus;
		dac->writeDAC(status ? 0x80 : 0xff, time);
	}
}
