// $Id$

#include <sstream>
#include <cassert>
#include "PixelRenderer.hh"
#include "VDP.hh"
#include "VDPVRAM.hh"
#include "SpriteChecker.hh"
#include "RealTime.hh"

namespace openmsx {

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
	case DRAW_SPRITES: {
		// Calculate display coordinates.
		int displayX = (startX - vdp->getLeftSprites()) / 2;
		int displayY = startY - vdp->getLineZero();
		if (!vdp->getDisplayMode().isTextMode()) {
			displayY += vdp->getVerticalScroll();
		}
		displayY &= 255; // Page wrap.
		int displayWidth = (endX - (startX & ~1)) / 2;
		int displayHeight = endY - startY;
		
		assert(0 <= displayX);
		assert(displayX + displayWidth <= 512);

		if (drawType == DRAW_DISPLAY) {
			drawDisplay(
				startX, startY,
				displayX - vdp->getHorizontalScrollLow() * 2, displayY,
				displayWidth, displayHeight
				);
		} else { // DRAW_SPRITES
			drawSprites(
				startX, startY,
				displayX / 2, displayY,
				(displayWidth + 1) / 2, displayHeight
				);
		}
		break;
	}
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

PixelRenderer::PixelRenderer(RendererFactory::RendererID id, VDP *vdp)
	: Renderer(id)
{
	this->vdp = vdp;
	vram = vdp->getVRAM();
	spriteChecker = vdp->getSpriteChecker();

	curFrameSkip = 0;
	frameSkipShortAvg = 5.0;
	frameSkipLongAvg  = 100.0;
	frameSkipDelay = 0;
	while (!buffer.isFull()) {
		buffer.addFront(1.0);
	}

	settings->getFrameSkip()->addListener(this);
}

PixelRenderer::~PixelRenderer()
{
	settings->getFrameSkip()->removeListener(this);
}

void PixelRenderer::reset(const EmuTime &time)
{
	displayEnabled = vdp->isDisplayEnabled();
	frameStart(time);
}

void PixelRenderer::updateDisplayEnabled(bool enabled, const EmuTime &time)
{
	sync(time, true);
	displayEnabled = enabled;
}

void PixelRenderer::frameStart(const EmuTime &time)
{
	accuracy = settings->getAccuracy()->getValue();

	nextX = 0;
	nextY = 0;

	if (--curFrameSkip < 0) {
		curFrameSkip = settings->getFrameSkip()->getValue().getFrameSkip();
	}
}

void PixelRenderer::putImage(const EmuTime &time, bool store)
{
	// Render changes from this last frame.
	sync(time, true);

	// Let underlying graphics system finish rendering this frame.
	if (curFrameSkip == 0) finishFrame(store);

	// The screen will be locked for a while, so now is a good time
	// to perform real time sync.
	float factor = RealTime::instance()->sync(time);

	if (settings->getFrameSkip()->getValue().isAutoFrameSkip()) {
		int frameSkip = settings->getFrameSkip()->getValue().getFrameSkip();
		//PRT_DEBUG("FrameSkip " << frameSkip);
		frameSkipShortAvg += (factor - buffer[4]);	// sum last 5
		frameSkipLongAvg  += (factor - buffer[99]);	// sum last 100
		buffer.removeBack();
		buffer.addFront(factor);

		if (frameSkipDelay) {
			// recently changed frameSkip, give time to stabilize
			frameSkipDelay--;
		} else {
			if (frameSkipShortAvg > 5.25  && frameSkip < 30) {
				// over the last 5 frames we where on average
				// ~5% too slow, increase frameSkip
				settings->getFrameSkip()->setValue(
					settings->getFrameSkip()->getValue().modify(+1)
					);
				frameSkipDelay = 25;
			} else if (frameSkipLongAvg < 50.0 && frameSkip > 0) {
				// over the last 100 frames we where on average
				// ~50% too fast, decrease frameSkip
				settings->getFrameSkip()->setValue(
					settings->getFrameSkip()->getValue().modify(-1)
					);
				frameSkipDelay = 250;
			}
		}
	}
}

void PixelRenderer::updateHorizontalScrollLow(
	byte scroll, const EmuTime &time
) {
	sync(time);
}

void PixelRenderer::updateHorizontalScrollHigh(
	byte scroll, const EmuTime &time
) {
	sync(time);
}

void PixelRenderer::updateBorderMask(
	bool masked, const EmuTime &time
) {
	sync(time);
}

void PixelRenderer::updateMultiPage(
	bool multiPage, const EmuTime &time
) {
	sync(time);
}

void PixelRenderer::updateSpritesEnabled(
	bool enabled, const EmuTime &time
) {
	sync(time);
}

void PixelRenderer::updateVRAM(int offset, const EmuTime &time) {
	// If display is disabled, VRAM changes will not affect the
	// renderer output, therefore sync is not necessary.
	// TODO: Have bitmapVisibleWindow disabled in this case.
	if (vdp->isDisplayEnabled() && curFrameSkip == 0
	&& accuracy != RenderSettings::ACC_SCREEN) {
		renderUntil(time);
	}
	// TODO: Because range is entire VRAM, offset == address.
	updateVRAMCache(offset);
}

void PixelRenderer::updateWindow(bool enabled, const EmuTime &time) {
	// The bitmapVisibleWindow has moved to a different area.
	// This update is redundant: Renderer will be notified in another way
	// as well (updateDisplayEnabled or updateNameBase, for example).
	// TODO: Can this be used as the main update method instead?
}

void PixelRenderer::renderUntil(const EmuTime &time)
{
	// Translate from time to pixel position.
	int limitTicks = vdp->getTicksThisFrame(time);
	assert(limitTicks <= vdp->getTicksPerFrame());
	int limitX, limitY;
	switch (accuracy) {
	case RenderSettings::ACC_PIXEL: {
		limitX = limitTicks % VDP::TICKS_PER_LINE;
		limitY = limitTicks / VDP::TICKS_PER_LINE;
		break;
	}
	case RenderSettings::ACC_LINE:
	case RenderSettings::ACC_SCREEN: {
		// Note: I'm not sure the rounding point is optimal.
		//       It used to be based on the left margin, but that doesn't work
		//       because the margin can change which leads to a line being
		//       rendered even though the time doesn't advance.
		limitX = 0;
		limitY =
			(limitTicks + VDP::TICKS_PER_LINE - 400) / VDP::TICKS_PER_LINE;
		break;
	}
	default:
		assert(false);
		limitX = limitY = 0; // avoid warning
	}

	// Stop here if there is nothing to render.
	// This ensures that no pixels are rendered in a series of updates that
	// happen at exactly the same time; the VDP subsystem states may be
	// inconsistent until all updates are performed.
	// Also it is a small performance optimisation.
	if (limitX == nextX && limitY == nextY) return;

	if (displayEnabled) {

		// Calculate start and end of borders in ticks since start of line.
		// The 0..7 extra horizontal scroll low pixels should be drawn in
		// border colour. These will be drawn together with the border,
		// but sprites above these pixels are clipped at the actual border
		// rather than the end of the border coloured area.
		// TODO: Move these calculations and getDisplayLeft() to VDP.
		int borderL = vdp->getLeftBorder();
		int displayL =
			vdp->isBorderMasked() ? borderL : vdp->getLeftBackground();
		int borderR = vdp->getRightBorder();

		// Left border.
		subdivide(nextX, nextY, limitX, limitY,
			0, displayL, DRAW_BORDER );
		// Display area.
		subdivide(nextX, nextY, limitX, limitY,
			displayL, borderR, DRAW_DISPLAY );
		// Sprite plane.
		if (vdp->spritesEnabled()) {
			// Update sprite checking, so that subclass can call getSprites.
			spriteChecker->checkUntil(time);
			subdivide(nextX, nextY, limitX, limitY,
				borderL, borderR, DRAW_SPRITES );
		}
		// Right border.
		subdivide(nextX, nextY, limitX, limitY,
			borderR, VDP::TICKS_PER_LINE, DRAW_BORDER );
	} else {
		subdivide(nextX, nextY, limitX, limitY,
			0, VDP::TICKS_PER_LINE, DRAW_BORDER );
	}

	nextX = limitX;
	nextY = limitY;
}

void PixelRenderer::update(const SettingLeafNode* setting) throw()
{
	curFrameSkip = 1;	// reset frameskip counter
}

} // namespace openmsx
