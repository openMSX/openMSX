// $Id$

/*
TODO:
- Implement blinking (of page mask) in bitmap modes.
*/

#include "LDPixelRenderer.hh"
#include "LDRasterizer.hh"
#include "Display.hh"
#include "VideoSystem.hh"
#include "RenderSettings.hh"
#include "IntegerSetting.hh"
#include "BooleanSetting.hh"
#include "EnumSetting.hh"
#include "SpriteChecker.hh"
#include "EventDistributor.hh"
#include "FinishFrameEvent.hh"
#include "RealTime.hh"
#include "MSXMotherBoard.hh"
#include "LaserdiscPlayer.hh"
#include "Timer.hh"
#include "Reactor.hh"

#include <algorithm>
#include <cassert>

namespace openmsx {

LDPixelRenderer::LDPixelRenderer(LaserdiscPlayer& ld, Display& display)
	: laserdiscPlayer(ld)
	, eventDistributor(ld.getMotherBoard().getReactor().getEventDistributor())
	, realTime(ld.getMotherBoard().getRealTime())
	, renderSettings(display.getRenderSettings())
	, rasterizer(display.getVideoSystem().createLDRasterizer(ld))
{
	rasterizer->reset();
	renderFrame = true;

	renderSettings.getMaxFrameSkip().attach(*this);
	renderSettings.getMinFrameSkip().attach(*this);
}

LDPixelRenderer::~LDPixelRenderer()
{
	renderSettings.getMinFrameSkip().detach(*this);
	renderSettings.getMaxFrameSkip().detach(*this);
}

void LDPixelRenderer::reset(EmuTime::param time)
{
	rasterizer->reset();
	frameStart(time);
}

void LDPixelRenderer::frameStart(EmuTime::param time)
{
	rasterizer->frameStart(time);

	accuracy = renderSettings.getAccuracy().getValue();
}

void LDPixelRenderer::frameEnd(EmuTime::param time)
{
	if (renderFrame) {
		// Render changes from this last frame.
		sync(time, true);

		rasterizer->frameEnd();
	}
	eventDistributor.distributeEvent(
		new FinishFrameEvent(VIDEO_LASERDISC, false));
}

void LDPixelRenderer::sync(EmuTime::param time, bool force)
{
	if (!renderFrame) return;

	// Synchronisation is done in two phases:
	// 1. update VRAM
	// 2. update other subsystems
	// Note that as part of step 1, type 2 updates can be triggered.
	// Executing step 2 takes care of the subsystem changes that occur
	// after the last VRAM update.
	// This scheme makes sure type 2 routines such as renderUntil and
	// checkUntil are not re-entered, which was causing major pain in
	// the past.
	// TODO: I wonder if it's possible to enforce this synchronisation
	//       scheme at a higher level. Probably. But how...
	//if ((frameSkipCounter == 0) && TODO
	if (accuracy != RenderSettings::ACC_SCREEN || force) {
		renderUntil(time);
	}
}

void LDPixelRenderer::renderUntil(EmuTime::param /*time*/)
{
}

void LDPixelRenderer::drawBlank(int r, int g, int b )
{
	rasterizer->drawBlank(r, g, b);
}

void LDPixelRenderer::drawBitmap(byte *frame)
{
	rasterizer->drawBitmap(frame);
}

void LDPixelRenderer::update(const Setting& setting)
{
	if (&setting == &renderSettings.getMinFrameSkip()
	|| &setting == &renderSettings.getMaxFrameSkip() ) {
		// Force drawing of frame.
		frameSkipCounter = 999;
	} else {
		assert(false);
	}
}

} // namespace openmsx
