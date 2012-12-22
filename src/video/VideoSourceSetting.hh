// $Id$

#ifndef VIDEOSOURCESETTING_HH
#define VIDEOSOURCESETTING_HH

#include "VideoSource.hh"
#include "EnumSetting.hh"
#include <vector>

namespace openmsx {

class VideoSourceSettingPolicy : public EnumSettingPolicy<VideoSource>
{
protected:
	VideoSourceSettingPolicy(const Map& map);
	virtual void checkSetValue(VideoSource& value) const;
	VideoSource checkGetValue(VideoSource value) const;

	// TODO currently we allow duplicates, should we instead have this
	//      setting per machine?
	std::vector<VideoSource> activeSources;

private:
	bool has(VideoSource value) const;
};


class VideoSourceSetting : public SettingImpl<VideoSourceSettingPolicy>
{
public:
	explicit VideoSourceSetting(CommandController& commandController);
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
