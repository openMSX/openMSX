// $Id$

#include "NullSoundDriver.hh"

namespace openmsx {

void NullSoundDriver::lock()
{
}

void NullSoundDriver::unlock()
{
}

void NullSoundDriver::mute()
{
}

void NullSoundDriver::unmute()
{
}

unsigned NullSoundDriver::getFrequency() const
{
	return 44100;
}

unsigned NullSoundDriver::getSamples() const
{
	return 0;
}

double NullSoundDriver::uploadBuffer(short* /*buffer*/, unsigned /*len*/)
{
	return 1.0;
}

} // namespace openmsx
