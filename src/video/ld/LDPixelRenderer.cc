// $Id$

/*
TODO:
- Implement blinking (of page mask) in bitmap modes.
*/

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

void LDPixelRenderer::frameEnd(EmuTime::param time)
{
	eventDistributor.distributeEvent(
		new FinishFrameEvent(VIDEO_LASERDISC, false));
}

void LDPixelRenderer::drawBlank(int r, int g, int b )
{
	rasterizer->drawBlank(r, g, b);
}

void LDPixelRenderer::drawBitmap(const byte* frame)
{
	rasterizer->drawBitmap(frame);
}

} // namespace openmsx
