// $Id$

#include "V9990.hh"
#include "V9990DisplayTiming.hh"
#include "V9990PixelRenderer.hh"
#include "V9990Rasterizer.hh"
#include "Scheduler.hh"
#include "Display.hh"
#include "VideoSystem.hh"
#include "FinishFrameEvent.hh"
#include "RealTime.hh"
#include "Timer.hh"
#include "EventDistributor.hh"
#include "RenderSettings.hh"
#include "IntegerSetting.hh"
#include "openmsx.hh"

namespace openmsx {
	
V9990PixelRenderer::V9990PixelRenderer(V9990* vdp_)
	: vdp(vdp_)
	, rasterizer(Display::instance().getVideoSystem()
	                                .createV9990Rasterizer(vdp_))
	, horTiming(&V9990DisplayTiming::lineMCLK)
	, verTiming(&V9990DisplayTiming::displayNTSC_MCLK)
{
	frameSkipCounter = 999; // force drawing of frame;
	finishFrameDuration = 0;
	drawFrame = false; // don't draw before frameStart is called
	
	reset(Scheduler::instance().getCurrentTime());

	settings.getMaxFrameSkip()->addListener(this);
	settings.getMinFrameSkip()->addListener(this);
}

V9990PixelRenderer::~V9990PixelRenderer()
{
	settings.getMaxFrameSkip()->removeListener(this);
	settings.getMinFrameSkip()->removeListener(this);
}
	
void V9990PixelRenderer::reset(const EmuTime& time)
{
	PRT_DEBUG("V9990PixelRenderer::reset");

	lastX = 0;
	lastY = 0;

	setDisplayMode(vdp->getDisplayMode(), time);
	setColorMode(vdp->getColorMode(), time);

	rasterizer->reset();
}

void V9990PixelRenderer::frameStart(const EmuTime& time)
{
	if (rasterizer->getZ() == Layer::Z_MSX_PASSIVE) {
		// V99x8 is active
		frameSkipCounter = 0;
		drawFrame = false;
	} else if (frameSkipCounter < settings.getMinFrameSkip()->getValue()) {
		++frameSkipCounter;
		drawFrame = false;
	} else if (frameSkipCounter >= settings.getMaxFrameSkip()->getValue()) {
		frameSkipCounter = 0;
		drawFrame = true;
	} else {
		++frameSkipCounter;
		drawFrame = RealTime::instance().timeLeft(
			(unsigned)finishFrameDuration, time);
		if (drawFrame) {
			frameSkipCounter = 0;
		}
	}
	if (!drawFrame) return;
	
	// Make sure that the correct timing is used
	setDisplayMode(vdp->getDisplayMode(), time);
	rasterizer->frameStart(horTiming, verTiming);
}

void V9990PixelRenderer::frameEnd(const EmuTime& time)
{
	PRT_DEBUG("V9990PixelRenderer::frameEnd");
	
	if (!drawFrame) return;

	unsigned long long time1 = Timer::getTime();
	
	// Render last changes in this frame before starting a new frame
	// sync(time);
	renderUntil(time);
	lastX = 0; lastY = 0;
	rasterizer->frameEnd();

	unsigned long long time2 = Timer::getTime();
	unsigned long long current = time2 - time1;
	const double ALPHA = 0.2;
	finishFrameDuration = finishFrameDuration * (1 - ALPHA) +
	                      current * ALPHA;
	
	FinishFrameEvent *f = new FinishFrameEvent(VIDEO_GFX9000);
	EventDistributor::instance().distributeEvent(f);
}

void V9990PixelRenderer::renderUntil(const EmuTime& time)
{
	PRT_DEBUG("V9990PixelRenderer::renderUntil");

	// Translate time to pixel position
	int nofTicks = vdp->getUCTicksThisFrame(time);
	int toX      = nofTicks % V9990DisplayTiming::UC_TICKS_PER_LINE
	               - horTiming->blank;
	int toY      = nofTicks / V9990DisplayTiming::UC_TICKS_PER_LINE
	               - verTiming->blank;
	
	// Screen accuracy for now...
	if (nofTicks < V9990DisplayTiming::getUCTicksPerFrame(vdp->isPalTiming())) {
		return;
	}
	
	// edges of the DISPLAY part of the vdp output
	int left       = horTiming->border1;
	int right      = left   + horTiming->display;
	int rightEdge  = right  + horTiming->border2;
	int top        = verTiming->border1;
	int bottom     = top    + verTiming->display;
	int bottomEdge = bottom + verTiming->border2;

	if (vdp->isDisplayEnabled()) {
		// top border
		render(lastX, lastY, toX, toY,
		       0, 0, rightEdge, top, DRAW_BORDER);

		// display area: left border/display/right border
		render(lastX, lastY, toX, toY,
		       0, top, left, bottom, DRAW_BORDER);
		render(lastX, lastY, toX, toY,
		       left, top, right, bottom, DRAW_DISPLAY);
		render(lastX, lastY, toX, toY,
		       right, top, rightEdge, bottom, DRAW_BORDER);

		// bottom border
		render(lastX, lastY, toX, toY,
		       0, bottom, rightEdge, bottomEdge, DRAW_BORDER);
	} else {
		// complete screen
		render(lastX, lastY, toX, toY,
		       0, 0, rightEdge, bottomEdge, DRAW_BORDER);
	}

	lastX = toX;
	lastY = toY;
}

void V9990PixelRenderer::draw(int fromX, int fromY, int toX, int toY,
                              DrawType type)
{
	PRT_DEBUG("V9990PixelRenderer::draw(" << std::dec <<
			fromX << "," << fromY << "," << toX << "," << toY << ","
			<< ((type == DRAW_BORDER)? "BORDER": "DISPLAY") << ")");
	
	int displayX = fromX - horTiming->border1;
	int displayY = fromY - verTiming->border1;
	int displayWidth = toX - fromX;
	int displayHeight = toY - fromY;

	switch(type) {
	case DRAW_BORDER:
		rasterizer->drawBorder(fromX, fromY, toX, toY);
		break;
	case DRAW_DISPLAY:
		rasterizer->drawDisplay(fromX, fromY,
		                        displayX, displayY,
		                        displayWidth, displayHeight);
		break;
	default:
		assert(false);
	}
}

void V9990PixelRenderer::render(int fromX, int fromY, int toX, int toY,
                                int clipL, int clipT, int clipR, int clipB,
                                DrawType drawType)
{
	PRT_DEBUG("V9990PixelRenderer::render(" <<
		  fromX << "," << fromY << "," << toX << "," << toY << ", " <<
		  clipL << "," << clipT << "," << clipR << "," << clipB << ", " <<
		  ((drawType == DRAW_BORDER)? "BORDER": "DISPLAY") << ")");
	
	// clip top & bottom 
	if(clipT > fromY) {
		fromX = clipL;
		fromY = clipT;
	}
	if(clipB < toY) {
		toX = clipR;
		toY = clipB - 1;
	}
	
	if(fromY < toY) {
		// draw partial first line
		if(fromX > clipL) {
			if(fromX < clipR) {
				draw(fromX, fromY, clipR, fromY+1, drawType);
			}
			fromY++;
		}

		if(toX >= clipR) {
			toY++;
			toX = clipL - 1;
		}

		// draw middle recangle
		if(fromY < toY) {
			draw(clipL, fromY, clipR, toY, drawType);
			fromY = toY;
			fromX = clipL;
		}
	}
	
	// draw last part
	if((fromY == toY) && (toY < clipB) && 
	   (toX > fromX)) {
		draw(fromX, fromY, toX, toY+1, drawType);
	}
		
}

void V9990PixelRenderer::setDisplayMode(V9990DisplayMode mode, const EmuTime& time)
{
	// sync(time);
	switch(mode) {
	case P1:
	case P2:
	case B1:
	case B3:
	case B7:
		horTiming = &V9990DisplayTiming::lineMCLK;
		if(vdp->isPalTiming()) {
			verTiming = &V9990DisplayTiming::displayPAL_MCLK;
		} else {
			verTiming = &V9990DisplayTiming::displayNTSC_MCLK;
		}
		break;
	case B0:
	case B2:
	case B4:
		horTiming = &V9990DisplayTiming::lineXTAL;
		if(vdp->isPalTiming()) {
			verTiming = &V9990DisplayTiming::displayPAL_XTAL;
		} else {
			verTiming = &V9990DisplayTiming::displayNTSC_XTAL;
		}
	case B5:
	case B6:
		break;
	default:
		assert(false);
	}

	rasterizer->setDisplayMode(mode);
}

void V9990PixelRenderer::updatePalette(int index, byte r, byte g, byte b,
                                       const EmuTime& time)
{
	// sync(time);
	rasterizer->setPalette(index, r, g, b);
}
void V9990PixelRenderer::setColorMode(V9990ColorMode mode, const EmuTime& time)
{
	// sync(time);
	rasterizer->setColorMode(mode);
}

void V9990PixelRenderer::updateBackgroundColor(int index, const EmuTime& time)
{
	// sync(time);
}

void V9990PixelRenderer::setImageWidth(int width)
{
	rasterizer->setImageWidth(width);
}

void V9990PixelRenderer::update(const Setting* setting)
{
	if (setting == settings.getMinFrameSkip() ||
	    setting == settings.getMaxFrameSkip()) {
		// Force drawing of frame
		frameSkipCounter = 999;
	} else {
		assert(false);
	}
}

} // namespace openmsx
