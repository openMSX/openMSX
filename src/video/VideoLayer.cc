// $Id$

#include "VideoLayer.hh"
#include "RenderSettings.hh"
#include "Display.hh"
#include "CommandController.hh"
#include "GlobalSettings.hh"
#include "BooleanSetting.hh"
#include "VideoSourceSetting.hh"
#include "MSXEventDistributor.hh"
#include "MSXMotherBoard.hh"
#include "Event.hh"
#include <cassert>

namespace openmsx {

VideoLayer::VideoLayer(MSXMotherBoard& motherBoard_,
                       VideoSource videoSource_,
                       Display& display_)
	: motherBoard(motherBoard_)
	, display(display_)
	, renderSettings(display.getRenderSettings())
	, videoSourceSetting(renderSettings.getVideoSource())
	, videoSourceActivator(new VideoSourceActivator(
              videoSourceSetting, videoSource_))
	, powerSetting(motherBoard.getCommandController().
	                   getGlobalSettings().getPowerSetting())
	, videoSource(videoSource_)
{
	calcCoverage();
	calcZ();
	display.addLayer(*this);

	videoSourceSetting.attach(*this);
	powerSetting.attach(*this);
	motherBoard.getMSXEventDistributor().registerEventListener(*this);
}

VideoLayer::~VideoLayer()
{
	motherBoard.getMSXEventDistributor().unregisterEventListener(*this);
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
		calcZ();
	} else if (&setting == &powerSetting) {
		calcCoverage();
	}
}

void VideoLayer::calcZ()
{
	setZ((renderSettings.getVideoSource().getValue() == videoSource)
		? Z_MSX_ACTIVE
		: Z_MSX_PASSIVE);
}

void VideoLayer::calcCoverage()
{
	setCoverage((powerSetting.getValue() && motherBoard.isActive())
		? COVER_FULL
		: COVER_NONE);
}

void VideoLayer::signalEvent(shared_ptr<const Event> event, EmuTime::param /*time*/)
{
	if ((event->getType() == OPENMSX_MACHINE_ACTIVATED) ||
	    (event->getType() == OPENMSX_MACHINE_DEACTIVATED)) {
		calcCoverage();
	}
}

} // namespace openmsx
