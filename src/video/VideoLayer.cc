// $Id$

#include "VideoLayer.hh"
#include "RenderSettings.hh"
#include "Display.hh"
#include "Reactor.hh"
#include "GlobalSettings.hh"
#include "BooleanSetting.hh"
#include "VideoSourceSetting.hh"
#include "MSXEventDistributor.hh"
#include "MSXMotherBoard.hh"
#include "Event.hh"
#include "openmsx.hh"
#include "memory.hh"
#include <cassert>

namespace openmsx {

VideoLayer::VideoLayer(MSXMotherBoard& motherBoard_,
                       const std::string& videoSource_)
	: motherBoard(motherBoard_)
	, display(motherBoard.getReactor().getDisplay())
	, renderSettings(display.getRenderSettings())
	, videoSourceSetting(motherBoard.getVideoSource())
	, videoSourceActivator(make_unique<VideoSourceActivator>(
		videoSourceSetting, videoSource_))
	, powerSetting(motherBoard.getReactor().getGlobalSettings().getPowerSetting())
	, video9000Source(0)
	, activeVideo9000(INACTIVE)
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
	PRT_DEBUG("Destructing VideoLayer...");
	motherBoard.getMSXEventDistributor().unregisterEventListener(*this);
	powerSetting.detach(*this);
	videoSourceSetting.detach(*this);

	display.removeLayer(*this);
	PRT_DEBUG("Destructing VideoLayer... DONE!");
}

int VideoLayer::getVideoSource() const
{
	return videoSourceActivator->getID();
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
	setZ((videoSourceSetting.getValue() == getVideoSource())
		? Z_MSX_ACTIVE
		: Z_MSX_PASSIVE);
}

void VideoLayer::calcCoverage()
{
	Coverage coverage;

	if (!powerSetting.getValue() || !motherBoard.isActive()) {
		coverage = COVER_NONE;
	} else {
		coverage = COVER_FULL;
	}

	setCoverage(coverage);
}

void VideoLayer::signalEvent(const std::shared_ptr<const Event>& event,
                             EmuTime::param /*time*/)
{
	if ((event->getType() == OPENMSX_MACHINE_ACTIVATED) ||
	    (event->getType() == OPENMSX_MACHINE_DEACTIVATED)) {
		calcCoverage();
	}
}

bool VideoLayer::needRender() const
{
	// Either when this layer itself is selected or when the video9000
	// layer is selected and this layer is needed to render a
	// (superimposed) image.
	int current = videoSourceSetting.getValue();
	return (current == getVideoSource()) ||
	      ((current == video9000Source) && (activeVideo9000 != INACTIVE));
}

bool VideoLayer::needRecord() const
{
	// Either when this layer itself is selected or when the video9000
	// layer is selected and this layer is the front layer of a
	// (superimposed) image
	int current = videoSourceSetting.getValue();
	return (current == getVideoSource()) ||
	      ((current == video9000Source) && (activeVideo9000 == ACTIVE_FRONT));
}

} // namespace openmsx
