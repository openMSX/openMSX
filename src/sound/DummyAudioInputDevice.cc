// $Id$

#include "DummyAudioInputDevice.hh"


namespace openmsx {

DummyAudioInputDevice::DummyAudioInputDevice()
{
}

void DummyAudioInputDevice::plug(Connector* connector, const EmuTime &time)
	throw()
{
}

void DummyAudioInputDevice::unplug(const EmuTime &time)
{
}

short DummyAudioInputDevice::readSample(const EmuTime &time)
{
	return 0;	// silence
}

} // namespace openmsx
