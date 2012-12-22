// $Id$

#include "VideoSourceSetting.hh"
#include "CommandException.hh"

namespace openmsx {

VideoSourceSettingPolicy::VideoSourceSettingPolicy(const Map& map)
	: EnumSettingPolicy<VideoSource>(map)
{
}

bool VideoSourceSettingPolicy::has(VideoSource value) const
{
	return find(activeSources.begin(), activeSources.end(), value)
	       != activeSources.end();
}

void VideoSourceSettingPolicy::checkSetValue(VideoSource& value) const
{
	if (!has(value)) {
		throw CommandException("video source not available");
	}
}

VideoSource VideoSourceSettingPolicy::checkGetValue(VideoSource value) const
{
	if (has(value)) {
		return value;
	} else if (has(VIDEO_9000)) {
		// prefer video9000 over v99x8
		return VIDEO_9000;
	} else if (has(VIDEO_MSX)) {
		return VIDEO_MSX;
	} else if (has(VIDEO_GFX9000)) {
		return VIDEO_GFX9000;
	} else if (has(VIDEO_LASERDISC)) {
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
	activeSources.push_back(source);
	notify();
	notifyPropertyChange();
}

void VideoSourceSetting::unregisterVideoSource(VideoSource source)
{
	auto it = find(activeSources.begin(), activeSources.end(), source);
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
