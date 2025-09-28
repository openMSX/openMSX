#include "LDPixelRenderer.hh"

#include "Display.hh"
#include "LDRasterizer.hh"
#include "PostProcessor.hh"
#include "VideoSourceSetting.hh"
#include "VideoSystem.hh"

#include "Event.hh"
#include "EventDistributor.hh"
#include "LaserdiscPlayer.hh"
#include "MSXMotherBoard.hh"
#include "Reactor.hh"

namespace openmsx {

LDPixelRenderer::LDPixelRenderer(LaserdiscPlayer& ld, Display& display)
	: motherboard(ld.getMotherBoard())
	, eventDistributor(motherboard.getReactor().getEventDistributor())
	, rasterizer(display.getVideoSystem().createLDRasterizer(ld))
{
}

LDPixelRenderer::~LDPixelRenderer() = default;

void LDPixelRenderer::frameStart(EmuTime time)
{
	rasterizer->frameStart(time);
}

bool LDPixelRenderer::isActive() const
{
	return motherboard.isActive();
}

void LDPixelRenderer::frameEnd()
{
	eventDistributor.distributeEvent(FinishFrameEvent(
		rasterizer->getPostProcessor()->getVideoSource(),
		motherboard.getVideoSource().getSource(),
		!isActive()));
}

void LDPixelRenderer::drawBlank(int r, int g, int b)
{
	rasterizer->drawBlank(r, g, b);
}

RawFrame* LDPixelRenderer::getRawFrame()
{
	return rasterizer->getRawFrame();
}

} // namespace openmsx
