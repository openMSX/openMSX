// $Id$

#include "VideoLayer.hh"
#include "RenderSettings.hh"
#include "GlobalSettings.hh"
#include <cassert>


namespace openmsx {

VideoLayer::VideoLayer(RenderSettings::VideoSource videoSource)
	: videoSource(videoSource)
	, videoSourceSetting(*RenderSettings::instance().getVideoSource())
	, powerSetting(GlobalSettings::instance().getPowerSetting())
{
	setCoverage(getCoverage());
	setZ(getZ());
	// Register as display layer.
	Display::INSTANCE->addLayer(this);

	videoSourceSetting.addListener(this);
	powerSetting.addListener(this);
}

VideoLayer::~VideoLayer()
{
	powerSetting.removeListener(this);
	videoSourceSetting.removeListener(this);
}

void VideoLayer::update(const SettingLeafNode* setting)
{
	if (setting == &videoSourceSetting) {
		setZ(getZ());
	} else if (setting == &powerSetting) {
		setCoverage(getCoverage());
	} else {
		assert(false);
	}
}

Layer::ZIndex VideoLayer::getZ()
{
	return videoSourceSetting.getValue() == videoSource
		? Z_MSX_ACTIVE
		: Z_MSX_PASSIVE;
}

Layer::Coverage VideoLayer::getCoverage()
{
	return powerSetting.getValue()
		? COVER_FULL
		: COVER_NONE;
}

} // namespace openmsx

