// $Id$

#include "V9990.hh"
#include "Scheduler.hh"
#include "V9990PixelRenderer.hh"
#include "Display.hh"
#include "VideoSystem.hh"

#include "openmsx.hh"

namespace openmsx {
	
// TODO make TOP_BLANK & LEFT_BLANK Clock & display timing dependent

static const int TOP_BLANK  =   3 +  15; // VSYNC + top erase
static const int LEFT_BLANK = 200 + 200; // HSYNC + left retrace (UC) 

V9990PixelRenderer::V9990PixelRenderer(V9990* vdp_)
	: vdp(vdp_)
{
	PRT_DEBUG("V9990PixelRenderer::V9990PixelRenderer");
	
	const EmuTime& now = Scheduler::instance().getCurrentTime();
	
	rasterizer = Display::INSTANCE->getVideoSystem()
		                          ->createV9990Rasterizer(vdp_);
	reset(now);
}

V9990PixelRenderer::~V9990PixelRenderer()
{
	PRT_DEBUG("V9990PixelRenderer::~V9990PixelRenderer");
}
	
void V9990PixelRenderer::reset(const EmuTime& time)
{
	PRT_DEBUG("V9990PixelRenderer::reset");

	const EmuTime& now = Scheduler::instance().getCurrentTime();

	lastX = 0;
	lastY = 0;

	setDisplayMode(vdp->getDisplayMode(), now);
	setColorMode(vdp->getColorMode(), now);

	rasterizer->reset();
}

void V9990PixelRenderer::frameStart(const EmuTime& time)
{
	rasterizer->frameStart();
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
	int toX      = nofTicks % V9990::UC_TICKS_PER_LINE - LEFT_BLANK;
	int toY      = nofTicks / V9990::UC_TICKS_PER_LINE - TOP_BLANK;
	
	// Screen accuracy for now...
	if(nofTicks < vdp->getUCTicksPerFrame()) return;
	
	// edges of the DISPLAY part of the vdp output
	int left   = leftBorderPeriod;
	int top    = topBorderPeriod;
	int right  = left + displayWidth;
	int bottom = top  + displayPeriod;
		
	// top border
	render(lastX, lastY, toX, toY,
		   0, 0, V9990::UC_TICKS_PER_LINE - LEFT_BLANK,top, DRAW_BORDER);

	// display area: left border/display/right border
	render(lastX, lastY, toX, toY,
	       0, top, left, bottom, DRAW_BORDER);
	render(lastX, lastY, toX, toY,
	       left, top, right, bottom, DRAW_DISPLAY);
	render(lastX, lastY, toX, toY,
		   right, top, V9990::UC_TICKS_PER_LINE - LEFT_BLANK, bottom,
		   DRAW_BORDER);

	// bottom border
	render(lastX, lastY, toX, toY,
		   0, bottom, V9990::UC_TICKS_PER_LINE - LEFT_BLANK, nofLines,
		   DRAW_BORDER);

	lastX = toX;
	lastY = toY;
}

void V9990PixelRenderer::draw(int fromX, int fromY, int toX, int toY,
                              DrawType type)
{
	PRT_DEBUG("V9990PixelRenderer::draw(" << dec <<
			fromX << "," << fromY << "," << toX << "," << toY << ","
			<< ((type == DRAW_BORDER)? "BORDER": "DISPLAY") << ")");

	int displayX = (fromX - leftBorderPeriod); // TODO depend on clock
	int displayY = (fromY - topBorderPeriod);
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
		leftBorderPeriod =  112;
		topBorderPeriod  =   14;
		displayPeriod    =  212;
		displayWidth     = 2048;
		nofLines         =  240;
		break;
	case B0:
	case B2:
	case B4:
		leftBorderPeriod =    0;
		topBorderPeriod  =    0;
		displayPeriod    =  240;
		displayWidth     = V9990::UC_TICKS_PER_LINE - LEFT_BLANK;
		nofLines         =  240;
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
