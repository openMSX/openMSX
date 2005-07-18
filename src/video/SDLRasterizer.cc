// $Id$

#include "SDLRasterizer.hh"
#include "VDP.hh"
#include "VDPVRAM.hh"
#include "Renderer.hh"
#include "RenderSettings.hh"
#include "RawFrame.hh"
#include "Scaler.hh"
#include "BooleanSetting.hh"
#include "FloatSetting.hh"
#include "MemoryOps.hh"
#include <SDL.h>
#include <algorithm>
#include <cassert>
#include <cmath>

using std::max;
using std::min;
using std::string;


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

template <class Pixel, Renderer::Zoom zoom>
inline int SDLRasterizer<Pixel, zoom>::translateX(int absoluteX, bool narrow)
{
	int maxX = narrow ? WIDTH : WIDTH / LINE_ZOOM;
	if (absoluteX == VDP::TICKS_PER_LINE) return maxX;

	// Note: The ROUND_MASK forces the ticks to a pixel (2-tick) boundary.
	//       If this is not done, rounding errors will occur.
	//       This is especially tricky because division of a negative number
	//       is rounded towards zero instead of down.
	const int ROUND_MASK =
		zoom == Renderer::ZOOM_REAL && narrow ? ~1 : ~3;
	int screenX =
		((absoluteX & ROUND_MASK) - (TICKS_VISIBLE_MIDDLE & ROUND_MASK))
		/ (zoom == Renderer::ZOOM_REAL && narrow ? 2 : 4)
		+ maxX / 2;
	return max(screenX, 0);
}

template <class Pixel, Renderer::Zoom zoom>
inline Pixel* SDLRasterizer<Pixel, zoom>::getLinePtr(
	SDL_Surface* displayCache, int line)
{
	return (Pixel*)( (byte*)displayCache->pixels
		+ line * displayCache->pitch );
}

template <class Pixel, Renderer::Zoom zoom>
inline void SDLRasterizer<Pixel, zoom>::renderBitmapLine(
	byte mode, int vramLine)
{
	if (lineValidInMode[vramLine] != mode) {
		const byte* vramPtr =
			vram.bitmapCacheWindow.readArea(vramLine << 7);
		bitmapConverter.convertLine(
			getLinePtr(bitmapDisplayCache, vramLine), vramPtr );
		lineValidInMode[vramLine] = mode;
	}
}

template <class Pixel, Renderer::Zoom zoom>
inline void SDLRasterizer<Pixel, zoom>::renderBitmapLines(
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

template <class Pixel, Renderer::Zoom zoom>
inline void SDLRasterizer<Pixel, zoom>::renderPlanarBitmapLine(
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
			getLinePtr(bitmapDisplayCache, vramLine),
			vramPtr0, vramPtr1
			);
		lineValidInMode[vramLine] =
			lineValidInMode[vramLine | 512] = mode;
	}
}

template <class Pixel, Renderer::Zoom zoom>
inline void SDLRasterizer<Pixel, zoom>::renderPlanarBitmapLines(
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

template <class Pixel, Renderer::Zoom zoom>
inline void SDLRasterizer<Pixel, zoom>::renderCharacterLines(
	byte line, int count)
{
	while (count--) {
		// Render this line.
		characterConverter.convertLine(
			getLinePtr(charDisplayCache, line), line);
		line++; // is a byte, so wraps at 256
	}
}

template <class Pixel, Renderer::Zoom zoom>
SDLRasterizer<Pixel, zoom>::SDLRasterizer(VDP& vdp_, SDL_Surface* screen)
	: vdp(vdp_), vram(vdp.getVRAM())
	, blender(Blender<Pixel>::createFromFormat(screen->format))
	, characterConverter(vdp, palFg, palBg, blender)
	, bitmapConverter(palFg, PALETTE256, V9958_COLOURS, blender)
	, spriteConverter(vdp.getSpriteChecker(), blender)
{
	interlaced = false;
	this->screen = screen;
	currScalerID = (ScalerID)-1; // not a valid scaler

	// Allocate work surface.
	initFrames(true);

	// Create display caches.
	charDisplayCache = SDL_CreateRGBSurface(
		SDL_SWSURFACE,
		zoom == Renderer::ZOOM_256 ? 256 : 512,
		vdp.isMSX1VDP() ? 192 : 256,
		screen->format->BitsPerPixel,
		screen->format->Rmask,
		screen->format->Gmask,
		screen->format->Bmask,
		screen->format->Amask
		);
	bitmapDisplayCache = ( vdp.isMSX1VDP()
		? NULL
		: SDL_CreateRGBSurface(
			SDL_SWSURFACE,
			zoom == Renderer::ZOOM_256 ? 256 : 512,
			256 * 4,
			screen->format->BitsPerPixel,
			screen->format->Rmask,
			screen->format->Gmask,
			screen->format->Bmask,
			screen->format->Amask
			)
		);

	// Init the palette.
	precalcPalette(RenderSettings::instance().getGamma().getValue());

	RenderSettings::instance().getDeinterlace().addListener(this);
}

template <class Pixel, Renderer::Zoom zoom>
SDLRasterizer<Pixel, zoom>::~SDLRasterizer()
{
	RenderSettings::instance().getDeinterlace().removeListener(this);

	SDL_FreeSurface(charDisplayCache);
	if (bitmapDisplayCache) SDL_FreeSurface(bitmapDisplayCache);
	delete workFrame;
	delete currFrame;
	delete prevFrame;
}

template <class Pixel, Renderer::Zoom zoom>
void SDLRasterizer<Pixel, zoom>::paint()
{
	// All of the current postprocessing steps require hi-res.
	if (LINE_ZOOM != 2) {
		// Just copy the image as-is.
		// TODO: This is incorrect, since the image is now always 640 wide.
		if (SDL_MUSTLOCK(screen)) SDL_UnlockSurface(screen);
		SDL_BlitSurface(currFrame->getSurface(), NULL, screen, NULL);
		if (SDL_MUSTLOCK(screen)) SDL_LockSurface(screen);
		return;
	}

	const bool deinterlace =
		interlaced && RenderSettings::instance().getDeinterlace().getValue();

	// New scaler algorithm selected?
	ScalerID scalerID = RenderSettings::instance().getScaler().getValue();
	if (currScalerID != scalerID) {
		currScaler = Scaler<Pixel>::createScaler(scalerID, screen->format);
		currScalerID = scalerID;
	}

	// Scale image.
	// TODO: Check deinterlace first: now that we keep LineContent info per
	//       frame, we should take the content of two frames into account.
	const unsigned deltaY = interlaced && vdp.getEvenOdd() ? 1 : 0;
	unsigned startY = 0;
	while (startY < HEIGHT / 2) {
		const RawFrame::LineContent content = currFrame->getLineContent(startY);
		unsigned endY = startY + 1;
		while (endY < HEIGHT / 2
			&& currFrame->getLineContent(endY) == content) endY++;

		switch (content) {
		case RawFrame::LINE_BLANK: {
			// Reduce area to same-colour starting segment.
			const Pixel colour = *currFrame->getPixelPtr<Pixel>(0, startY);
			for (unsigned y = startY + 1; y < endY; y++) {
				const Pixel colour2 = *currFrame->getPixelPtr<Pixel>(0, y);
				if (colour != colour2) endY = y;
			}

			if (deinterlace) {
				// TODO: This isn't 100% accurate:
				//       on the previous frame, this area may have contained
				//       graphics instead of blank pixels.
				currScaler->scaleBlank(
					colour, screen,
					startY * 2 + deltaY, endY * 2 + deltaY );
			} else {
				currScaler->scaleBlank(
					colour, screen,
					startY * 2 + deltaY, endY * 2 + deltaY );
			}
			break;
		}
		case RawFrame::LINE_256:
			if (deinterlace) {
				for (unsigned y = startY; y < endY; y++) {
					bool odd = currFrame->isOddField();
					deinterlacer.deinterlaceLine256(
						(odd ? prevFrame : currFrame)->getSurface(), // even
						(odd ? currFrame : prevFrame)->getSurface(), // odd
						y, screen, y * 2
						);
				}
			} else {
				currScaler->scale256(
					currFrame->getSurface(),
					startY, endY, screen, startY * 2 + deltaY
					);
			}
			break;
		case RawFrame::LINE_512:
			if (deinterlace) {
				for (unsigned y = startY; y < endY; y++) {
					bool odd = currFrame->isOddField();
					deinterlacer.deinterlaceLine512(
						(odd ? prevFrame : currFrame)->getSurface(), // even
						(odd ? currFrame : prevFrame)->getSurface(), // odd
						y, screen, y * 2
						);
				}
			} else {
				currScaler->scale512(
					currFrame->getSurface(),
					startY, endY, screen, startY * 2 + deltaY
					);
			}
			break;
		default:
			assert(false);
			break;
		}

		//fprintf(stderr, "post processing lines %d-%d: %d\n",
		//	startY, endY, content );
		startY = endY;
	}
}

template <class Pixel, Renderer::Zoom zoom>
const string& SDLRasterizer<Pixel, zoom>::getName()
{
	static const string NAME = "SDLRasterizer";
	return NAME;
}

template <class Pixel, Renderer::Zoom zoom>
void SDLRasterizer<Pixel, zoom>::reset()
{
	// Init renderer state.
	setDisplayMode(vdp.getDisplayMode());
	spriteConverter.setTransparency(vdp.getTransparency());

	// Invalidate bitmap cache.
	memset(lineValidInMode, 0xFF, sizeof(lineValidInMode));

	resetPalette();
}

template <class Pixel, Renderer::Zoom zoom>
void SDLRasterizer<Pixel, zoom>::resetPalette()
{
	if (!vdp.isMSX1VDP()) {
		// Reset the palette.
		for (int i = 0; i < 16; i++) {
			setPalette(i, vdp.getPalette(i));
		}
	}
}

template <class Pixel, Renderer::Zoom zoom>
void SDLRasterizer<Pixel, zoom>::frameStart()
{
	workFrame = rotateFrames(workFrame, vdp.getEvenOdd());

	// Remember interlace status of the completed frame,
	// for use in paint().
	interlaced = vdp.isInterlaced();

	// Calculate line to render at top of screen.
	// Make sure the display area is centered.
	// 240 - 212 = 28 lines available for top/bottom border; 14 each.
	// NTSC: display at [32..244),
	// PAL:  display at [59..271).
	lineRenderTop = vdp.isPalTiming() ? 59 - 14 : 32 - 14;

	double gamma = RenderSettings::instance().getGamma().getValue();
	// (gamma != prevGamma) gives compiler warnings
	if ((gamma > prevGamma) || (gamma < prevGamma)) {
		precalcPalette(gamma);
		resetPalette();
	}
}

template <class Pixel, Renderer::Zoom zoom>
void SDLRasterizer<Pixel, zoom>::frameEnd()
{
	// Nothing to do.
}

template <class Pixel, Renderer::Zoom zoom>
void SDLRasterizer<Pixel, zoom>::setDisplayMode(DisplayMode mode)
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

template <class Pixel, Renderer::Zoom zoom>
void SDLRasterizer<Pixel, zoom>::setPalette(
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

template <class Pixel, Renderer::Zoom zoom>
void SDLRasterizer<Pixel, zoom>::setBackgroundColour(int index)
{
	precalcColourIndex0(
		vdp.getDisplayMode(), vdp.getTransparency(), index);
}

template <class Pixel, Renderer::Zoom zoom>
void SDLRasterizer<Pixel, zoom>::setTransparency(bool enabled)
{
	spriteConverter.setTransparency(enabled);
	precalcColourIndex0(
		vdp.getDisplayMode(), enabled, vdp.getBackgroundColour());
}

template <class Pixel, Renderer::Zoom zoom>
void SDLRasterizer<Pixel, zoom>::precalcPalette(double gamma)
{
	prevGamma = gamma;

	// It's gamma correction, so apply in reverse.
	gamma = 1.0 / gamma;

	if (vdp.isMSX1VDP()) {
		// Fixed palette.
		for (int i = 0; i < 16; i++) {
			const byte* rgb = Renderer::TMS99X8A_PALETTE[i];
			palFg[i] = palFg[i + 16] = palBg[i] = SDL_MapRGB(
				screen->format,
				(int)(::pow((double)rgb[0] / 255.0, gamma) * 255),
				(int)(::pow((double)rgb[1] / 255.0, gamma) * 255),
				(int)(::pow((double)rgb[2] / 255.0, gamma) * 255)
				);
		}
	} else {
		// Precalculate palette for V9938 colours.
		for (int r = 0; r < 8; r++) {
			for (int g = 0; g < 8; g++) {
				for (int b = 0; b < 8; b++) {
					V9938_COLOURS[r][g][b] = SDL_MapRGB(
						screen->format,
						(int)(::pow((double)r / 7.0, gamma) * 255),
						(int)(::pow((double)g / 7.0, gamma) * 255),
						(int)(::pow((double)b / 7.0, gamma) * 255)
						);
				}
			}
		}
		// Precalculate palette for V9958 colours.
		for (int r = 0; r < 32; r++) {
			for (int g = 0; g < 32; g++) {
				for (int b = 0; b < 32; b++) {
					V9958_COLOURS[(r<<10) + (g<<5) + b] = SDL_MapRGB(
						screen->format,
						(int)(::pow((double)r / 31.0, gamma) * 255),
						(int)(::pow((double)g / 31.0, gamma) * 255),
						(int)(::pow((double)b / 31.0, gamma) * 255)
						);
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
		// Initialize palette (avoid UMR)
		for (int i = 0; i < 16; ++i) {
			palFg[i] = palFg[i + 16] = palBg[i] = V9938_COLOURS[0][0][0];
		}
	}
}

template <class Pixel, Renderer::Zoom zoom>
void SDLRasterizer<Pixel, zoom>::precalcColourIndex0(
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

template <class Pixel, Renderer::Zoom zoom>
void SDLRasterizer<Pixel, zoom>::initFrames(bool first)
{
	// TODO: Before, we only kept prevFrame if deinterlacing was active.
	//       It is much simpler to do it always and that also leaves the way
	//       open for video effects such as the framerate control by
	//       interpolation idea from Daniel Vik.
	//       And it looks better if the user activates deinterlace when
	//       the emulator is paused.
	if (first) {
		bool oddField = vdp.getEvenOdd();
		workFrame = new RawFrame(screen->format, oddField);
		currFrame = new RawFrame(screen->format, !oddField);
		prevFrame = new RawFrame(screen->format, oddField);
	}
}

template <class Pixel, Renderer::Zoom zoom>
RawFrame* SDLRasterizer<Pixel, zoom>::rotateFrames(
	RawFrame* finishedFrame, bool oddField )
{
	RawFrame* reuseFrame = prevFrame;
	prevFrame = currFrame;
	currFrame = finishedFrame;
	reuseFrame->reinit(vdp.getEvenOdd());
	return reuseFrame;
}

template <class Pixel, Renderer::Zoom zoom>
void SDLRasterizer<Pixel, zoom>::updateVRAMCache(int address)
{
	lineValidInMode[address >> 7] = 0xFF;
}

template <class Pixel, Renderer::Zoom zoom>
void SDLRasterizer<Pixel, zoom>::drawBorder(
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
		if (zoom == Renderer::ZOOM_256) {
			border0 = border1 = blender.blend(border0, border1);
		}
	} else if (modeBase == DisplayMode::GRAPHIC7) {
		border0 = border1 = PALETTE256[bgColor];
	} else {
		border0 = border1 = palBg[bgColor & 0x0F];
	}

	int startY = max(fromY - lineRenderTop, 0);
	int endY = min(limitY - lineRenderTop, (int)HEIGHT / LINE_ZOOM);
	if (fromX == 0 && limitX == VDP::TICKS_PER_LINE) {
		// TODO: Disabled this optimisation during interlace, because
		//       lineContent administration is available only for current
		//       frame, not for previous frame, which led to glitches.
		if ((LINE_ZOOM == 2) && !interlaced &&
		    (modeBase != DisplayMode::GRAPHIC5)) {
			//fprintf(stderr, "saved: %d-%d\n", fromY, limitY);
			//int endY = rect.y + rect.h;
			//for (int y = rect.y; y < endY; y++) {
			for (int y = startY; y < endY; y++) {
				workFrame->setBlank(y, border0, border1);
			}
			return;
		}
	}

	bool narrow = mode.getLineWidth() == 512;
	unsigned x = translateX(fromX, narrow);
	unsigned num = translateX(limitX, narrow) - x;

	RawFrame::LineContent lineType = narrow ? RawFrame::LINE_512 : RawFrame::LINE_256;
	for (int y = startY; y < endY; ++y) {
		MemoryOps::memset_2<Pixel, MemoryOps::NO_STREAMING>(
			workFrame->getPixelPtr<Pixel>(x, y), num, border0, border1);
		if (LINE_ZOOM == 2) {
			workFrame->setLineContent(y, lineType);
		}
	}
}

template <class Pixel, Renderer::Zoom zoom>
void SDLRasterizer<Pixel, zoom>::drawDisplay(
	int /*fromX*/, int fromY,
	int displayX, int displayY,
	int displayWidth, int displayHeight
) {
	DisplayMode mode = vdp.getDisplayMode();
	int lineWidth = mode.getLineWidth();
	if (zoom == Renderer::ZOOM_256 || lineWidth == 256) {
		int endX = displayX + displayWidth;
		displayX /= 2;
		displayWidth = endX / 2 - displayX;
	}

	// Clip to screen area.
	int screenLimitY = min(
		fromY + displayHeight - lineRenderTop,
		(int)HEIGHT / LINE_ZOOM
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
	RawFrame::LineContent lineType = lineWidth == 256 ? RawFrame::LINE_256 : RawFrame::LINE_512;
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
		int pageMaskEven, pageMaskOdd;
		if (vdp.isMultiPageScrolling()) {
			pageMaskEven = mode.isPlanar() ? 0x000 : 0x200;
			pageMaskOdd  = pageMaskEven | 0x100;
		} else {
			pageMaskEven = pageMaskOdd =
				(mode.isPlanar() ? 0x000 : 0x200) | vdp.getEvenOddMask();
		}

		// Copy from cache to screen.
		SDL_Rect source, dest;
		for (int y = screenY; y < screenLimitY; y++) {
			const int vramLine[2] = {
				(vram.nameTable.getMask() >> 7) & (pageMaskEven | displayY),
				(vram.nameTable.getMask() >> 7) & (pageMaskOdd  | displayY)
				};

			// TODO: Can we safely use SDL_LowerBlit?
			int firstPageWidth = pageBorder - displayX;
			if (firstPageWidth > 0) {
				source.x = displayX + hScroll;
				source.y = vramLine[scrollPage1];
				source.w = firstPageWidth;
				source.h = 1;
				dest.x = leftBackground + displayX;
				dest.y = y;
				SDL_BlitSurface(
					bitmapDisplayCache, &source, workFrame->getSurface(), &dest
					);
			} else {
				firstPageWidth = 0;
			}
			if (firstPageWidth < displayWidth) {
				source.x = displayX < pageBorder
					? 0 : displayX + hScroll - lineWidth;
				source.y = vramLine[scrollPage2];
				source.w = displayWidth - firstPageWidth;
				source.h = 1;
				dest.x = leftBackground + displayX + firstPageWidth;
				dest.y = y;
				SDL_BlitSurface(
					bitmapDisplayCache, &source, workFrame->getSurface(), &dest
					);
			}
			if (LINE_ZOOM == 2) {
				workFrame->setLineContent(y, lineType);
			}

			displayY = (displayY + 1) & 255;
		}
	} else {
		renderCharacterLines(displayY, displayHeight);

		// TODO: Implement horizontal scroll high.
		SDL_Rect source, dest;
		for (int y = screenY; y < screenLimitY; y++) {
			assert(!vdp.isMSX1VDP() || displayY < 192);
			source.x = displayX;
			source.y = displayY;
			source.w = displayWidth;
			source.h = 1;
			dest.x = leftBackground + displayX;
			dest.y = y;
			// TODO: Can we safely use SDL_LowerBlit?
			// Note: return value is ignored.
			/*
			printf("plotting character cache line %d to screen line %d\n",
				source.y, dest.y);
			*/
			SDL_BlitSurface(charDisplayCache, &source, workFrame->getSurface(), &dest);
			if (LINE_ZOOM == 2) {
				workFrame->setLineContent(y, lineType);
			}
			displayY = (displayY + 1) & 255;
		}
	}
}

template <class Pixel, Renderer::Zoom zoom>
void SDLRasterizer<Pixel, zoom>::drawSprites(
	int /*fromX*/, int fromY,
	int displayX, int displayY,
	int displayWidth, int displayHeight
) {
	// Clip to screen area.
	// TODO: Code duplicated from drawDisplay.
	int screenLimitY = min(
		fromY + displayHeight - lineRenderTop,
		(int)HEIGHT / LINE_ZOOM
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
		Pixel* pixelPtr = workFrame->getPixelPtr<Pixel>(screenX, screenY);
		if (spriteMode == 1) {
			spriteConverter.drawMode1(y, displayX, displayLimitX, pixelPtr);
		} else {
			spriteConverter.drawMode2(y, displayX, displayLimitX, pixelPtr);
		}
	}
}

template <class Pixel, Renderer::Zoom zoom>
void SDLRasterizer<Pixel, zoom>::update(const Setting* setting)
{
	if (setting == &RenderSettings::instance().getDeinterlace()) {
		initFrames();
	} else {
		Rasterizer::update(setting);
	}
}


// Force template instantiation.
template class SDLRasterizer<Uint16, Renderer::ZOOM_256>;
template class SDLRasterizer<Uint16, Renderer::ZOOM_REAL>;
template class SDLRasterizer<Uint32, Renderer::ZOOM_256>;
template class SDLRasterizer<Uint32, Renderer::ZOOM_REAL>;

} // namespace openmsx
