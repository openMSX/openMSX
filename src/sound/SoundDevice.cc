// $Id$

#include "SoundDevice.hh"
#include "xmlx.hh"

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

void SoundDevice::registerSound(const XMLElement& config,
                                    Mixer::ChannelMode mode)
{
	short volume = config.getChildDataAsInt("volume");
	if (mode != Mixer::STEREO) {
		string modeStr = config.getChildData("mode", "mono");
		if (modeStr == "left") {
			mode = Mixer::MONO_LEFT;
		} else if (modeStr == "right") {
			mode = Mixer::MONO_RIGHT;
		} else {
			mode = Mixer::MONO;
		}
	}
	unsigned samples = Mixer::instance().registerSound(*this, volume, mode);
	unsigned bufSize = (mode == Mixer::STEREO) ? 2 * samples : samples;
	buffer = new int[bufSize];
}

void SoundDevice::unregisterSound()
{
	Mixer::instance().unregisterSound(*this);
	delete[] buffer;
}

} // namespace openmsx
