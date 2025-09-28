#include "VideoLayer.hh"

#include "Display.hh"

#include "BooleanSetting.hh"
#include "Event.hh"
#include "GlobalSettings.hh"
#include "MSXEventDistributor.hh"
#include "MSXMotherBoard.hh"
#include "Reactor.hh"

#include "one_of.hh"

namespace openmsx {

VideoLayer::VideoLayer(MSXMotherBoard& motherBoard_,
                       const std::string& videoSource_)
	: Layer(Layer::Coverage::NONE, Layer::ZIndex::BACKGROUND)
	, motherBoard(motherBoard_)
	, display(motherBoard.getReactor().getDisplay())
	, videoSourceSetting(motherBoard.getVideoSource())
	, videoSourceActivator(videoSourceSetting, videoSource_)
	, powerSetting(motherBoard.getReactor().getGlobalSettings().getPowerSetting())
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

int VideoLayer::getVideoSource() const
{
	return videoSourceActivator.getID();
}
int VideoLayer::getVideoSourceSetting() const
{
	return videoSourceSetting.getSource();
}

void VideoLayer::update(const Setting& setting) noexcept
{
	if (&setting == &videoSourceSetting) {
		calcZ();
	} else if (&setting == &powerSetting) {
		calcCoverage();
	}
}

void VideoLayer::calcZ()
{
	setZ((videoSourceSetting.getSource() == getVideoSource())
		? ZIndex::MSX_ACTIVE
		: ZIndex::MSX_PASSIVE);
}

void VideoLayer::calcCoverage()
{
	auto cov = (!powerSetting.getBoolean() || !motherBoard.isActive())
	         ? Coverage::NONE
	         : Coverage::FULL;
	setCoverage(cov);
}

void VideoLayer::signalMSXEvent(const Event& event,
                                EmuTime /*time*/) noexcept
{
	if (getType(event) == one_of(EventType::MACHINE_ACTIVATED,
		                     EventType::MACHINE_DEACTIVATED)) {
		calcCoverage();
	}
}

bool VideoLayer::needRender() const
{
	// Either when this layer itself is selected or when the video9000
	// layer is selected and this layer is needed to render a
	// (superimposed) image.
	int current = videoSourceSetting.getSource();
	return (current == getVideoSource()) ||
	      ((current == video9000Source) && (activeVideo9000 != Video9000Active::NO));
}

bool VideoLayer::needRecord() const
{
	// Either when this layer itself is selected or when the video9000
	// layer is selected and this layer is the front layer of a
	// (superimposed) image
	int current = videoSourceSetting.getSource();
	return (current == getVideoSource()) ||
	      ((current == video9000Source) && (activeVideo9000 == Video9000Active::FRONT));
}

} // namespace openmsx
