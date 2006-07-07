// $Id$

#include "SoundDevice.hh"
#include "XMLElement.hh"
#include <cassert>

using std::string;

namespace openmsx {

SoundDevice::SoundDevice(Mixer& mixer_, const string& name_,
                         const string& description_)
	: mixer(mixer_), name(name_), description(description_)
	, muteCount(1), muted(true)
{
}

SoundDevice::~SoundDevice()
{
}

const std::string& SoundDevice::getName() const
{
	return name;
}

const std::string& SoundDevice::getDescription() const
{
	return description;
}

void SoundDevice::increaseMuteCount()
{
	++muteCount;
}

void SoundDevice::decreaseMuteCount()
{
	assert(isMuted());
	--muteCount;
}

void SoundDevice::setMute(bool newMuted)
{
	if (newMuted != muted) {
		muted = newMuted;
		if (muted) {
			increaseMuteCount();
		} else {
			decreaseMuteCount();
		}
	}
}

bool SoundDevice::isMuted() const
{
	return muteCount;
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
	mixer.registerSound(*this, volume, mode);
}

void SoundDevice::unregisterSound()
{
	mixer.unregisterSound(*this);
}

} // namespace openmsx
