// $Id$

/*
TODO:
- Implement blinking (of page mask) in bitmap modes.
*/

#include "SDLRenderer.hh"
#include "Rasterizer.hh"
#include "RenderSettings.hh"
#include "EventDistributor.hh"
#include "Scheduler.hh"
#include "SDLVideoSystem.hh"
#include <cassert>


namespace openmsx {

SDLRenderer::SDLRenderer(
	RendererFactory::RendererID id, VDP* vdp, Rasterizer* rasterizer)
	: PixelRenderer(id, vdp)
	, powerSetting(Scheduler::instance().getPowerSetting())
{
	this->rasterizer = rasterizer;
	powerSetting.addListener(this);
}

SDLRenderer::~SDLRenderer()
{
	powerSetting.removeListener(this);
}

void SDLRenderer::update(const SettingLeafNode* setting)
{
	if (setting == &powerSetting) {
		Display::INSTANCE->setAlpha(
			rasterizer, powerSetting.getValue() ? 255 : 0);
	} else {
		PixelRenderer::update(setting);
	}
}

void SDLRenderer::reset(const EmuTime& time)
{
	PixelRenderer::reset(time);
	rasterizer->reset();
}

void SDLRenderer::finishFrame()
{
	Event* finishFrameEvent = new SimpleEvent<FINISH_FRAME_EVENT>();
	EventDistributor::instance().distributeEvent(finishFrameEvent);
}

bool SDLRenderer::checkSettings()
{
	// TODO: Move this check away from Renderer entirely?
	return PixelRenderer::checkSettings() // right renderer?
		&& Display::INSTANCE->getVideoSystem()->checkSettings();
}

void SDLRenderer::frameStart(const EmuTime& time)
{
	PixelRenderer::frameStart(time);
	rasterizer->frameStart();
}

void SDLRenderer::updateTransparency(
	bool enabled, const EmuTime& time)
{
	sync(time);
	rasterizer->setTransparency(enabled);
}

void SDLRenderer::updateForegroundColour(
	int /*colour*/, const EmuTime& time)
{
	sync(time);
}

void SDLRenderer::updateBackgroundColour(
	int colour, const EmuTime& time)
{
	sync(time);
	rasterizer->setBackgroundColour(colour);
}

void SDLRenderer::updateBlinkForegroundColour(
	int /*colour*/, const EmuTime& time)
{
	sync(time);
}

void SDLRenderer::updateBlinkBackgroundColour(
	int /*colour*/, const EmuTime& time)
{
	sync(time);
}

void SDLRenderer::updateBlinkState(
	bool /*enabled*/, const EmuTime& /*time*/)
{
	// TODO: When the sync call is enabled, the screen flashes on
	//       every call to this method.
	//       I don't know why exactly, but it's probably related to
	//       being called at frame start.
	//sync(time);
}

void SDLRenderer::updatePalette(
	int index, int grb, const EmuTime& time)
{
	sync(time);
	rasterizer->setPalette(index, grb);
}

void SDLRenderer::updateVerticalScroll(
	int /*scroll*/, const EmuTime& time)
{
	sync(time);
}

void SDLRenderer::updateHorizontalAdjust(
	int /*adjust*/, const EmuTime& time)
{
	sync(time);
}

void SDLRenderer::updateDisplayMode(
	DisplayMode mode, const EmuTime& time)
{
	sync(time, true);
	rasterizer->setDisplayMode(mode);
}

void SDLRenderer::updateNameBase(
	int /*addr*/, const EmuTime& time)
{
	sync(time);
}

void SDLRenderer::updatePatternBase(
	int /*addr*/, const EmuTime& time)
{
	sync(time);
}

void SDLRenderer::updateColourBase(
	int /*addr*/, const EmuTime& time)
{
	sync(time);
}

void SDLRenderer::updateVRAMCache(int address)
{
	rasterizer->updateVRAMCache(address);
}

void SDLRenderer::drawBorder(int fromX, int fromY, int limitX, int limitY)
{
	rasterizer->drawBorder(fromX, fromY, limitX, limitY);
}

void SDLRenderer::drawDisplay(
	int fromX, int fromY,
	int displayX, int displayY,
	int displayWidth, int displayHeight
) {
	rasterizer->drawDisplay(
		fromX, fromY, displayX, displayY, displayWidth, displayHeight );
}

void SDLRenderer::drawSprites(
	int fromX, int fromY,
	int displayX, int displayY,
	int displayWidth, int displayHeight
) {
	rasterizer->drawSprites(
		fromX, fromY, displayX, displayY, displayWidth, displayHeight );
}

} // namespace openmsx
