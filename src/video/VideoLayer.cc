// $Id$

#include "VideoLayer.hh"
#include "RenderSettings.hh"
#include "Display.hh"
#include "GlobalSettings.hh"
#include <cassert>

namespace openmsx {

VideoLayer::VideoLayer(VideoSource videoSource_)
	: videoSource(videoSource_)
	, videoSourceSetting(*RenderSettings::instance().getVideoSource())
	, videoSourceActivator(videoSourceSetting, videoSource)
	, powerSetting(GlobalSettings::instance().getPowerSetting())
{
	setCoverage(getCoverage());
	setZ(calcZ());
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

void VideoLayer::update(const Setting* setting)
{
	if (setting == &videoSourceSetting) {
		setZ(calcZ());
	} else if (setting == &powerSetting) {
		setCoverage(getCoverage());
	} else {
		assert(false);
	}
}

Layer::ZIndex VideoLayer::calcZ()
{
	return RenderSettings::instance().getVideoSource()->getValue() == videoSource
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

