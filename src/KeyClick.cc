// $Id: KeyClick.cc,v 

#include "KeyClick.hh"
#include "openmsx.hh"
#include "Mixer.hh"
#include "emutime.hh"


KeyClick::KeyClick()
{
}

KeyClick::~KeyClick()
{
}

void KeyClick::init()
{
	PRT_DEBUG("Initializing a KeyClick Device");
	
	bufSize = Mixer::instance()->registerSound(this);
	buffer = new int[bufSize];	// TODO fix race

	setVolume(20000); //TODO find a good value and put it in config file
	
	reset();
}

void KeyClick::reset()
{
	Emutime dummy;
	setClick(false, dummy);
}

void KeyClick::setClick(bool status, const Emutime &time)
{
	PRT_DEBUG("KeyClick: " << status << " time: " << time);
	Mixer::instance()->updateStream(time);
	setInternalMute(!status);	// mute if low (=false)
}

void KeyClick::setInternalVolume(short newVolume)
{
	for (int i=0; i<bufSize; i++) {
		buffer[i] = newVolume;
	}
}

void KeyClick::setSampleRate(int sampleRate)
{
	// ignore sample rate
}

int* KeyClick::updateBuffer(int length)
{
	PRT_DEBUG("KeyClick update buffer");
	return buffer;
}
