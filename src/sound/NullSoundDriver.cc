// $Id$

#include "NullSoundDriver.hh"

namespace openmsx {

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

void NullSoundDriver::uploadBuffer(short* /*buffer*/, unsigned /*len*/)
{
}

} // namespace openmsx
