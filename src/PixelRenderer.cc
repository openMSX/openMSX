// $Id$

#include <sstream>
#include "PixelRenderer.hh"
#include "VDP.hh"
#include "VDPVRAM.hh"
#include "SpriteChecker.hh"
#include "RealTime.hh"
#include "RenderSettings.hh"

/*
TODO:
- Move accuracy handling here. (from Renderer).
- Move full screen handling here? (from Renderer)
- Is it possible to do some for of dirty checking here?
  And is it a good idea?
*/

/** Line number where top border starts.
  * This is independent of PAL/NTSC timing or number of lines per screen.
  */
static const int LINE_TOP_BORDER = 3 + 13;

inline void PixelRenderer::draw(
	int startX, int startY, int endX, int endY, DrawType drawType )
{
	switch(drawType) {
	case DRAW_BORDER:
		drawBorder(startX, startY, endX, endY);
		break;
	case DRAW_DISPLAY:
		drawDisplay(startX, startY, endX, endY);
		break;
	default:
		assert(false);
	}
}

inline void PixelRenderer::subdivide(
	int startX, int startY, int endX, int endY, int clipL, int clipR,
	DrawType drawType )
{
	// Partial first line.
	if (startX > clipL) {
		if (startX < clipR) {
			draw(
				startX, startY,
				( startY == endY && endX < clipR
				? endX
				: clipR
				), startY + 1,
				drawType
				);
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
		draw(clipL, startY, clipR, endY, drawType);
	}
	// Actually draw last line if necessary.
	// The point of keeping top-to-bottom draw order is that it increases
	// the locality of memory references, which generally improves cache
	// hit rates.
	if (drawLast) draw(clipL, endY, endX, endY + 1, drawType);
}

inline void PixelRenderer::renderUntil(const EmuTime &time)
{
	if (curFrameSkip != 0) return;

	// Translate from time to pixel position.
	int limitTicks = vdp->getTicksThisFrame(time);
	assert(limitTicks <= vdp->getTicksPerFrame());
	int limitX, limitY;
	switch (settings->getAccuracy()->getValue()) {
	case RenderSettings::ACC_PIXEL: {
		limitX = limitTicks % VDP::TICKS_PER_LINE;
		limitY = limitTicks / VDP::TICKS_PER_LINE;
		break;
	}
	case RenderSettings::ACC_LINE: {
		// Note: I'm not sure the rounding point is optimal.
		//       It used to be based on the left margin, but that doesn't work
		//       because the margin can change which leads to a line being
		//       rendered even though the time doesn't advance.
		limitX = 0;
		limitY =
			(limitTicks + VDP::TICKS_PER_LINE - 400) / VDP::TICKS_PER_LINE;
		break;
	}
	case RenderSettings::ACC_SCREEN: {
		// TODO: Implement.
		return;
	}
	default:
		assert(false);
	}

	// Stop here if there is nothing to render.
	// This ensures that no pixels are rendered in a series of updates that
	// happen at exactly the same time; the VDP subsystem states may be
	// inconsistent until all updates are performed.
	// Also it is a small performance optimisation.
	if (limitX == nextX && limitY == nextY) return;

	if (displayEnabled) {
		// Update sprite checking, so that subclass can call getSprites.
		spriteChecker->checkUntil(time);

		// Calculate start and end of display in ticks since start of line.
		int displayL = getDisplayLeft();
		int displayR = displayL +
			(vdp->getDisplayMode().isTextMode() ? 960 : 1024);

		// Left border.
		subdivide(nextX, nextY, limitX, limitY,
			0, displayL, DRAW_BORDER );
		// Display area.
		subdivide(nextX, nextY, limitX, limitY,
			displayL, displayR, DRAW_DISPLAY );
		// Right border.
		subdivide(nextX, nextY, limitX, limitY,
			displayR, VDP::TICKS_PER_LINE, DRAW_BORDER );
	} else {
		subdivide(nextX, nextY, limitX, limitY,
			0, VDP::TICKS_PER_LINE, DRAW_BORDER );
	}

	nextX = limitX;
	nextY = limitY;
}

PixelRenderer::PixelRenderer(VDP *vdp)
	: Renderer(), frameSkipSetting(this)
{
	this->vdp = vdp;
	vram = vdp->getVRAM();
	spriteChecker = vdp->getSpriteChecker();

	frameSkipShortAvg = 10.0;
	frameSkipLongAvg  = 100.0;
	frameSkipDelay = 0;
	while (!buffer.isFull()) {
		buffer.addFront(1.0);
	}

}

PixelRenderer::~PixelRenderer()
{
}

void PixelRenderer::reset(const EmuTime &time)
{
	displayEnabled = false;
	frameStart(time);
}

void PixelRenderer::updateDisplayEnabled(bool enabled, const EmuTime &time)
{
	sync(time);
	displayEnabled = enabled;
}

void PixelRenderer::frameStart(const EmuTime &time)
{
	nextX = 0;
	nextY = 0;

	if (--curFrameSkip < 0) {
		curFrameSkip = frameSkip;
	}
}

void PixelRenderer::putImage(const EmuTime &time)
{
	// Render changes from this last frame.
	sync(time);

	// Let underlying graphics system finish rendering this frame.
	if (curFrameSkip == 0) finishFrame();

	// The screen will be locked for a while, so now is a good time
	// to perform real time sync.
	float factor = RealTime::instance()->sync(time);

	if (autoFrameSkip) {
		frameSkipShortAvg += (factor - buffer[9]);	// sum last 10
		frameSkipLongAvg  += (factor - buffer[99]);	// sum last 100
		buffer.removeBack();
		buffer.addFront(factor);

		if (frameSkipDelay) {
			// recently changed frameSkip, give time to stabilize
			frameSkipDelay--;
		} else {
			if (frameSkipShortAvg > 11.0  && frameSkip < 30) {
				// over the last 10 frames we where on average
				// ~10% too slow, increase frameSkip
				frameSkip++;
				frameSkipDelay = 100;
			} else if (frameSkipLongAvg < 65.0 && frameSkip > 0) {
				// over the last 100 frames we where on average
				// ~50% too fast, decrease frameSkip
				frameSkip--;
				frameSkipDelay = 10;
			}
		}
	}
}


// class FramsSkipSetting

PixelRenderer::FrameSkipSetting::FrameSkipSetting(PixelRenderer* renderer_)
	: Setting("frameskip", "set the amount of frameskip"),
	  renderer(renderer_)
{
	type = "0 - 100 / auto";
	
	renderer->autoFrameSkip = false;
	renderer->frameSkip = 0;
	renderer->curFrameSkip = 0;
}

std::string PixelRenderer::FrameSkipSetting::getValueString() const
{
	if (renderer->autoFrameSkip) {
		return "auto";
	} else {
		std::ostringstream out;
		out << renderer->frameSkip;
		return out.str();
	}
}

void PixelRenderer::FrameSkipSetting::setValueString(
	const std::string &valueString, const EmuTime &time)
{
	if (valueString == "auto") {
		renderer->autoFrameSkip = true;
	} else {
		int tmp = strtol(valueString.c_str(), NULL, 0);
		if ((0 <= tmp) && (tmp <= 100)) {
			renderer->autoFrameSkip = false;
			renderer->frameSkip = tmp;
			renderer->curFrameSkip = 0;
		} else {
			throw CommandException("Not a valid value");
		}
	}
}
                                                      
