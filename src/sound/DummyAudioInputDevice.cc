// $Id$

#include "DummyAudioInputDevice.hh"


DummyAudioInputDevice::DummyAudioInputDevice()
{
}

void DummyAudioInputDevice::plug(Connector* connector, const EmuTime &time)
{
}

void DummyAudioInputDevice::unplug(const EmuTime &time)
{
}

short DummyAudioInputDevice::readSample(const EmuTime &time)
{
	return 0;	// silence
}
