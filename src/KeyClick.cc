// $Id: KeyClick.cc,v 

#include "KeyClick.hh"
#include "openmsx.hh"
#include "Mixer.hh"
#include "EmuTime.hh"
#include "MSXCPU.hh"


KeyClick::KeyClick()
{
	dac = new DACSound(15000);	// TODO find a good value and put it in config file
	status = false;
}

KeyClick::~KeyClick()
{
	delete dac;
}

void KeyClick::reset()
{
	setClick(false, MSXCPU::instance()->getCurrentTime());
}

void KeyClick::setClick(bool newStatus, const EmuTime &time)
{
	if (newStatus != status) {
		PRT_DEBUG("KeyClick: " << status << " time: " << time);
		status = newStatus;
		dac->writeDAC((status ? 0xff : 0x80), time);
	}
}
