// $Id$

#include "PixelRenderer.hh"
#include "VDP.hh"
#include "VDPVRAM.hh"
#include "SpriteChecker.hh"
#include "RealTime.hh"

/*
TODO:
- Move accuracy handling here. (from Renderer).
- Move full screen handling here? (from Renderer)
- Is it possible to do some for of dirty checking here?
  And is it a good idea?
- What happens to nextX and nextY on accuracy changes?
*/

/** Line number where top border starts.
  * This is independent of PAL/NTSC timing or number of lines per screen.
  */
static const int LINE_TOP_BORDER = 3 + 13;

inline void PixelRenderer::renderUntil(const EmuTime &time)
{
	int limitTicks = vdp->getTicksThisFrame(time);

	switch (accuracy) {
	case ACC_PIXEL: {
		int limitX = limitTicks % VDP::TICKS_PER_LINE;
		int limitY = limitTicks / VDP::TICKS_PER_LINE;
		// Note: Rounding errors might push limitTicks beyond
		//       #lines * TICKS_PER_LINE, but never more than a line beyond.
		// TODO: Get rid of rounding errors altogether.
		assert(limitY <= (vdp->isPalTiming() ? 313 : 262));

		// Split in rectangles.
		if (limitY == nextY) {
			// All on a single line.
			drawArea(nextX, nextY, limitX, nextY + 1);
		} else {
			// Finish partial top line.
			drawArea(nextX, nextY, VDP::TICKS_PER_LINE, nextY + 1);
			nextY++;
			// Draw full-width middle part (multiple lines).
			if (limitY != nextY) {
				drawArea(0, nextY, VDP::TICKS_PER_LINE, limitY);
				nextY = limitY;
			}
			// Start partial bottom line.
			drawArea(0, nextY, limitX, nextY + 1);
		}

		nextX = limitX;
		break;
	}
	case ACC_LINE: {
		// TODO: Find a better sampling point than start-of-line.
		int limitY = limitTicks / VDP::TICKS_PER_LINE;
		// Note: Rounding errors might push limitTicks beyond
		//       #lines * TICKS_PER_LINE, but never more than a line beyond.
		// TODO: Get rid of rounding errors altogether.
		assert(limitY <= (vdp->isPalTiming() ? 313 : 262));
		if (limitY != nextY) {
			drawArea(0, nextY, VDP::TICKS_PER_LINE, limitY);
			nextY = limitY ;
		}
		break;
	}
	case ACC_SCREEN: {
		// TODO
		break;
	}
	default:
		assert(false);
	}
}

PixelRenderer::PixelRenderer(VDP *vdp, bool fullScreen, const EmuTime &time)
	: Renderer(fullScreen)
{
	this->vdp = vdp;
	vram = vdp->getVRAM();
	spriteChecker = vdp->getSpriteChecker();
	// TODO: Store current time.
	//       Does the renderer actually have to keep time?
	//       Keeping render position should be good enough.

	// Now we're ready to start rendering the first frame.
	frameStart(time);
}

PixelRenderer::~PixelRenderer()
{
}

void PixelRenderer::frameStart(const EmuTime &time)
{
	//cerr << "timing: " << (vdp->isPalTiming() ? "PAL" : "NTSC") << "\n";

	// Calculate line to render at top of screen.
	// Make sure the display area is centered.
	// 240 - 212 = 28 lines available for top/bottom border; 14 each.
	// NTSC: display at [32..244),
	// PAL:  display at [59..271).
	// TODO: Use screen lines instead.
	lineRenderTop = vdp->isPalTiming() ? 59 - 14 : 32 - 14;

	// Calculate important moments in frame rendering.
	lineBottomErase = vdp->isPalTiming() ? 313 - 3 : 262 - 3;
	nextX = 0;
	nextY = 0;
}

void PixelRenderer::putImage(const EmuTime &time)
{
	// Render changes from this last frame.
	sync(time);

	// Let underlying graphics system finish rendering this frame.
	finishFrame();

	// The screen will be locked for a while, so now is a good time
	// to perform real time sync.
	RealTime::instance()->sync();
}

