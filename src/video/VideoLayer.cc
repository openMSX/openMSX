#include "VideoLayer.hh"

#include "Display.hh"

#include "BooleanSetting.hh"
#include "GlobalSettings.hh"
#include "MSXMotherBoard.hh"
#include "Reactor.hh"

namespace openmsx {

VideoLayer::VideoLayer(MSXMotherBoard& motherBoard_,
                       const std::string& videoSource_)
	: motherBoard(motherBoard_)
	, display(motherBoard.getReactor().getDisplay())
	, videoSourceSetting(motherBoard.getVideoSource())
	, videoSourceActivator(videoSourceSetting, videoSource_)
	, powerSetting(motherBoard.getReactor().getGlobalSettings().getPowerSetting())
{
	display.addVideoLayer(*this);
}

VideoLayer::~VideoLayer()
{
	display.removeVideoLayer(*this);
}

int VideoLayer::getVideoSource() const
{
	return videoSourceActivator.getID();
}
int VideoLayer::getVideoSourceSetting() const
{
	return videoSourceSetting.getSource();
}

bool VideoLayer::isActive() const
{
	return getVideoSourceSetting() == getVideoSource()
	    && powerSetting.getBoolean()
	    && motherBoard.isActive();
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
