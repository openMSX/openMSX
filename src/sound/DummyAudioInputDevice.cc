// $Id$

#include "DummyAudioInputDevice.hh"


namespace openmsx {

DummyAudioInputDevice::DummyAudioInputDevice()
{
}

const string& DummyAudioInputDevice::getDescription() const
{
	static const string EMPTY;
	return EMPTY;
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
