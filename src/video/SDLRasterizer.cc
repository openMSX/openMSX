// $Id$

#include "SDLRasterizer.hh"
#include "VDP.hh"
#include "VDPVRAM.hh"
#include "RawFrame.hh"
#include "Display.hh"
#include "Renderer.hh"
#include "RenderSettings.hh"
#include "PostProcessor.hh"
#include "FloatSetting.hh"
#include "MemoryOps.hh"
#include "MSXMotherBoard.hh"
#include "VisibleSurface.hh"
#include <algorithm>
#include <cassert>

namespace openmsx {

/** VDP ticks between start of line and start of left border.
  */
static const int TICKS_LEFT_BORDER = 100 + 102;

/** The middle of the visible (display + borders) part of a line,
  * expressed in VDP ticks since the start of the line.
  * TODO: Move this to a central location?
  */
static const int TICKS_VISIBLE_MIDDLE =
	TICKS_LEFT_BORDER + (VDP::TICKS_PER_LINE - TICKS_LEFT_BORDER - 27) / 2;

template <class Pixel>
inline int SDLRasterizer<Pixel>::translateX(int absoluteX, bool narrow)
{
	int maxX = narrow ? 640 : 320;
	if (absoluteX == VDP::TICKS_PER_LINE) return maxX;

	// Note: The ROUND_MASK forces the ticks to a pixel (2-tick) boundary.
	//       If this is not done, rounding errors will occur.
	//       This is especially tricky because division of a negative number
	//       is rounded towards zero instead of down.
	const int ROUND_MASK = narrow ? ~1 : ~3;
	int screenX =
		((absoluteX & ROUND_MASK) - (TICKS_VISIBLE_MIDDLE & ROUND_MASK))
		/ (narrow ? 2 : 4)
		+ maxX / 2;
	return std::max(screenX, 0);
}

template <class Pixel>
inline void SDLRasterizer<Pixel>::renderBitmapLine(
	byte mode, int vramLine)
{
	if (lineValidInMode[vramLine] != mode) {
		const byte* vramPtr =
			vram.bitmapCacheWindow.readArea(vramLine << 7);
		bitmapConverter.convertLine(
			bitmapDisplayCache->getLinePtr(vramLine, (Pixel*)0),
			vramPtr);
		lineValidInMode[vramLine] = mode;
	}
}

template <class Pixel>
inline void SDLRasterizer<Pixel>::renderBitmapLines(
	byte line, int count)
{
	byte mode = vdp.getDisplayMode().getByte();
	// Which bits in the name mask determine the page?
	int pageMask = 0x200 | vdp.getEvenOddMask();
	int nameMask = vram.nameTable.getMask() >> 7;

	while (count--) {
		// TODO: Optimise addr and line; too many conversions right now.
		int vramLine = nameMask & (pageMask | line);
		renderBitmapLine(mode, vramLine);
		if (vdp.isMultiPageScrolling()) {
			vramLine &= ~0x100;
			renderBitmapLine(mode, vramLine);
		}
		line++; // is a byte, so wraps at 256
	}
}

template <class Pixel>
inline void SDLRasterizer<Pixel>::renderPlanarBitmapLine(
	byte mode, int vramLine)
{
	if ( lineValidInMode[vramLine] != mode
	|| lineValidInMode[vramLine | 512] != mode ) {
		int addr0 = vramLine << 7;
		int addr1 = addr0 | 0x10000;
		const byte* vramPtr0 =
			vram.bitmapCacheWindow.readArea(addr0);
		const byte* vramPtr1 =
			vram.bitmapCacheWindow.readArea(addr1);
		bitmapConverter.convertLinePlanar(
			bitmapDisplayCache->getLinePtr(vramLine, (Pixel*)0),
			vramPtr0, vramPtr1
			);
		lineValidInMode[vramLine] =
			lineValidInMode[vramLine | 512] = mode;
	}
}

template <class Pixel>
inline void SDLRasterizer<Pixel>::renderPlanarBitmapLines(
	byte line, int count)
{
	byte mode = vdp.getDisplayMode().getByte();
	// Which bits in the name mask determine the page?
	int pageMask = vdp.getEvenOddMask();
	while (count--) {
		// TODO: Optimise addr and line; too many conversions right now.
		int vramLine = (vram.nameTable.getMask() >> 7) & (pageMask | line);
		renderPlanarBitmapLine(mode, vramLine);
		if (vdp.isMultiPageScrolling()) {
			vramLine &= ~0x100;
			renderPlanarBitmapLine(mode, vramLine);
		}
		line++; // is a byte, so wraps at 256
	}
}

template <class Pixel>
inline void SDLRasterizer<Pixel>::renderCharacterLines(
	byte line, int count)
{
	while (count--) {
		// Render this line.
		characterConverter.convertLine(
			charDisplayCache->getLinePtr(line, (Pixel*)0), line);
		line++; // is a byte, so wraps at 256
	}
}

template <class Pixel>
SDLRasterizer<Pixel>::SDLRasterizer(
		VDP& vdp_, Display& display, VisibleSurface& screen_)
	: vdp(vdp_), vram(vdp.getVRAM())
	, screen(screen_)
	, postProcessor(new PostProcessor<Pixel>(
		vdp.getMotherBoard().getCommandController(),
		display, screen_, VIDEO_MSX, 640, 240
		))
	, renderSettings(display.getRenderSettings())
	, characterConverter(vdp, palFg, palBg)
	, bitmapConverter(palFg, PALETTE256, V9958_COLOURS)
	, spriteConverter(vdp.getSpriteChecker())
{
	workFrame = new RawFrame(screen.getFormat(), 640, 240);

	// Create display caches.
	charDisplayCache = new RawFrame(
		screen.getFormat(), 512, vdp.isMSX1VDP() ? 192 : 256
		);
	bitmapDisplayCache = vdp.isMSX1VDP()
		? NULL
		: new RawFrame(screen.getFormat(), 512, 256 * 4);

	// Init the palette.
	precalcPalette();

	// Initialize palette (avoid UMR)
	if (!vdp.isMSX1VDP()) {
		for (int i = 0; i < 16; ++i) {
			palFg[i] = palFg[i + 16] = palBg[i] =
				V9938_COLOURS[0][0][0];
		}
	}

	renderSettings.getGamma()     .attach(*this);
	renderSettings.getBrightness().attach(*this);
	renderSettings.getContrast()  .attach(*this);
}

template <class Pixel>
SDLRasterizer<Pixel>::~SDLRasterizer()
{
	renderSettings.getGamma()     .detach(*this);
	renderSettings.getBrightness().detach(*this);
	renderSettings.getContrast()  .detach(*this);

	delete bitmapDisplayCache;
	delete charDisplayCache;
	delete workFrame;
}

template <class Pixel>
bool SDLRasterizer<Pixel>::isActive()
{
	return postProcessor->getZ() != Layer::Z_MSX_PASSIVE;
}

template <class Pixel>
void SDLRasterizer<Pixel>::reset()
{
	// Init renderer state.
	setDisplayMode(vdp.getDisplayMode());
	spriteConverter.setTransparency(vdp.getTransparency());

	// Invalidate bitmap cache.
	memset(lineValidInMode, 0xFF, sizeof(lineValidInMode));

	resetPalette();
}

template <class Pixel>
void SDLRasterizer<Pixel>::resetPalette()
{
	if (!vdp.isMSX1VDP()) {
		// Reset the palette.
		for (int i = 0; i < 16; i++) {
			setPalette(i, vdp.getPalette(i));
		}
	}
}

template <class Pixel>
void SDLRasterizer<Pixel>::frameStart()
{
	workFrame = postProcessor->rotateFrames(workFrame,
	    vdp.isInterlaced()
	    ? (vdp.getEvenOdd() ? FrameSource::FIELD_ODD : FrameSource::FIELD_EVEN)
	    : FrameSource::FIELD_NONINTERLACED);

	// Calculate line to render at top of screen.
	// Make sure the display area is centered.
	// 240 - 212 = 28 lines available for top/bottom border; 14 each.
	// NTSC: display at [32..244),
	// PAL:  display at [59..271).
	lineRenderTop = vdp.isPalTiming() ? 59 - 14 : 32 - 14;
}

template <class Pixel>
void SDLRasterizer<Pixel>::frameEnd()
{
	// Nothing to do.
}

template <class Pixel>
void SDLRasterizer<Pixel>::setDisplayMode(DisplayMode mode)
{
	if (mode.isBitmapMode()) {
		bitmapConverter.setDisplayMode(mode);
	} else {
		characterConverter.setDisplayMode(mode);
	}
	precalcColourIndex0(mode, vdp.getTransparency(),
	                    vdp.getBackgroundColour());
	spriteConverter.setDisplayMode(mode);
	spriteConverter.setPalette(
		mode.getByte() == DisplayMode::GRAPHIC7 ? palGraphic7Sprites : palBg
		);
}

template <class Pixel>
void SDLRasterizer<Pixel>::setPalette(
	int index, int grb)
{
	// Update SDL colours in palette.
	Pixel newColor = V9938_COLOURS[(grb >> 4) & 7][grb >> 8][grb & 7];
	if ((palFg[index     ] != newColor) ||
	    (palFg[index + 16] != newColor) ||
	    (palBg[index     ] != newColor)) {
		palFg[index     ] = newColor;
		palFg[index + 16] = newColor;
		palBg[index     ] = newColor;
		// Any line containing pixels of this colour must be repainted.
		// We don't know which lines contain which colours,
		// so we have to repaint them all.
		memset(lineValidInMode, 0xFF, sizeof(lineValidInMode));
	}

	precalcColourIndex0(vdp.getDisplayMode(), vdp.getTransparency(),
	                    vdp.getBackgroundColour());
}

template <class Pixel>
void SDLRasterizer<Pixel>::setBackgroundColour(int index)
{
	precalcColourIndex0(
		vdp.getDisplayMode(), vdp.getTransparency(), index);
}

template <class Pixel>
void SDLRasterizer<Pixel>::setTransparency(bool enabled)
{
	spriteConverter.setTransparency(enabled);
	precalcColourIndex0(
		vdp.getDisplayMode(), enabled, vdp.getBackgroundColour());
}

template <class Pixel>
void SDLRasterizer<Pixel>::precalcPalette()
{
	if (vdp.isMSX1VDP()) {
		// Fixed palette.
		for (int i = 0; i < 16; i++) {
			const byte* rgb = Renderer::TMS99X8A_PALETTE[i];
			double dr = rgb[0] / 255.0;
			double dg = rgb[1] / 255.0;
			double db = rgb[2] / 255.0;
			renderSettings.transformRGB(dr, dg, db);
			palFg[i] = palFg[i + 16] = palBg[i] =
				screen.mapRGB(dr, dg, db);
		}
	} else {
		// Precalculate palette for V9938 colours.
		for (int r = 0; r < 8; r++) {
			for (int g = 0; g < 8; g++) {
				for (int b = 0; b < 8; b++) {
					double dr = r / 7.0;
					double dg = g / 7.0;
					double db = b / 7.0;
					renderSettings.transformRGB(dr, dg, db);
					V9938_COLOURS[r][g][b] =
						screen.mapRGB(dr, dg, db);
				}
			}
		}
		// Precalculate palette for V9958 colours.
		for (int r = 0; r < 32; r++) {
			for (int g = 0; g < 32; g++) {
				for (int b = 0; b < 32; b++) {
					double dr = r / 31.0;
					double dg = g / 31.0;
					double db = b / 31.0;
					renderSettings.transformRGB(dr, dg, db);
					V9958_COLOURS[(r<<10) + (g<<5) + b] =
						screen.mapRGB(dr, dg, db);
				}
			}
		}
		// Precalculate Graphic 7 bitmap palette.
		for (int i = 0; i < 256; i++) {
			PALETTE256[i] = V9938_COLOURS
				[(i & 0x1C) >> 2]
				[(i & 0xE0) >> 5]
				[(i & 0x03) == 3 ? 7 : (i & 0x03) * 2];
		}
		// Precalculate Graphic 7 sprite palette.
		for (int i = 0; i < 16; i++) {
			word grb = Renderer::GRAPHIC7_SPRITE_PALETTE[i];
			palGraphic7Sprites[i] =
				V9938_COLOURS[(grb >> 4) & 7][grb >> 8][grb & 7];
		}
	}
}

template <class Pixel>
void SDLRasterizer<Pixel>::precalcColourIndex0(
	DisplayMode mode, bool transparency, byte bgcolorIndex)
{
	// Graphic7 mode doesn't use transparency.
	if (mode.getByte() == DisplayMode::GRAPHIC7) {
		transparency = false;
	}

	int tpIndex = transparency ? bgcolorIndex : 0;
	if (mode.getBase() != DisplayMode::GRAPHIC5) {
		if (palFg[0] != palBg[tpIndex]) {
			palFg[0] = palBg[tpIndex];

			// Any line containing pixels of colour 0 must be repainted.
			// We don't know which lines contain such pixels,
			// so we have to repaint them all.
			memset(lineValidInMode, 0xFF, sizeof(lineValidInMode));
		}
	} else {
		if ((palFg[ 0] != palBg[tpIndex >> 2]) ||
		    (palFg[16] != palBg[tpIndex &  3])) {
			palFg[ 0] = palBg[tpIndex >> 2];
			palFg[16] = palBg[tpIndex &  3];

			// Any line containing pixels of colour 0 must be repainted.
			// We don't know which lines contain such pixels,
			// so we have to repaint them all.
			memset(lineValidInMode, 0xFF, sizeof(lineValidInMode));
		}
	}
}

template <class Pixel>
void SDLRasterizer<Pixel>::updateVRAMCache(int address)
{
	lineValidInMode[address >> 7] = 0xFF;
}

template <class Pixel>
void SDLRasterizer<Pixel>::drawBorder(
	int fromX, int fromY, int limitX, int limitY)
{
	DisplayMode mode = vdp.getDisplayMode();
	byte modeBase = mode.getBase();
	int bgColor = vdp.getBackgroundColour();
	Pixel border0, border1;
	if (modeBase == DisplayMode::GRAPHIC5) {
		// border in SCREEN6 has separate color for even and odd pixels.
		// TODO odd/even swapped?
		border0 = palBg[(bgColor & 0x0C) >> 2];
		border1 = palBg[(bgColor & 0x03) >> 0];
	} else if (modeBase == DisplayMode::GRAPHIC7) {
		border0 = border1 = PALETTE256[bgColor];
	} else {
		border0 = border1 = palBg[bgColor & 0x0F];
	}

	int startY = std::max(fromY - lineRenderTop, 0);
	int endY = std::min(limitY - lineRenderTop, 240);
	if ((fromX == 0) && (limitX == VDP::TICKS_PER_LINE)) {
		for (int y = startY; y < endY; y++) {
			workFrame->setBlank(y, border0, border1);
		}
		return;
	}

	unsigned lineWidth = mode.getLineWidth();
	unsigned x = translateX(fromX, (lineWidth == 512));
	unsigned num = translateX(limitX, (lineWidth == 512)) - x;
	for (int y = startY; y < endY; ++y) {
		MemoryOps::memset_2<Pixel, MemoryOps::NO_STREAMING>(
			workFrame->getLinePtr(y, (Pixel*)0) + x, num, border0, border1);
		workFrame->setLineWidth(y, (lineWidth == 512) ? 640 : 320);
	}
}

template <class Pixel>
void SDLRasterizer<Pixel>::drawDisplay(
	int /*fromX*/, int fromY,
	int displayX, int displayY,
	int displayWidth, int displayHeight
) {
	DisplayMode mode = vdp.getDisplayMode();
	unsigned lineWidth = mode.getLineWidth();
	if (lineWidth == 256) {
		int endX = displayX + displayWidth;
		displayX /= 2;
		displayWidth = endX / 2 - displayX;
	}

	// Clip to screen area.
	int screenLimitY = std::min(
		fromY + displayHeight - lineRenderTop,
		240
		);
	int screenY = fromY - lineRenderTop;
	if (screenY < 0) {
		displayY -= screenY;
		fromY = lineRenderTop;
		screenY = 0;
	}
	displayHeight = screenLimitY - screenY;
	if (displayHeight <= 0) return;

	int leftBackground =
		translateX(vdp.getLeftBackground(), lineWidth == 512);
	// TODO: Find out why this causes 1-pixel jitter:
	//dest.x = translateX(fromX);
	int hScroll =
		  mode.isTextMode()
		? 0
		: 8 * (lineWidth / 256) * (vdp.getHorizontalScrollHigh() & 0x1F);

	// Page border is display X coordinate where to stop drawing current page.
	// This is either the multi page split point, or the right edge of the
	// rectangle to draw, whichever comes first.
	// Note that it is possible for pageBorder to be to the left of displayX,
	// in that case only the second page should be drawn.
	int pageBorder = displayX + displayWidth;
	int scrollPage1, scrollPage2;
	if (vdp.isMultiPageScrolling()) {
		scrollPage1 = vdp.getHorizontalScrollHigh() >> 5;
		scrollPage2 = scrollPage1 ^ 1;
	} else {
		scrollPage1 = 0;
		scrollPage2 = 0;
	}
	// Because SDL blits do not wrap, unlike GL textures, the pageBorder is
	// also used if multi page is disabled.
	int pageSplit = lineWidth - hScroll;
	if (pageSplit < pageBorder) {
		pageBorder = pageSplit;
	}

	if (mode.isBitmapMode()) {
		// Bring bitmap cache up to date.
		if (mode.isPlanar()) {
			renderPlanarBitmapLines(displayY, displayHeight);
		} else {
			renderBitmapLines(displayY, displayHeight);
		}

		// Which bits in the name mask determine the page?
		int pageMaskOdd = (mode.isPlanar() ? 0x000 : 0x200) |
		                  vdp.getEvenOddMask();
		int pageMaskEven = vdp.isMultiPageScrolling()
		                 ? (pageMaskOdd & ~0x100)
		                 : pageMaskOdd;

		// Copy from cache to screen.
		for (int y = screenY; y < screenLimitY; y++) {
			const int vramLine[2] = {
				(vram.nameTable.getMask() >> 7) & (pageMaskEven | displayY),
				(vram.nameTable.getMask() >> 7) & (pageMaskOdd  | displayY)
				};

			int firstPageWidth = pageBorder - displayX;
			if (firstPageWidth > 0) {
				const Pixel* src = bitmapDisplayCache->getLinePtr(
					vramLine[scrollPage1], (Pixel*)0) +
						displayX + hScroll;
				Pixel* dst = workFrame->getLinePtr(y, (Pixel*)0)
				           + leftBackground + displayX;
				memcpy(dst, src, firstPageWidth * sizeof(Pixel));
			} else {
				firstPageWidth = 0;
			}
			if (firstPageWidth < displayWidth) {
				unsigned x = displayX < pageBorder
					? 0 : displayX + hScroll - lineWidth;
				unsigned num = displayWidth - firstPageWidth;
				const Pixel* src = bitmapDisplayCache->getLinePtr(
					vramLine[scrollPage2], (Pixel*)0) + x;
				Pixel* dst = workFrame->getLinePtr(y, (Pixel*)0)
				           + leftBackground + displayX
					   + firstPageWidth;
				memcpy(dst, src, num * sizeof(Pixel));
			}
			workFrame->setLineWidth(y, (lineWidth == 512) ? 640 : 320);

			displayY = (displayY + 1) & 255;
		}
	} else {
		renderCharacterLines(displayY, displayHeight);

		// TODO: Implement horizontal scroll high.
		for (int y = screenY; y < screenLimitY; y++) {
			assert(!vdp.isMSX1VDP() || displayY < 192);

			const Pixel* src = charDisplayCache->getLinePtr(
				displayY, (Pixel*)0) + displayX;
			Pixel* dst = workFrame->getLinePtr(y, (Pixel*)0)
			           + leftBackground + displayX;
			memcpy(dst, src, displayWidth * sizeof(Pixel));

			workFrame->setLineWidth(y, (lineWidth == 512) ? 640 : 320);
			displayY = (displayY + 1) & 255;
		}
	}
}

template <class Pixel>
void SDLRasterizer<Pixel>::drawSprites(
	int /*fromX*/, int fromY,
	int displayX, int displayY,
	int displayWidth, int displayHeight
) {
	// Clip to screen area.
	// TODO: Code duplicated from drawDisplay.
	int screenLimitY = std::min(
		fromY + displayHeight - lineRenderTop,
		240
		);
	int screenY = fromY - lineRenderTop;
	if (screenY < 0) {
		displayY -= screenY;
		fromY = lineRenderTop;
		screenY = 0;
	}
	displayHeight = screenLimitY - screenY;
	if (displayHeight <= 0) return;

	// Render sprites.
	// TODO: Call different SpriteConverter methods depending on narrow/wide
	//       pixels in this display mode?
	int spriteMode = vdp.getDisplayMode().getSpriteMode();
	int displayLimitX = displayX + displayWidth;
	int limitY = fromY + displayHeight;
	int screenX = translateX(
		vdp.getLeftSprites(),
		vdp.getDisplayMode().getLineWidth() == 512
		);
	for (int y = fromY; y < limitY; y++, screenY++) {
		Pixel* pixelPtr = workFrame->getLinePtr(screenY, (Pixel*)0) + screenX;
		if (spriteMode == 1) {
			spriteConverter.drawMode1(y, displayX, displayLimitX, pixelPtr);
		} else {
			spriteConverter.drawMode2(y, displayX, displayLimitX, pixelPtr);
		}
	}
}

template <class Pixel>
void SDLRasterizer<Pixel>::update(const Setting& setting)
{
	if ((&setting == &renderSettings.getGamma()) ||
	    (&setting == &renderSettings.getBrightness()) ||
	    (&setting == &renderSettings.getContrast())) {
		precalcPalette();
		resetPalette();

		// Invalidate bitmap cache (still needed for non-palette modes)
		memset(lineValidInMode, 0xFF, sizeof(lineValidInMode));
	}
}


// Force template instantiation.
template class SDLRasterizer<word>;
template class SDLRasterizer<unsigned>;

} // namespace openmsx
