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
                       Display& display_)
	: videoSource(videoSource_)
	, display(display_)
	, renderSettings(display.getRenderSettings())
	, videoSourceSetting(renderSettings.getVideoSource())
	, videoSourceActivator(videoSourceSetting, videoSource)
	, powerSetting(commandController.getGlobalSettings().getPowerSetting())
{
	setCoverage(getCoverage());
	setZ(calcZ());
	display.addLayer(*this);

	videoSourceSetting.attach(*this);
	powerSetting.attach(*this);
}

VideoLayer::~VideoLayer()
{
	powerSetting.detach(*this);
	videoSourceSetting.detach(*this);

	display.removeLayer(*this);
}

VideoSource VideoLayer::getVideoSource() const
{
	return videoSource;
}

void VideoLayer::update(const Setting& setting)
{
	if (&setting == &videoSourceSetting) {
		setZ(calcZ());
	} else if (&setting == &powerSetting) {
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

