// $Id: SoundDevice.hh,v 

#include "SoundDevice.hh"

SoundDevice::SoundDevice()
{
	internalMuted = false;
	userMuted = false;
	// TODO is it possible to make a default implementation
	//      to read the start volume from the config file?
}

void SoundDevice::setVolume (short newVolume)
{
	volume = newVolume;
	setInternalVolume(volume);
}

short SoundDevice::getVolume()
{
	return volume;
}

void SoundDevice::setMute (bool muted)
{
	userMuted = muted;
}

bool SoundDevice::isMuted()
{
	return userMuted;
}

void SoundDevice::setInternalMute (bool muted)
{
	internalMuted = muted;
}

bool SoundDevice::isInternalMuted()
{
	return (internalMuted || userMuted);
}
