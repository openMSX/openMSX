// $Id$

#include "SoundDevice.hh"


namespace openmsx {

SoundDevice::SoundDevice()
{
	internalMuted = true;
	userMuted = false;
	// TODO is it possible to make a default implementation
	//      to read the start volume from the config file?
}

void SoundDevice::setMute (bool muted)
{
	userMuted = muted;
}

bool SoundDevice::isMuted() const
{
	return userMuted;
}

void SoundDevice::setInternalMute (bool muted)
{
	internalMuted = muted;
}

bool SoundDevice::isInternalMuted() const
{
	return (internalMuted || userMuted);
}

} // namespace openmsx
