// $Id$

#ifndef VIDEOSOURCESETTING_HH
#define VIDEOSOURCESETTING_HH

#include "EnumSetting.hh"
#include <set>

namespace openmsx {

/** Video sources: devices which produce a video stream.
  */
enum VideoSource { VIDEO_MSX, VIDEO_GFX9000 };


class VideoSourceSettingPolicy : public EnumSettingPolicy<VideoSource>
{
protected:
	VideoSourceSettingPolicy(CommandController& commandController,
	                         const std::string& name, const Map& map);
	virtual void checkSetValue(VideoSource& value) const;
	VideoSource checkGetValue(VideoSource value) const;

	std::set<VideoSource> activeSources;
};


class VideoSourceSetting : public SettingImpl<VideoSourceSettingPolicy>
{
public:
	VideoSourceSetting(CommandController& commandController);
	void registerVideoSource(VideoSource source);
	void unregisterVideoSource(VideoSource source);
};

class VideoSourceActivator
{
public:
	VideoSourceActivator(VideoSourceSetting& setting, VideoSource source);
	~VideoSourceActivator();
private:
	VideoSourceSetting& setting;
	VideoSource source;
};

} // namespace openmsx

#endif
