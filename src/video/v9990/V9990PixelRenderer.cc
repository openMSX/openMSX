#include "V9990PixelRenderer.hh"
#include "V9990.hh"
#include "V9990VRAM.hh"
#include "V9990DisplayTiming.hh"
#include "V9990Rasterizer.hh"
#include "PostProcessor.hh"
#include "Display.hh"
#include "VideoSystem.hh"
#include "VideoSourceSetting.hh"
#include "Event.hh"
#include "RealTime.hh"
#include "Timer.hh"
#include "EventDistributor.hh"
#include "MSXMotherBoard.hh"
#include "Reactor.hh"
#include "RenderSettings.hh"
#include "IntegerSetting.hh"
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

void V9990PixelRenderer::reset(EmuTime::param time)
{
	displayEnabled = vdp.isDisplayEnabled();
	setDisplayMode(vdp.getDisplayMode(), time);
	setColorMode(vdp.getColorMode(), time);

	rasterizer->reset();
}

void V9990PixelRenderer::frameStart(EmuTime::param time)
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

void V9990PixelRenderer::frameEnd(EmuTime::param time)
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

void V9990PixelRenderer::sync(EmuTime::param time, bool force)
{
	if (!drawFrame) return;

	if (accuracy != RenderSettings::Accuracy::SCREEN || force) {
		vdp.getVRAM().sync(time);
		renderUntil(time);
	}
}

void V9990PixelRenderer::renderUntil(EmuTime::param time)
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

void V9990PixelRenderer::updateDisplayEnabled(bool enabled, EmuTime::param time)
{
	sync(time, true);
	displayEnabled = enabled;
}

void V9990PixelRenderer::setDisplayMode(V9990DisplayMode mode, EmuTime::param time)
{
	sync(time);
	rasterizer->setDisplayMode(mode);
}

void V9990PixelRenderer::updatePalette(int index, uint8_t r, uint8_t g, uint8_t b, bool ys,
                                       EmuTime::param time)
{
	if (displayEnabled) {
		sync(time);
	} else {
		// TODO only sync if border color changed
		sync(time);
	}
	rasterizer->setPalette(index, r, g, b, ys);
}
void V9990PixelRenderer::updateSuperimposing(bool enabled, EmuTime::param time)
{
	sync(time);
	rasterizer->setSuperimpose(enabled);
}
void V9990PixelRenderer::setColorMode(V9990ColorMode mode, EmuTime::param time)
{
	sync(time);
	rasterizer->setColorMode(mode);
}

void V9990PixelRenderer::updateBackgroundColor(int /*index*/, EmuTime::param time)
{
	sync(time);
}

void V9990PixelRenderer::updateScrollAX(EmuTime::param time)
{
	if (displayEnabled) sync(time);
}
void V9990PixelRenderer::updateScrollBX(EmuTime::param time)
{
	// TODO only in P1 mode
	if (displayEnabled) sync(time);
}
void V9990PixelRenderer::updateScrollAYLow(EmuTime::param time)
{
	if (displayEnabled) {
		sync(time);
		// happens in all display modes (verified)
		// TODO high byte still seems to be wrong .. need to investigate
		verticalOffsetA = lastY;
	}
}
void V9990PixelRenderer::updateScrollBYLow(EmuTime::param time)
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
