// $Id$

#include "V9990.hh"
#include "V9990DisplayTiming.hh"
#include "V9990PixelRenderer.hh"
#include "V9990Rasterizer.hh"
#include "Scheduler.hh"
#include "Display.hh"
#include "VideoSystem.hh"
#include "openmsx.hh"

namespace openmsx {
	
V9990PixelRenderer::V9990PixelRenderer(V9990* vdp_)
	: vdp(vdp_),
	  rasterizer(Display::INSTANCE->getVideoSystem()
		                          ->createV9990Rasterizer(vdp_)),
	  horTiming(&V9990DisplayTiming::lineMCLK),
	  verTiming(&V9990DisplayTiming::displayNTSC_MCLK)
{
	PRT_DEBUG("V9990PixelRenderer::V9990PixelRenderer");
	
	reset(Scheduler::instance().getCurrentTime());
}

V9990PixelRenderer::~V9990PixelRenderer()
{
	PRT_DEBUG("V9990PixelRenderer::~V9990PixelRenderer");
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
	// Make sure that the correct timing is used
	setDisplayMode(vdp->getDisplayMode(), time);
	rasterizer->frameStart(horTiming, verTiming);
}

void V9990PixelRenderer::frameEnd(const EmuTime& time)
{
	PRT_DEBUG("V9990PixelRenderer::frameEnd");
	// Render last changes in this frame before starting a new frame
	// sync(time);
	renderUntil(time);
	lastX = 0; lastY = 0;
	rasterizer->frameEnd();
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
	if(nofTicks < V9990DisplayTiming::getUCTicksPerFrame(vdp->isPalTiming()))
	   return;
	
	// edges of the DISPLAY part of the vdp output
	int left       = horTiming->border1;
	int right      = left   + horTiming->display;
	int rightEdge  = right  + horTiming->border2;
	int top        = verTiming->border1;
	int bottom     = top    + verTiming->display;
	int bottomEdge = bottom + verTiming->border2;

	// top border
	render(lastX, lastY, toX, toY,
		   0, 0, rightEdge,top, DRAW_BORDER);

	// display area: left border/display/right border
	render(lastX, lastY, toX, toY,
	       0, top, left, bottom, DRAW_BORDER);
	render(lastX, lastY, toX, toY,
	       left, top, right, bottom, DRAW_DISPLAY);
	render(lastX, lastY, toX, toY,
		   right, top, rightEdge, bottom,
		   DRAW_BORDER);

	// bottom border
	render(lastX, lastY, toX, toY,
		   0, bottom, rightEdge, bottomEdge,
		   DRAW_BORDER);

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

void V9990PixelRenderer::setPalette(int index, byte r, byte g, byte b,
                                    const EmuTime& time)
{
	rasterizer->setPalette(index, r, g, b);
}
void V9990PixelRenderer::setColorMode(V9990ColorMode mode, const EmuTime& time)
{
	// sync(time);
	rasterizer->setColorMode(mode);
}

void V9990PixelRenderer::setBackgroundColor(int index)
{
	rasterizer->setBackgroundColor(index);
}

void V9990PixelRenderer::setImageWidth(int width)
{
	rasterizer->setImageWidth(width);
}

} // namespace openmsx
