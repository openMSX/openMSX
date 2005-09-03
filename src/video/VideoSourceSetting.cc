// $Id$

#include "VideoSourceSetting.hh"

namespace openmsx {

VideoSourceSettingPolicy::VideoSourceSettingPolicy(
		CommandController& commandController,
		const std::string& name, const Map& map)
	: EnumSettingPolicy<VideoSource>(commandController, name, map)
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
	} else if (activeSources.find(VIDEO_MSX) != activeSources.end()) {
		return VIDEO_MSX;
	} else if (activeSources.find(VIDEO_MSX) != activeSources.end()) {
		return VIDEO_GFX9000;
	} else {
		// happens during loading of setting
		return value;
	}
}

static VideoSourceSetting::Map getVideoSourceMap()
{
	VideoSourceSetting::Map result;
	result["MSX"]     = VIDEO_MSX;
	result["GFX9000"] = VIDEO_GFX9000;
	return result;
}

const char* const VIDEOSOURCE = "videosource";

VideoSourceSetting::VideoSourceSetting(CommandController& commandController)
	: SettingImpl<VideoSourceSettingPolicy>(commandController,
		VIDEOSOURCE, "selects the video source to display on the screen",
		VIDEO_MSX, Setting::SAVE, VIDEOSOURCE, getVideoSourceMap())
{
}

void VideoSourceSetting::registerVideoSource(VideoSource source)
{
	activeSources.insert(source);
	notify();
}

void VideoSourceSetting::unregisterVideoSource(VideoSource source)
{
	activeSources.erase(source);
	notify();
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
