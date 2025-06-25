/*
TODO:
- Implement blinking (of page mask) in bitmap modes.
*/

#include "PixelRenderer.hh"
#include "Rasterizer.hh"
#include "PostProcessor.hh"
#include "Display.hh"
#include "VideoSystem.hh"
#include "RenderSettings.hh"
#include "VideoSourceSetting.hh"
#include "IntegerSetting.hh"
#include "VDP.hh"
#include "VDPVRAM.hh"
#include "SpriteChecker.hh"
#include "EventDistributor.hh"
#include "Event.hh"
#include "RealTime.hh"
#include "SpeedManager.hh"
#include "ThrottleManager.hh"
#include "GlobalSettings.hh"
#include "MSXMotherBoard.hh"
#include "Reactor.hh"
#include "Timer.hh"
#include "narrow.hh"
#include "one_of.hh"
#include "unreachable.hh"
#include <algorithm>
#include <cassert>
#include <cmath>

namespace openmsx {

void PixelRenderer::draw(
	int startX, int startY, int endX, int endY, DrawType drawType, bool atEnd)
{
	if (drawType == DRAW_BORDER) {
		rasterizer->drawBorder(startX, startY, endX, endY);
	} else {
		assert(drawType == DRAW_DISPLAY);

		// Calculate display coordinates.
		int zero = vdp.getLineZero();
		int displayX = (startX - vdp.getLeftSprites()) / 2;
		int displayY = startY - zero;
		if (!vdp.getDisplayMode().isTextMode()) {
			displayY += vdp.getVerticalScroll();
		} else {
			// this is not what the real VDP does, but it is good
			// enough for "Boring scroll" demo part of "Relax"
			displayY = (displayY & 7) | (textModeCounter * 8);
			if (atEnd && (drawType == DRAW_DISPLAY)) {
				int low  = std::max(0, (startY - zero)) / 8;
				int high = std::max(0, (endY   - zero)) / 8;
				textModeCounter += (high - low);
			}
		}

		displayY &= 255; // Page wrap.
		int displayWidth = (endX - (startX & ~1)) / 2;
		int displayHeight = endY - startY;

		assert(0 <= displayX);
		assert(displayX + displayWidth <= 512);

		rasterizer->drawDisplay(
			startX, startY,
			displayX - vdp.getHorizontalScrollLow() * 2, displayY,
			displayWidth, displayHeight
			);
		if (vdp.spritesEnabled() && !renderSettings.getDisableSprites()) {
			rasterizer->drawSprites(
				startX, startY,
				displayX / 2, displayY,
				(displayWidth + 1) / 2, displayHeight);
		}
	}
}

void PixelRenderer::subdivide(
	int startX, int startY, int endX, int endY, int clipL, int clipR,
	DrawType drawType)
{
	// Partial first line.
	if (startX > clipL) {
		bool atEnd = (startY != endY) || (endX >= clipR);
		if (startX < clipR) {
			draw(startX, startY, (atEnd ? clipR : endX),
			     startY + 1, drawType, atEnd);
		}
		if (startY == endY) return;
		startY++;
	}
	// Partial last line.
	bool drawLast = false;
	if (endX >= clipR) {
		endY++;
	} else if (endX > clipL) {
		drawLast = true;
	}
	// Full middle lines.
	if (startY < endY) {
		draw(clipL, startY, clipR, endY, drawType, true);
	}
	// Actually draw last line if necessary.
	// The point of keeping top-to-bottom draw order is that it increases
	// the locality of memory references, which generally improves cache
	// hit rates.
	if (drawLast) draw(clipL, endY, endX, endY + 1, drawType, false);
}

PixelRenderer::PixelRenderer(VDP& vdp_, Display& display)
	: vdp(vdp_), vram(vdp.getVRAM())
	, eventDistributor(vdp.getReactor().getEventDistributor())
	, realTime(vdp.getMotherBoard().getRealTime())
	, speedManager(
		vdp.getReactor().getGlobalSettings().getSpeedManager())
	, throttleManager(
		vdp.getReactor().getGlobalSettings().getThrottleManager())
	, renderSettings(display.getRenderSettings())
	, videoSourceSetting(vdp.getMotherBoard().getVideoSource())
	, spriteChecker(vdp.getSpriteChecker())
	, rasterizer(display.getVideoSystem().createRasterizer(vdp))
{
	// In case of loadstate we can't yet query any state from the VDP
	// (because that object is not yet fully deserialized). But
	// VDP::serialize() will call Renderer::reInit() again when it is
	// safe to query.
	reInit();

	renderSettings.getMaxFrameSkipSetting().attach(*this);
	renderSettings.getMinFrameSkipSetting().attach(*this);
}

PixelRenderer::~PixelRenderer()
{
	renderSettings.getMinFrameSkipSetting().detach(*this);
	renderSettings.getMaxFrameSkipSetting().detach(*this);
}

PostProcessor* PixelRenderer::getPostProcessor() const
{
	return rasterizer->getPostProcessor();
}

void PixelRenderer::reInit()
{
	// Don't draw before frameStart() is called.
	// This for example can happen after a loadstate or after switching
	// renderer in the middle of a frame.
	renderFrame = false;
	paintFrame = false;

	rasterizer->reset();
	displayEnabled = vdp.isDisplayEnabled();
}

void PixelRenderer::updateDisplayEnabled(bool enabled, EmuTime time)
{
	sync(time, true);
	displayEnabled = enabled;
}

void PixelRenderer::frameStart(EmuTime time)
{
	if (!rasterizer->isActive()) {
		frameSkipCounter = 999.0f;
		renderFrame = false;
		prevRenderFrame = false;
		paintFrame = false;
		return;
	}

	prevRenderFrame = renderFrame;
	if (vdp.isInterlaced() && renderSettings.getDeinterlace()
			&& vdp.getEvenOdd() && vdp.isEvenOddEnabled()) {
		// Deinterlaced odd frame: do same as even frame.
		paintFrame = prevRenderFrame;
	} else if (throttleManager.isThrottled()) {
		// Note: min/maxFrameSkip control the number of skipped frames, but
		//       for every series of skipped frames there is also one painted
		//       frame, so our boundary checks are offset by one.
		auto counter = narrow_cast<int>(frameSkipCounter);
		if (counter < renderSettings.getMinFrameSkip()) {
			paintFrame = false;
		} else if (counter > renderSettings.getMaxFrameSkip()) {
			paintFrame = true;
		} else {
			paintFrame = realTime.timeLeft(
				unsigned(finishFrameDuration), time);
		}
		frameSkipCounter += 1.0f / float(speedManager.getSpeed());
	} else  {
		// We need to render a frame every now and then,
		// to show the user what is happening.
		paintFrame = (Timer::getTime() - lastPaintTime) >= 100000; // 10 fps
	}

	if (paintFrame) {
		frameSkipCounter = std::remainder(frameSkipCounter, 1.0f);
	} else if (!rasterizer->isRecording()) {
		renderFrame = false;
		return;
	}
	renderFrame = true;

	rasterizer->frameStart(time);

	accuracy = renderSettings.getAccuracy();

	nextX = 0;
	nextY = 0;
	// This is not what the real VDP does, but it is good enough
	// for the "Boring scroll" demo part of ANMA's "Relax" demo.
	textModeCounter = 0;
}

void PixelRenderer::frameEnd(EmuTime time)
{
	if (renderFrame) {
		// Render changes from this last frame.
		sync(time, true);

		// Let underlying graphics system finish rendering this frame.
		auto time1 = Timer::getTime();
		rasterizer->frameEnd();
		auto time2 = Timer::getTime();
		auto current = narrow_cast<float>(time2 - time1);
		const float ALPHA = 0.2f;
		finishFrameDuration = finishFrameDuration * (1 - ALPHA) +
		                      current * ALPHA;

		if (vdp.isInterlaced() && vdp.isEvenOddEnabled()
				&& renderSettings.getDeinterlace() && !prevRenderFrame) {
			// Don't paint in deinterlace mode when previous frame
			// was not rendered.
			paintFrame = false;
		}
		if (paintFrame) {
			lastPaintTime = time2;
		}
	}
	if (vdp.getMotherBoard().isActive() &&
	    !vdp.getMotherBoard().isFastForwarding()) {
		eventDistributor.distributeEvent(FinishFrameEvent(
			rasterizer->getPostProcessor()->getVideoSource(),
			videoSourceSetting.getSource(),
			!paintFrame));
	}
}

void PixelRenderer::updateHorizontalScrollLow(
	uint8_t scroll, EmuTime time)
{
	if (displayEnabled) sync(time);
	rasterizer->setHorizontalScrollLow(scroll);
}

void PixelRenderer::updateHorizontalScrollHigh(
	uint8_t /*scroll*/, EmuTime time)
{
	if (displayEnabled) sync(time);
}

void PixelRenderer::updateBorderMask(
	bool masked, EmuTime time)
{
	if (displayEnabled) sync(time);
	rasterizer->setBorderMask(masked);
}

void PixelRenderer::updateMultiPage(
	bool /*multiPage*/, EmuTime time)
{
	if (displayEnabled) sync(time);
}

void PixelRenderer::updateTransparency(
	bool enabled, EmuTime time)
{
	if (displayEnabled) sync(time);
	rasterizer->setTransparency(enabled);
}

void PixelRenderer::updateSuperimposing(
	const RawFrame* videoSource, EmuTime time)
{
	if (displayEnabled) sync(time);
	rasterizer->setSuperimposeVideoFrame(videoSource);
}

void PixelRenderer::updateForegroundColor(
	uint8_t /*color*/, EmuTime time)
{
	if (displayEnabled) sync(time);
}

void PixelRenderer::updateBackgroundColor(
	uint8_t color, EmuTime time)
{
	sync(time);
	rasterizer->setBackgroundColor(color);
}

void PixelRenderer::updateBlinkForegroundColor(
	uint8_t /*color*/, EmuTime time)
{
	if (displayEnabled) sync(time);
}

void PixelRenderer::updateBlinkBackgroundColor(
	uint8_t /*color*/, EmuTime time)
{
	if (displayEnabled) sync(time);
}

void PixelRenderer::updateBlinkState(
	bool /*enabled*/, EmuTime /*time*/)
{
	// TODO: When the sync call is enabled, the screen flashes on
	//       every call to this method.
	//       I don't know why exactly, but it's probably related to
	//       being called at frame start.
	//sync(time);
}

void PixelRenderer::updatePalette(
	unsigned index, int grb, EmuTime time)
{
	if (displayEnabled) {
		sync(time);
	} else {
		// Only sync if border color changed.
		DisplayMode mode = vdp.getDisplayMode();
		if (mode.getBase() == DisplayMode::GRAPHIC5) {
			auto bgColor = vdp.getBackgroundColor();
			if (index == one_of(uint8_t(bgColor & 3), uint8_t(bgColor >> 2))) {
				sync(time);
			}
		} else if (mode.getByte() != DisplayMode::GRAPHIC7) {
			if (index == vdp.getBackgroundColor()) {
				sync(time);
			}
		}
	}
	rasterizer->setPalette(index, grb);
}

void PixelRenderer::updateVerticalScroll(
	int /*scroll*/, EmuTime time)
{
	if (displayEnabled) sync(time);
}

void PixelRenderer::updateHorizontalAdjust(
	int adjust, EmuTime time)
{
	if (displayEnabled) sync(time);
	rasterizer->setHorizontalAdjust(adjust);
}

void PixelRenderer::updateDisplayMode(
	DisplayMode mode, EmuTime time)
{
	// Sync if in display area or if border drawing process changes.
	DisplayMode oldMode = vdp.getDisplayMode();
	if (displayEnabled
	|| oldMode.getByte() == DisplayMode::GRAPHIC5
	|| oldMode.getByte() == DisplayMode::GRAPHIC7
	|| mode.getByte() == DisplayMode::GRAPHIC5
	|| mode.getByte() == DisplayMode::GRAPHIC7) {
		sync(time, true);
	}
	rasterizer->setDisplayMode(mode);
}

void PixelRenderer::updateNameBase(
	unsigned /*addr*/, EmuTime time)
{
	if (displayEnabled) sync(time);
}

void PixelRenderer::updatePatternBase(
	unsigned /*addr*/, EmuTime time)
{
	if (displayEnabled) sync(time);
}

void PixelRenderer::updateColorBase(
	unsigned /*addr*/, EmuTime time)
{
	if (displayEnabled) sync(time);
}

void PixelRenderer::updateSpritesEnabled(
	bool /*enabled*/, EmuTime time
) {
	if (displayEnabled) sync(time);
}

static constexpr bool overlap(
	int displayY0, // start of display region, inclusive
	int displayY1, // end of display region, exclusive
	int vramLine0, // start of VRAM region, inclusive
	int vramLine1  // end of VRAM region, exclusive
	// Note: Display region can wrap around: 256 -> 0.
	//       VRAM region cannot wrap around.
) {
	if (displayY0 <= displayY1) {
		if ((vramLine1 > displayY0) && (vramLine0 <= displayY1)) {
			return true;
		}
	} else {
		if (vramLine1 > displayY0) return true;
		if (vramLine0 <= displayY1) return true;
	}
	return false;
}

bool PixelRenderer::checkSync(unsigned offset, EmuTime time) const
{
	// TODO: Because range is entire VRAM, offset == address.

	// If display is disabled, VRAM changes will not affect the
	// renderer output, therefore sync is not necessary.
	// TODO: Have bitmapVisibleWindow disabled in this case.
	if (!displayEnabled) return false;
	//if (frameSkipCounter != 0) return false; // TODO
	if (accuracy == RenderSettings::Accuracy::SCREEN) return false;

	// Calculate what display lines are scanned between current
	// renderer time and update-to time.
	// Note: displayY1 is inclusive.
	int deltaY = vdp.getVerticalScroll() - vdp.getLineZero();
	int limitY = vdp.getTicksThisFrame(time) / VDP::TICKS_PER_LINE;
	int displayY0 = (nextY + deltaY) & 255;
	int displayY1 = (limitY + deltaY) & 255;

	switch(vdp.getDisplayMode().getBase()) {
	case DisplayMode::GRAPHIC2:
	case DisplayMode::GRAPHIC3:
		if (vram.colorTable.isInside(offset)) {
			unsigned vramQuarter = (offset & 0x1800) >> 11;
			unsigned mask = (vram.colorTable.getMask() & 0x1800) >> 11;
			for (auto i : xrange(4)) {
				if ((i & mask) == vramQuarter
				&& overlap(displayY0, displayY1, i * 64, (i + 1) * 64)) {
					/*fprintf(stderr,
						"color table: %05X %04X - quarter %d\n",
						offset, offset & 0x1FFF, i
						);*/
					return true;
				}
			}
		}
		if (vram.patternTable.isInside(offset)) {
			unsigned vramQuarter = (offset & 0x1800) >> 11;
			unsigned mask = (vram.patternTable.getMask() & 0x1800) >> 11;
			for (auto i : xrange(4)) {
				if ((i & mask) == vramQuarter
				&& overlap(displayY0, displayY1, i * 64, (i + 1) * 64)) {
					/*fprintf(stderr,
						"pattern table: %05X %04X - quarter %d\n",
						offset, offset & 0x1FFF, i
						);*/
					return true;
				}
			}
		}
		if (vram.nameTable.isInside(offset)) {
			int vramLine = narrow<int>(((offset & 0x3FF) / 32) * 8);
			if (overlap(displayY0, displayY1, vramLine, vramLine + 8)) {
				/*fprintf(stderr,
					"name table: %05X %03X - line %d\n",
					offset, offset & 0x3FF, vramLine
					);*/
				return true;
			}
		}
		return false;
	case DisplayMode::GRAPHIC4:
	case DisplayMode::GRAPHIC5: {
		if (vdp.isFastBlinkEnabled()) {
			// TODO could be improved
			return true;
		}
		// Is the address inside the visual page(s)?
		// TODO: Also look at which lines are touched inside pages.
		unsigned visiblePage = vram.nameTable.getMask()
			& (0x10000 | (vdp.getEvenOddMask() << 7));
		if (vdp.isMultiPageScrolling()) {
			return (offset & 0x18000) == visiblePage
				|| (offset & 0x18000) == (visiblePage & 0x10000);
		} else {
			return (offset & 0x18000) == visiblePage;
		}
	}
	case DisplayMode::GRAPHIC6:
	case DisplayMode::GRAPHIC7:
		return true; // TODO: Implement better detection.
	default:
		// Range unknown; assume full range.
		return vram.nameTable.isInside(offset)
			|| vram.colorTable.isInside(offset)
			|| vram.patternTable.isInside(offset);
	}
}

void PixelRenderer::updateVRAM(unsigned offset, EmuTime time)
{
	// Note: No need to sync if display is disabled, because then the
	//       output does not depend on VRAM (only on background color).
	if (renderFrame && displayEnabled && checkSync(offset, time)) {
		renderUntil(time);
	}
}

void PixelRenderer::updateWindow(bool /*enabled*/, EmuTime /*time*/)
{
	// The bitmapVisibleWindow has moved to a different area.
	// This update is redundant: Renderer will be notified in another way
	// as well (updateDisplayEnabled or updateNameBase, for example).
	// TODO: Can this be used as the main update method instead?
}

void PixelRenderer::sync(EmuTime time, bool force)
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
	if (accuracy != RenderSettings::Accuracy::SCREEN || force) {
		vram.sync(time);
		renderUntil(time);
	}
}

void PixelRenderer::renderUntil(EmuTime time)
{
	// Translate from time to pixel position.
	int limitTicks = vdp.getTicksThisFrame(time);
	assert(limitTicks <= vdp.getTicksPerFrame());
	auto [limitX, limitY] = [&]() -> std::pair<int, int> {
		switch (accuracy) {
		case RenderSettings::Accuracy::PIXEL: {
			return {limitTicks % VDP::TICKS_PER_LINE,
			        limitTicks / VDP::TICKS_PER_LINE};
		}
		case RenderSettings::Accuracy::LINE:
		case RenderSettings::Accuracy::SCREEN:
			// Note: I'm not sure the rounding point is optimal.
			//       It used to be based on the left margin, but that doesn't work
			//       because the margin can change which leads to a line being
			//       rendered even though the time doesn't advance.
			return {0,
				(limitTicks + VDP::TICKS_PER_LINE - 400) / VDP::TICKS_PER_LINE};
		default:
			UNREACHABLE;
		}
	}();

	// Stop here if there is nothing to render.
	// This ensures that no pixels are rendered in a series of updates that
	// happen at exactly the same time; the VDP subsystem states may be
	// inconsistent until all updates are performed.
	// Also it is a small performance optimisation.
	if (limitX == nextX && limitY == nextY) return;

	if (displayEnabled) {
		if (vdp.spritesEnabled()) {
			// Update sprite checking, so that rasterizer can call getSprites.
			spriteChecker.checkUntil(time);
		}

		// Calculate start and end of borders in ticks since start of line.
		// The 0..7 extra horizontal scroll low pixels should be drawn in
		// border color. These will be drawn together with the border,
		// but sprites above these pixels are clipped at the actual border
		// rather than the end of the border colored area.
		// TODO: Move these calculations and getDisplayLeft() to VDP.
		int borderL = vdp.getLeftBorder();
		int displayL =
			vdp.isBorderMasked() ? borderL : vdp.getLeftBackground();
		int borderR = vdp.getRightBorder();

		// It's important that right border is drawn last (after left
		// border and display area). See comment in SDLRasterizer::drawBorder().
		// Left border.
		subdivide(nextX, nextY, limitX, limitY,
			0, displayL, DRAW_BORDER);
		// Display area.
		subdivide(nextX, nextY, limitX, limitY,
			displayL, borderR, DRAW_DISPLAY);
		// Right border.
		subdivide(nextX, nextY, limitX, limitY,
			borderR, VDP::TICKS_PER_LINE, DRAW_BORDER);
	} else {
		subdivide(nextX, nextY, limitX, limitY,
			0, VDP::TICKS_PER_LINE, DRAW_BORDER);
	}

	nextX = limitX;
	nextY = limitY;
}

void PixelRenderer::update(const Setting& setting) noexcept
{
	assert(&setting == one_of(&renderSettings.getMinFrameSkipSetting(),
	                          &renderSettings.getMaxFrameSkipSetting()));
	(void)setting;
	// Force drawing of frame.
	frameSkipCounter = 999;
}

} // namespace openmsx
