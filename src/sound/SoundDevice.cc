// $Id$

#include "SoundDevice.hh"
#include "xmlx.hh"


namespace openmsx {

SoundDevice::SoundDevice()
{
	muted = true;
}

SoundDevice::~SoundDevice()
{
}

void SoundDevice::setMute(bool muted)
{
	this->muted = muted;
}

bool SoundDevice::isMuted() const
{
	return muted;
}

void SoundDevice::registerSound(const XMLElement& config,
                                Mixer::ChannelMode mode)
{
	const XMLElement& soundConfig = config.getChild("sound");
	short volume = soundConfig.getChildDataAsInt("volume");
	if (mode != Mixer::STEREO) {
		string modeStr = soundConfig.getChildData("mode", "mono");
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
