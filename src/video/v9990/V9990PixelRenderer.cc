#include "V9990PixelRenderer.hh"

#include "V9990.hh"
#include "V9990DisplayTiming.hh"
#include "V9990Rasterizer.hh"
#include "V9990VRAM.hh"

#include "Display.hh"
#include "PostProcessor.hh"
#include "RenderSettings.hh"
#include "VideoSystem.hh"

#include "BooleanSetting.hh"
#include "Event.hh"
#include "EventDistributor.hh"
#include "IntegerSetting.hh"
#include "MSXMotherBoard.hh"
#include "Reactor.hh"
#include "RealTime.hh"
#include "Timer.hh"
#include "VideoSourceSetting.hh"

#include "narrow.hh"
#include "one_of.hh"
#include "unreachable.hh"

#include <cassert>

namespace openmsx {

V9990PixelRenderer::V9990PixelRenderer(V9990& vdp_)
	: vdp(vdp_)
	, eventDistributor(vdp.getReactor().getEventDistributor())
	, realTime(vdp.getMotherBoard().getRealTime())
	, renderSettings(vdp.getReactor().getDisplay().getRenderSettings())
	, videoSourceSetting(vdp.getMotherBoard().getVideoSource())
	, rasterizer(vdp.getReactor().getDisplay().
	                getVideoSystem().createV9990Rasterizer(vdp))
	, paletteSnowSetting(vdp.getMotherBoard().getSharedStuff<BooleanSetting>(
		"v9990_palette_snow",
		vdp.getCommandController(), "v9990_palette_snow",
		"Show the stray dots ('snow') a real V9990 shows when its "
		"palette is written while the screen is drawn", true))
{
	reset(vdp.getMotherBoard().getCurrentTime());

	renderSettings.getMaxFrameSkipSetting().attach(*this);
	renderSettings.getMinFrameSkipSetting().attach(*this);
}

V9990PixelRenderer::~V9990PixelRenderer()
{
	renderSettings.getMaxFrameSkipSetting().detach(*this);
	renderSettings.getMinFrameSkipSetting().detach(*this);
}

PostProcessor* V9990PixelRenderer::getPostProcessor() const
{
	return rasterizer->getPostProcessor();
}

void V9990PixelRenderer::reset(EmuTime time)
{
	displayEnabled = vdp.isDisplayEnabled();
	setDisplayMode(vdp.getDisplayMode(), time);
	setColorMode(vdp.getColorMode(), time);

	rasterizer->reset();
}

void V9990PixelRenderer::frameStart(EmuTime time)
{
	if (!rasterizer->isActive()) {
		frameSkipCounter = 999;
		drawFrame = false;
		prevDrawFrame = false;
		return;
	}
	prevDrawFrame = drawFrame;
	if (vdp.isInterlaced() && renderSettings.getDeinterlace() &&
	    vdp.getEvenOdd() && vdp.isEvenOddEnabled()) {
		// deinterlaced odd frame, do same as even frame
	} else {
		if (frameSkipCounter < renderSettings.getMinFrameSkip()) {
			++frameSkipCounter;
			drawFrame = false;
		} else if (frameSkipCounter >= renderSettings.getMaxFrameSkip()) {
			frameSkipCounter = 0;
			drawFrame = true;
		} else {
			++frameSkipCounter;
			if (rasterizer->isRecording()) {
				drawFrame = true;
			} else {
				drawFrame = realTime.timeLeft(
					unsigned(finishFrameDuration), time);
			}
			if (drawFrame) {
				frameSkipCounter = 0;
			}
		}
	}
	if (!drawFrame) return;

	accuracy = renderSettings.getAccuracy();
	lastX = 0;
	lastY = 0;
	verticalOffsetA = verticalOffsetB = vdp.getTopBorder();

	// Make sure that the correct timing is used
	setDisplayMode(vdp.getDisplayMode(), time);
	rasterizer->frameStart();
}

void V9990PixelRenderer::frameEnd(EmuTime time)
{
	bool skipEvent = !drawFrame;
	if (drawFrame) {
		// Render last changes in this frame before starting a new frame
		sync(time, true);

		auto time1 = Timer::getTime();
		rasterizer->frameEnd(time);
		auto time2 = Timer::getTime();
		auto current = narrow_cast<float>(time2 - time1);
		const float ALPHA = 0.2f;
		finishFrameDuration = finishFrameDuration * (1 - ALPHA) +
		                      current * ALPHA;

		if (vdp.isInterlaced() && vdp.isEvenOddEnabled() &&
		    renderSettings.getDeinterlace() &&
		    !prevDrawFrame) {
			// dont send event in deinterlace mode when
			// previous frame was not rendered
			skipEvent = true;
		}

	}
	if (vdp.getMotherBoard().isActive() &&
	    !vdp.getMotherBoard().isFastForwarding()) {
		eventDistributor.distributeEvent(FinishFrameEvent(
			rasterizer->getPostProcessor()->getVideoSource(),
			videoSourceSetting.getSource(),
			skipEvent));
	}
}

void V9990PixelRenderer::sync(EmuTime time, bool force)
{
	if (!drawFrame) return;

	if (accuracy != RenderSettings::Accuracy::SCREEN || force) {
		vdp.getVRAM().sync(time);
		renderUntil(time);
	}
}

void V9990PixelRenderer::renderUntil(EmuTime time)
{
	// Translate time to pixel position
	int limitTicks = vdp.getUCTicksThisFrame(time);
	assert(limitTicks <=
	       V9990DisplayTiming::getUCTicksPerFrame(vdp.isPalTiming()));
	auto [toX, toY] = [&] {
		switch (accuracy) {
		case RenderSettings::Accuracy::PIXEL:
			return std::pair{
				limitTicks % V9990DisplayTiming::UC_TICKS_PER_LINE,
				limitTicks / V9990DisplayTiming::UC_TICKS_PER_LINE
			};
		case RenderSettings::Accuracy::LINE:
		case RenderSettings::Accuracy::SCREEN:
			// TODO figure out rounding point
			return std::pair{
				0,
				(limitTicks + V9990DisplayTiming::UC_TICKS_PER_LINE - 400) /
				     V9990DisplayTiming::UC_TICKS_PER_LINE
			};
		default:
			UNREACHABLE;
		}
	}();

	if ((toX == lastX) && (toY == lastY)) return;

	// edges of the DISPLAY part of the vdp output
	int left       = vdp.getLeftBorder();
	int right      = vdp.getRightBorder();
	int rightEdge  = V9990DisplayTiming::UC_TICKS_PER_LINE;

	if (displayEnabled) {
		// Left border
		subdivide(lastX, lastY, toX, toY, 0, left, DRAW_BORDER);
		// Display area
		//  It's possible this draws a few pixels too many (this
		//  allowed to simplify the implementation of the Bx modes).
		//  So it's important to draw from left to right (right border
		//  must come _after_ display area).
		subdivide(lastX, lastY, toX, toY, left, right, DRAW_DISPLAY);
		// Right border
		subdivide(lastX, lastY, toX, toY, right, rightEdge, DRAW_BORDER);
	} else {
		// complete screen
		subdivide(lastX, lastY, toX, toY, 0, rightEdge, DRAW_BORDER);
	}

	lastX = toX;
	lastY = toY;
}

void V9990PixelRenderer::subdivide(int fromX, int fromY, int toX, int toY,
                                   int clipL, int clipR, DrawType drawType)
{
	// partial first line
	if (fromX > clipL) {
		if (fromX < clipR) {
			bool atEnd = (fromY != toY) || (toX >= clipR);
			draw(fromX, fromY, (atEnd ? clipR : toX), fromY + 1,
			     drawType);
		}
		if (fromY == toY) return;
		fromY++;
	}

	bool drawLast = false;
	if (toX >= clipR) {
		toY++;
	} else if (toX > clipL) {
		drawLast = true;
	}
	// full middle lines
	if (fromY < toY) {
		draw(clipL, fromY, clipR, toY, drawType);
	}

	// partial last line
	if (drawLast) draw(clipL, toY, toX, toY + 1, drawType);
}

void V9990PixelRenderer::draw(int fromX, int fromY, int toX, int toY,
                              DrawType type)
{
	if (type == DRAW_BORDER) {
		rasterizer->drawBorder(fromX, fromY, toX, toY);

	} else {
		assert(type == DRAW_DISPLAY);

		int displayX  = fromX - vdp.getLeftBorder();
		int displayY  = fromY - vdp.getTopBorder();
		int displayYA = fromY - verticalOffsetA;
		int displayYB = fromY - verticalOffsetB;

		rasterizer->drawDisplay(fromX, fromY, toX, toY,
		                        displayX,
		                        displayY, displayYA, displayYB);
	}
}

void V9990PixelRenderer::updateDisplayEnabled(bool enabled, EmuTime time)
{
	sync(time, true);
	displayEnabled = enabled;
}

void V9990PixelRenderer::setDisplayMode(V9990DisplayMode mode, EmuTime time)
{
	sync(time);
	rasterizer->setDisplayMode(mode);
}

void V9990PixelRenderer::updatePalette(int index, uint8_t r, uint8_t g, uint8_t b, bool ys,
                                       EmuTime time)
{
	if (displayEnabled) {
		sync(time);
	} else {
		// TODO only sync if border color changed
		sync(time);
	}
	rasterizer->setPalette(index, r, g, b, ys);
}

// Does a picture dot of this colour mode take its colour from the palette?
// (The border always does.)
bool V9990PixelRenderer::readsPalette(V9990ColorMode mode)
{
	using enum V9990ColorMode;
	switch (mode) {
	case PP: case BP2: case BP4: case BP6:
		return true;
	case BD8: case BD16: case BYJK: case BYUV:
		// Measured: the picture never snows, BD8's all-zero dots
		// included.
		// TODO sprites and cursors read the palette, so their dots
		//      should snow in these modes too (not tested).
		return false;
	case BYJKP: case BYUVP:
		// Measured: the palette dots (VD3 = 1) snow, the YJK/YUV dots
		// beside them don't.
		// TODO V9990::getColorMode() never returns these modes (YAE,
		//      R#13 bit 5, isn't implemented), so they show as BYJK and
		//      BYUV. Once it is, snow only on the palette dots.
		return false;
	}
	UNREACHABLE;
}

// Measured on a real V9990 (Tecnobytes PowerGraph, B1, NTSC): every byte
// written to the palette data port while the beam is in the picture or the
// border puts a dot at the beam's position, of the colour being written. It
// doesn't matter which entry is written, whether any pixel uses it, or
// whether the value changes. Writes during the horizontal or vertical blank
// show nothing. Only a dot whose colour comes out of the palette snows: the
// border (the backdrop) in every colour mode and the picture in the
// paletted modes (P1, P2, BP2, BP4, BP6). The picture of the direct-colour
// modes (BD8, BD16, BYJK, BYUV) stays clean. The colour is taken as the
// written entry's colour after the byte (all tests wrote equal R, G and B);
// the dot as one dot wide.
// Reads of the palette data port snow at the same places (measured, with
// the right data returned). Their colour can't be told apart on the photos;
// the entry being read is used. Some reads seem to leave no dot (maybe the
// chip prefetches the next byte); one dot per read is modelled.
void V9990PixelRenderer::paletteSnow(int index, EmuTime time)
{
	if (!drawFrame || !paletteSnowSetting->getBoolean()) return;

	int ticks = vdp.getUCTicksThisFrame(time);
	int x = ticks % V9990DisplayTiming::UC_TICKS_PER_LINE;
	int y = ticks / V9990DisplayTiming::UC_TICKS_PER_LINE;
	auto visible = [](int pos, const V9990DisplayPeriod& period) {
		int start = period.blank;
		int end = start + period.border1 + period.display + period.border2;
		return (start <= pos) && (pos < end);
	};
	if (!visible(x, vdp.getHorizontalTiming()) ||
	    !visible(y, vdp.getVerticalTiming())) return;

	bool inPicture = vdp.isDisplayEnabled() &&
	                 (vdp.getLeftBorder() <= x) && (x < vdp.getRightBorder());
	if (inPicture && !readsPalette(vdp.getColorMode())) return;

	auto [r, g, b, ys] = vdp.getPalette(index);
	rasterizer->drawSnow(x, y, r, g, b);
}

void V9990PixelRenderer::updateSuperimposing(bool enabled, EmuTime time)
{
	sync(time);
	rasterizer->setSuperimpose(enabled);
}
void V9990PixelRenderer::setColorMode(V9990ColorMode mode, EmuTime time)
{
	sync(time);
	rasterizer->setColorMode(mode);
}

void V9990PixelRenderer::updateBackgroundColor(int /*index*/, EmuTime time)
{
	sync(time);
}

void V9990PixelRenderer::updateScrollAX(EmuTime time)
{
	if (displayEnabled) sync(time);
}
void V9990PixelRenderer::updateScrollBX(EmuTime time)
{
	// TODO only in P1 mode
	if (displayEnabled) sync(time);
}
void V9990PixelRenderer::updateScrollAYLow(EmuTime time)
{
	if (displayEnabled) {
		sync(time);
		// happens in all display modes (verified)
		// TODO high byte still seems to be wrong .. need to investigate
		verticalOffsetA = lastY;
	}
}
void V9990PixelRenderer::updateScrollBYLow(EmuTime time)
{
	// TODO only in P1 mode
	if (displayEnabled) {
		sync(time);
		// happens in all display modes (verified)
		// TODO high byte still seems to be wrong .. need to investigate
		verticalOffsetB = lastY;
	}
}

void V9990PixelRenderer::update(const Setting& setting) noexcept
{
	assert(&setting == one_of(&renderSettings.getMinFrameSkipSetting(),
	                          &renderSettings.getMaxFrameSkipSetting()));
	(void)setting;
	// Force drawing of frame
	frameSkipCounter = 999;
}

} // namespace openmsx
