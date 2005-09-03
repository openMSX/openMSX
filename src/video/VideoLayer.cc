// $Id$

#include "VideoLayer.hh"
#include "RenderSettings.hh"
#include "Display.hh"
#include "CommandController.hh"
#include "GlobalSettings.hh"
#include "BooleanSetting.hh"
#include <cassert>

namespace openmsx {

VideoLayer::VideoLayer(VideoSource videoSource_,
                       CommandController& commandController,
                       RenderSettings& renderSettings_,
                       Display& display)
	: videoSource(videoSource_)
	, renderSettings(renderSettings_)
	, videoSourceSetting(renderSettings.getVideoSource())
	, videoSourceActivator(videoSourceSetting, videoSource)
	, powerSetting(commandController.getGlobalSettings().getPowerSetting())
{
	setCoverage(getCoverage());
	setZ(calcZ());
	// Register as display layer.
	display.addLayer(this);

	videoSourceSetting.addListener(this);
	powerSetting.addListener(this);
}

VideoLayer::~VideoLayer()
{
	powerSetting.removeListener(this);
	videoSourceSetting.removeListener(this);
}

VideoSource VideoLayer::getVideoSource() const
{
	return videoSource;
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
	return renderSettings.getVideoSource().getValue() == videoSource
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

