// $Id$

/*
TODO:
- Use GL display lists for geometry speedup.
- Is it possible to combine dirtyPattern and dirtyColour into a single
  dirty array?
  Pitfalls:
  * in SCREEN1, a colour change invalidates 8 consequetive characters
    however, the code only checks dirtyPattern, because there is nothing
    to cache for the colour table (patterns are monochrome textures)
  * A12 and A11 of patternMask and colourMask may be different
    also, colourMask has A10..A6 as well
    in most realistic cases however the two will be of equal size
*/

#include "SDLGLRenderer.hh"
#include "Rasterizer.hh"
#include "VDP.hh"
#include "VDPVRAM.hh"
#include "RenderSettings.hh"
#include "EventDistributor.hh"
#include "FloatSetting.hh"
#include "VideoSystem.hh"
#include "Scheduler.hh"
#include <cassert>
#include <cmath>


namespace openmsx {

void SDLGLRenderer::finishFrame()
{
	rasterizer->frameEnd();

	Event* finishFrameEvent = new SimpleEvent<FINISH_FRAME_EVENT>();
	EventDistributor::instance().distributeEvent(finishFrameEvent);
}

SDLGLRenderer::SDLGLRenderer(
	RendererFactory::RendererID id, VDP* vdp, Rasterizer* rasterizer)
	: PixelRenderer(id, vdp)
	, powerSetting(Scheduler::instance().getPowerSetting())
{
	this->rasterizer = rasterizer;
	powerSetting.addListener(this);
}

SDLGLRenderer::~SDLGLRenderer()
{
	powerSetting.removeListener(this);
}

void SDLGLRenderer::update(const SettingLeafNode* setting)
{
	if (setting == &powerSetting) {
		Display::INSTANCE->setAlpha(
			rasterizer, powerSetting.getValue() ? 255 : 0);
	} else {
		PixelRenderer::update(setting);
	}
}

void SDLGLRenderer::reset(const EmuTime& time)
{
	PixelRenderer::reset(time);
	rasterizer->reset();
}

bool SDLGLRenderer::checkSettings() {
	// TODO: Move this check away from Renderer entirely?
	return PixelRenderer::checkSettings() // right renderer?
		&& Display::INSTANCE->getVideoSystem()->checkSettings();
}

void SDLGLRenderer::frameStart(const EmuTime& time)
{
	PixelRenderer::frameStart(time);
	rasterizer->frameStart();
}

void SDLGLRenderer::updateTransparency(
	bool enabled, const EmuTime& time)
{
	sync(time);
	rasterizer->setTransparency(enabled);
}

void SDLGLRenderer::updateForegroundColour(
	int /*colour*/, const EmuTime& time)
{
	sync(time);
}

void SDLGLRenderer::updateBackgroundColour(
	int colour, const EmuTime& time)
{
	sync(time);
	rasterizer->setBackgroundColour(colour);
}

void SDLGLRenderer::updateBlinkForegroundColour(
	int /*colour*/, const EmuTime& time)
{
	sync(time);
}

void SDLGLRenderer::updateBlinkBackgroundColour(
	int /*colour*/, const EmuTime& time)
{
	sync(time);
}

void SDLGLRenderer::updateBlinkState(
	bool /*enabled*/, const EmuTime& /*time*/)
{
	// TODO: When the sync call is enabled, the screen flashes on
	//       every call to this method.
	//       I don't know why exactly, but it's probably related to
	//       being called at frame start.
	//sync(time);
}

void SDLGLRenderer::updatePalette(
	int index, int grb, const EmuTime& time)
{
	sync(time);
	rasterizer->setPalette(index, grb);
}

void SDLGLRenderer::updateVerticalScroll(
	int /*scroll*/, const EmuTime& time)
{
	sync(time);
}

void SDLGLRenderer::updateHorizontalAdjust(
	int /*adjust*/, const EmuTime& time)
{
	sync(time);
}

void SDLGLRenderer::updateDisplayMode(
	DisplayMode mode, const EmuTime& time)
{
	sync(time, true);
	rasterizer->setDisplayMode(mode);
}

void SDLGLRenderer::updateNameBase(
	int /*addr*/, const EmuTime& time)
{
	sync(time);
}

void SDLGLRenderer::updatePatternBase(
	int /*addr*/, const EmuTime& time)
{
	sync(time);
}

void SDLGLRenderer::updateColourBase(
	int /*addr*/, const EmuTime& time)
{
	sync(time);
}

void SDLGLRenderer::updateVRAMCache(int address)
{
	rasterizer->updateVRAMCache(address);
}

void SDLGLRenderer::drawBorder(int fromX, int fromY, int limitX, int limitY)
{
	rasterizer->drawBorder(fromX, fromY, limitX, limitY);
}

void SDLGLRenderer::drawDisplay(
	int fromX, int fromY,
	int displayX, int displayY,
	int displayWidth, int displayHeight
) {
	rasterizer->drawDisplay(
		fromX, fromY, displayX, displayY, displayWidth, displayHeight );
}

void SDLGLRenderer::drawSprites(
	int fromX, int fromY,
	int displayX, int displayY,
	int displayWidth, int displayHeight
) {
	rasterizer->drawSprites(
		fromX, fromY, displayX, displayY, displayWidth, displayHeight );
}

} // namespace openmsx

