// $Id$

#include <algorithm>
#include <sstream>
#include <cassert>
#include "PixelRenderer.hh"
#include "VDP.hh"
#include "VDPVRAM.hh"
#include "SpriteChecker.hh"
#include "RealTime.hh"
#include "InfoCommand.hh"
#include "CommandResult.hh"

using std::max;


namespace openmsx {

/** Line number where top border starts.
  * This is independent of PAL/NTSC timing or number of lines per screen.
  */
static const int LINE_TOP_BORDER = 3 + 13;

inline void PixelRenderer::draw(
	int startX, int startY, int endX, int endY, DrawType drawType, bool atEnd)
{
	switch(drawType) {
	case DRAW_BORDER:
		drawBorder(startX, startY, endX, endY);
		break;
	case DRAW_DISPLAY:
	case DRAW_SPRITES: {
		// Calculate display coordinates.
		int zero = vdp->getLineZero();
		int displayX = (startX - vdp->getLeftSprites()) / 2;
		int displayY = startY - zero;
		if (!vdp->getDisplayMode().isTextMode()) {
			displayY += vdp->getVerticalScroll();
		} else {
			// this is not what the real VDP does, but it is good
			// enough for "Boring scroll" demo part of "Relax"
			if (displayY <= 0) {
				textModeCounter = 0;
			} else {
				displayY = (displayY & 7) | (textModeCounter * 8);
			}
			if (atEnd && (drawType == DRAW_DISPLAY)) {
				int low  = max(0, (startY - zero)) / 8;
				int high = max(0, (endY   - zero)) / 8;
				textModeCounter += (high - low);
			}
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

PixelRenderer::PixelRenderer(RendererFactory::RendererID id, VDP *vdp)
	: Renderer(id), fpsInfo(*this)
{
	this->vdp = vdp;
	vram = vdp->getVRAM();
	spriteChecker = vdp->getSpriteChecker();

	frameSkipCounter = 0;
	finishFrameDuration = 0;

	frameDurationSum = 0;
	for (unsigned i = 0; i < NUM_FRAME_DURATIONS; ++i) {
		frameDurations.addFront(20);
		frameDurationSum += 20;
	}
	prevTimeStamp = RealTime::instance().getRealTime();
	
	settings.getFrameSkip()->addListener(this);
	InfoCommand::instance().registerTopic("fps", &fpsInfo);
}

PixelRenderer::~PixelRenderer()
{
	InfoCommand::instance().unregisterTopic("fps", &fpsInfo);
	settings.getFrameSkip()->removeListener(this);
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
	accuracy = settings.getAccuracy()->getValue();

	nextX = 0;
	nextY = 0;
}

void PixelRenderer::frameEnd(const EmuTime& time)
{
	// Render changes from this last frame.
	sync(time, true);

	bool draw;
	if (frameSkipCounter == 0) {
		// limit max frame skip
		draw = true;
	} else {
		--frameSkipCounter;
		draw = RealTime::instance().timeLeft(finishFrameDuration, time);
	}
	if (draw) {
		frameSkipCounter = settings.getFrameSkip()->getValue();
		
		// Let underlying graphics system finish rendering this frame.
		unsigned time1 = RealTime::instance().getRealTime();
		finishFrame();
		unsigned time2 = RealTime::instance().getRealTime();
		finishFrameDuration = time2 - time1;
	
		// update fps statistics
		unsigned duration = time2 - prevTimeStamp;
		prevTimeStamp = time2;
		frameDurationSum += duration - frameDurations.removeBack();
		frameDurations.addFront(duration);
	}

	// The screen will be locked for a while, so now is a good time
	// to perform real time sync.
	RealTime::instance().sync(time, draw);
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

static inline bool overlap(
	int displayY0, // start of display region, inclusive
	int displayY1, // end of display region, exclusive
	int vramLine0, // start of VRAM region, inclusive
	int vramLine1  // end of VRAM region, exclusive
	// Note: Display region can wrap around: 256 -> 0.
	//       VRAM region cannot wrap around.
) {
	if (displayY0 <= displayY1) {
		if (vramLine1 > displayY0) {
			if (vramLine0 <= displayY1) return true;
		}
	} else {
		if (vramLine1 > displayY0) return true;
		if (vramLine0 <= displayY1) return true;
	}
	return false;
}

inline bool PixelRenderer::checkSync(int offset, const EmuTime &time)
{
	// TODO: Because range is entire VRAM, offset == address.

	// If display is disabled, VRAM changes will not affect the
	// renderer output, therefore sync is not necessary.
	// TODO: Have bitmapVisibleWindow disabled in this case.
	if (!vdp->isDisplayEnabled()) return false;
	//if (frameSkipCounter != 0) return false; // TODO 
	if (accuracy == RenderSettings::ACC_SCREEN) return false;

	// Calculate what display lines are scanned between current
	// renderer time and update-to time.
	// Note: displayY1 is inclusive.
	int deltaY = vdp->getVerticalScroll() - vdp->getLineZero();
	int limitY = vdp->getTicksThisFrame(time) / VDP::TICKS_PER_LINE;
	int displayY0 = (nextY + deltaY) & 255;
	int displayY1 = (limitY + deltaY) & 255;

	switch(vdp->getDisplayMode().getBase()) {
	case DisplayMode::GRAPHIC2:
	case DisplayMode::GRAPHIC3:
		if (vram->colourTable.isInside(offset)) {
			int vramQuarter = (offset & 0x1800) >> 11;
			int mask = (vram->colourTable.getMask() & 0x1800) >> 11;
			for (int i = 0; i < 4; i++) {
				if ( (i & mask) == vramQuarter
				&& overlap(displayY0, displayY1, i * 64, (i + 1) * 64) ) {
					/*fprintf(stderr,
						"colour table: %05X %04X - quarter %d\n",
						offset, offset & 0x1FFF, i
						);*/
					return true;
				}
			}
		}
		if (vram->patternTable.isInside(offset)) {
			int vramQuarter = (offset & 0x1800) >> 11;
			int mask = (vram->patternTable.getMask() & 0x1800) >> 11;
			for (int i = 0; i < 4; i++) {
				if ( (i & mask) == vramQuarter
				&& overlap(displayY0, displayY1, i * 64, (i + 1) * 64) ) {
					/*fprintf(stderr,
						"pattern table: %05X %04X - quarter %d\n",
						offset, offset & 0x1FFF, i
						);*/
					return true;
				}
			}
		}
		if (vram->nameTable.isInside(offset)) {
			int vramLine = ((offset & 0x3FF) / 32) * 8;
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
		// Is the address inside the visual page(s)?
		// TODO: Also look at which lines are touched inside pages.
		int visiblePage = vram->nameTable.getMask()
			& (0x10000 | (vdp->getEvenOddMask() << 7));
		if (vdp->isMultiPageScrolling()) {
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
		return vram->nameTable.isInside(offset)
			|| vram->colourTable.isInside(offset)
			|| vram->patternTable.isInside(offset);
	}
}

void PixelRenderer::updateVRAM(int offset, const EmuTime &time) {
	// Note: No need to sync if display is disabled, because then the
	//       output does not depend on VRAM (only on background colour).
	if (displayEnabled && checkSync(offset, time)) {
		/*
		fprintf(stderr, "vram sync @ line %d\n",
			vdp->getTicksThisFrame(time) / VDP::TICKS_PER_LINE
			);
		*/
		renderUntil(time);
	}
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
	frameSkipCounter = 1;	// reset frameskip counter
}


// class FpsInfoTopic

PixelRenderer::FpsInfoTopic::FpsInfoTopic(PixelRenderer& parent_)
	: parent(parent_)
{
}

void PixelRenderer::FpsInfoTopic::execute(const vector<string>& tokens,
                                          CommandResult& result) const throw()
{
	result.setDouble(1000000.0 * NUM_FRAME_DURATIONS / parent.frameDurationSum);
}

string PixelRenderer::FpsInfoTopic::help (const vector<string>& tokens) const
	throw()
{
	return "Returns the current rendering speed in frames per second.";
}

} // namespace openmsx
