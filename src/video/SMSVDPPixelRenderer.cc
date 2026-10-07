#include "SMSVDPPixelRenderer.hh"

#include "SMSVDP.hh"
#include "SMSVDPRasterizer.hh"

#include "Display.hh"
#include "PostProcessor.hh"
#include "Reactor.hh"
#include "VideoSystem.hh"

#include "Event.hh"
#include "EventDistributor.hh"
#include "MSXMotherBoard.hh"
#include "VideoSourceSetting.hh"

namespace openmsx {

SMSVDPPixelRenderer::SMSVDPPixelRenderer(SMSVDP& vdp)
	: motherboard(vdp.getMotherBoard())
	, eventDistributor(motherboard.getReactor().getEventDistributor())
	, videoSourceSetting(motherboard.getVideoSource())
	, rasterizer(motherboard.getReactor().getDisplay()
	                 .getVideoSystem().createSMSVDPRasterizer(vdp))
{
}

SMSVDPPixelRenderer::~SMSVDPPixelRenderer() = default;

PostProcessor* SMSVDPPixelRenderer::getPostProcessor() const
{
	return rasterizer->getPostProcessor();
}

bool SMSVDPPixelRenderer::isActive() const
{
	return rasterizer->getPostProcessor()->needRender() &&
	       motherboard.isActive() &&
	       !motherboard.isFastForwarding();
}

void SMSVDPPixelRenderer::reset(EmuTime time)
{
	(void)time;
	rasterizer->reset();
}

void SMSVDPPixelRenderer::frameStart(EmuTime time)
{
	(void)time;
	if (isActive()) {
		rasterizer->frameStart();
	}
}

void SMSVDPPixelRenderer::frameEnd(EmuTime time)
{
	const bool active = isActive();
	if (active) {
		rasterizer->frameEnd(time);
	}
	if (motherboard.isActive() && !motherboard.isFastForwarding()) {
		eventDistributor.distributeEvent(FinishFrameEvent(
			rasterizer->getPostProcessor()->getVideoSource(),
			videoSourceSetting.getSource(),
			!active));
	}
}

void SMSVDPPixelRenderer::drawLine(
	unsigned y, std::span<const uint32_t, 320> pixels)
{
	rasterizer->drawLine(y, pixels);
}

} // namespace openmsx
