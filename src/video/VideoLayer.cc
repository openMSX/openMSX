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
	// Register as display layer.
	Display::INSTANCE->addLayer(this, getZ());
	Display::INSTANCE->setCoverage(this, getCoverage());

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
		Display::INSTANCE->setZ(this, getZ());
	} else if (setting == &powerSetting) {
		Display::INSTANCE->setCoverage(this, getCoverage());
	} else {
		assert(false);
	}
}

int VideoLayer::getZ()
{
	return videoSourceSetting.getValue() == videoSource
		? Display::Z_MSX_ACTIVE
		: Display::Z_MSX_PASSIVE;
}

Display::Coverage VideoLayer::getCoverage()
{
	return powerSetting.getValue()
		? Display::COVER_FULL
		: Display::COVER_NONE;
}

} // namespace openmsx

