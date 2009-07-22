// $Id$

#include "LDPixelRenderer.hh"
#include "LDRasterizer.hh"
#include "Display.hh"
#include "VideoSystem.hh"
#include "EventDistributor.hh"
#include "FinishFrameEvent.hh"
#include "MSXMotherBoard.hh"
#include "LaserdiscPlayer.hh"
#include "Reactor.hh"

namespace openmsx {

LDPixelRenderer::LDPixelRenderer(LaserdiscPlayer& ld, Display& display)
	: eventDistributor(ld.getMotherBoard().getReactor().getEventDistributor())
	, rasterizer(display.getVideoSystem().createLDRasterizer(ld))
{
}

LDPixelRenderer::~LDPixelRenderer()
{
}

void LDPixelRenderer::frameStart(EmuTime::param time)
{
	rasterizer->frameStart(time);
}

void LDPixelRenderer::frameEnd()
{
	eventDistributor.distributeEvent(
		new FinishFrameEvent(VIDEO_LASERDISC, false));
}

void LDPixelRenderer::drawBlank(int r, int g, int b )
{
	rasterizer->drawBlank(r, g, b);
}

RawFrame* LDPixelRenderer::getRawFrame()
{
	return rasterizer->getRawFrame();
}

} // namespace openmsx
