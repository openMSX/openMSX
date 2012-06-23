// $Id$

#include "VideoSourceSetting.hh"
#include "CommandException.hh"

namespace openmsx {

VideoSourceSettingPolicy::VideoSourceSettingPolicy(const Map& map)
	: EnumSettingPolicy<VideoSource>(map)
{
}

void VideoSourceSettingPolicy::checkSetValue(VideoSource& value) const
{
	if (activeSources.find(value) == activeSources.end()) {
		throw CommandException("video source not available");
	}
}

VideoSource VideoSourceSettingPolicy::checkGetValue(VideoSource value) const
{
	if (activeSources.find(value) != activeSources.end()) {
		return value;
	} else if (activeSources.find(VIDEO_9000) != activeSources.end()) {
		// prefer video9000 over v99x8
		return VIDEO_9000;
	} else if (activeSources.find(VIDEO_MSX) != activeSources.end()) {
		return VIDEO_MSX;
	} else if (activeSources.find(VIDEO_GFX9000) != activeSources.end()) {
		return VIDEO_GFX9000;
	} else if (activeSources.find(VIDEO_LASERDISC) != activeSources.end()) {
		return VIDEO_LASERDISC;
	} else {
		// happens during loading of setting
		return value;
	}
}

static VideoSourceSetting::Map getVideoSourceMap()
{
	VideoSourceSetting::Map result;
	result["MSX"]       = VIDEO_MSX;
	result["GFX9000"]   = VIDEO_GFX9000;
	result["Video9000"] = VIDEO_9000;
	result["Laserdisc"] = VIDEO_LASERDISC;
	return result;
}

const char* const VIDEOSOURCE = "videosource";

VideoSourceSetting::VideoSourceSetting(CommandController& commandController)
	: SettingImpl<VideoSourceSettingPolicy>(commandController,
		VIDEOSOURCE, "selects the video source to display on the screen",
		VIDEO_9000, Setting::DONT_SAVE, getVideoSourceMap())
{
}

void VideoSourceSetting::registerVideoSource(VideoSource source)
{
	activeSources.insert(source);
	notify();
	notifyPropertyChange();
}

void VideoSourceSetting::unregisterVideoSource(VideoSource source)
{
	ActiveSources::iterator it = activeSources.find(source);
	assert(it != activeSources.end());
	activeSources.erase(it);
	notify();
	notifyPropertyChange();
}


VideoSourceActivator::VideoSourceActivator(
	VideoSourceSetting& setting_, VideoSource source_)
	: setting(setting_), source(source_)
{
	setting.registerVideoSource(source);
}

VideoSourceActivator::~VideoSourceActivator()
{
	setting.unregisterVideoSource(source);
}

} // namespace openmsx
