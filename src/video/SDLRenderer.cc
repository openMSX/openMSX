// $Id$

/*
TODO:
- Implement blinking (of page mask) in bitmap modes.
- Spot references to EmuTime: if implemented correctly this class should
  not need EmuTime, since it uses absolute VDP coordinates instead.
*/

#include "SDLRenderer.hh"
#include "VDP.hh"
#include "VDPVRAM.hh"
#include "RenderSettings.hh"
#include "RealTime.hh"
#include "SDLConsole.hh"
#include "CommandConsole.hh"
#include "DebugConsole.hh"
#include <math.h>
#include "util.hh"


// Force template instantiation:
template class SDLRenderer<Uint8, Renderer::ZOOM_256>;
template class SDLRenderer<Uint8, Renderer::ZOOM_512>;
template class SDLRenderer<Uint16, Renderer::ZOOM_256>;
template class SDLRenderer<Uint16, Renderer::ZOOM_512>;
template class SDLRenderer<Uint32, Renderer::ZOOM_256>;
template class SDLRenderer<Uint32, Renderer::ZOOM_512>;

/** Dimensions of screen.
  */
static const int WIDTH = 640;
static const int HEIGHT = 480;

/** VDP ticks between start of line and start of left border.
  */
static const int TICKS_LEFT_BORDER = 100 + 102;

/** The middle of the visible (display + borders) part of a line,
  * expressed in VDP ticks since the start of the line.
  * TODO: Move this to PixelRenderer?
  *       It is not used there, but it is used by multiple subclasses.
  */
static const int TICKS_VISIBLE_MIDDLE =
	TICKS_LEFT_BORDER + (VDP::TICKS_PER_LINE - TICKS_LEFT_BORDER - 27) / 2;

template <class Pixel, Renderer::Zoom zoom>
inline int SDLRenderer<Pixel, zoom>::translateX(int absoluteX)
{
	if (absoluteX == VDP::TICKS_PER_LINE) return WIDTH;
	// Note: The ROUND_MASK forces the ticks to a pixel (2-tick) boundary.
	//       If this is not done, rounding errors will occur.
	//       This is especially tricky because division of a negative number
	//       is rounded towards zero instead of down.
	const int ROUND_MASK = (zoom == Renderer::ZOOM_256 ? ~3 : ~1);
	int screenX =
		((absoluteX & ROUND_MASK) - (TICKS_VISIBLE_MIDDLE & ROUND_MASK))
		/ (zoom == Renderer::ZOOM_256 ? 4 : 2)
		+ WIDTH / 2;
	return screenX < 0 ? 0 : screenX;
}

template <class Pixel, Renderer::Zoom zoom>
void SDLRenderer<Pixel, zoom>::finishFrame(bool store)
{
	if (store) {
		// Update screen.
		SDL_UpdateRect(screen, 0, 0, 0, 0);
		// Copy entire screen to stored image.
		SDL_BlitSurface(screen, NULL, storedImage, NULL);
	}

	// Render consoles if needed.
	console->drawConsole();
	debugger->drawConsole();

	// Update screen.
	SDL_UpdateRect(screen, 0, 0, 0, 0);
}

template <class Pixel, Renderer::Zoom zoom>
void SDLRenderer<Pixel, zoom>::putStoredImage()
{
	// Copy stored image to screen.
	SDL_BlitSurface(storedImage, NULL, screen, NULL);

	// TODO: The code below is a modified copy-paste of finishFrame.
	//       Refactor it to remove code duplication.

	// Render console if needed.
	console->drawConsole();

	// Update screen.
	SDL_UpdateRect(screen, 0, 0, 0, 0);
}

template <class Pixel, Renderer::Zoom zoom>
inline Pixel *SDLRenderer<Pixel, zoom>::getLinePtr(
	SDL_Surface *displayCache, int line)
{
	return (Pixel *)( (byte *)displayCache->pixels
		+ line * displayCache->pitch );
}

// TODO: Cache this?
template <class Pixel, Renderer::Zoom zoom>
inline Pixel SDLRenderer<Pixel, zoom>::getBorderColour()
{
	// TODO: Used knowledge of V9938 to merge two 4-bit colours
	//       into a single 8 bit colour for SCREEN8.
	//       Keep doing that or make VDP handle SCREEN8 differently?
	int baseMode = vdp->getDisplayMode().getBase();
	return
		( baseMode == DisplayMode::GRAPHIC7
		? PALETTE256[
			vdp->getBackgroundColour() | (vdp->getForegroundColour() << 4) ]
		: palBg[ baseMode == DisplayMode::GRAPHIC5
		       ? vdp->getBackgroundColour() & 3
		       : vdp->getBackgroundColour()
		       ]
		);
}

template <class Pixel, Renderer::Zoom zoom>
inline void SDLRenderer<Pixel, zoom>::renderBitmapLine(
	byte mode, int vramLine)
{
	if (lineValidInMode[vramLine] != mode) {
		const byte *vramPtr =
			vram->bitmapCacheWindow.readArea(vramLine << 7);
		bitmapConverter.convertLine(
			getLinePtr(bitmapDisplayCache, vramLine), vramPtr );
		lineValidInMode[vramLine] = mode;
	}
}

template <class Pixel, Renderer::Zoom zoom>
inline void SDLRenderer<Pixel, zoom>::renderBitmapLines(
	byte line, int count)
{
	// Lock surface, because we will access pixels directly.
	if (SDL_MUSTLOCK(bitmapDisplayCache) && SDL_LockSurface(bitmapDisplayCache) < 0) {
		// Display will be wrong, but this is not really critical.
		return;
	}

	byte mode = vdp->getDisplayMode().getByte();
	// Which bits in the name mask determine the page?
	int pageMask = 0x200 | vdp->getEvenOddMask();
	while (count--) {
		// TODO: Optimise addr and line; too many conversions right now.
		int vramLine = (vram->nameTable.getMask() >> 7) & (pageMask | line);
		renderBitmapLine(mode, vramLine);
		if (vdp->isMultiPageScrolling()) {
			vramLine &= ~0x100;
			renderBitmapLine(mode, vramLine);
		}
		line++; // is a byte, so wraps at 256
	}

	// Unlock surface.
	if (SDL_MUSTLOCK(bitmapDisplayCache)) SDL_UnlockSurface(bitmapDisplayCache);
}

template <class Pixel, Renderer::Zoom zoom>
inline void SDLRenderer<Pixel, zoom>::renderPlanarBitmapLine(
	byte mode, int vramLine)
{
	if ( lineValidInMode[vramLine] != mode
	|| lineValidInMode[vramLine | 512] != mode ) {
		int addr0 = vramLine << 7;
		int addr1 = addr0 | 0x10000;
		const byte *vramPtr0 =
			vram->bitmapCacheWindow.readArea(addr0);
		const byte *vramPtr1 =
			vram->bitmapCacheWindow.readArea(addr1);
		bitmapConverter.convertLinePlanar(
			getLinePtr(bitmapDisplayCache, vramLine),
			vramPtr0, vramPtr1
			);
		lineValidInMode[vramLine] =
			lineValidInMode[vramLine | 512] = mode;
	}
}

template <class Pixel, Renderer::Zoom zoom>
inline void SDLRenderer<Pixel, zoom>::renderPlanarBitmapLines(
	byte line, int count)
{
	// Lock surface, because we will access pixels directly.
	if (SDL_MUSTLOCK(bitmapDisplayCache) && SDL_LockSurface(bitmapDisplayCache) < 0) {
		// Display will be wrong, but this is not really critical.
		return;
	}

	byte mode = vdp->getDisplayMode().getByte();
	// Which bits in the name mask determine the page?
	int pageMask = vdp->getEvenOddMask();
	while (count--) {
		// TODO: Optimise addr and line; too many conversions right now.
		int vramLine = (vram->nameTable.getMask() >> 7) & (pageMask | line);
		renderPlanarBitmapLine(mode, vramLine);
		if (vdp->isMultiPageScrolling()) {
			vramLine &= ~0x100;
			renderPlanarBitmapLine(mode, vramLine);
		}
		line++; // is a byte, so wraps at 256
	}

	// Unlock surface.
	if (SDL_MUSTLOCK(bitmapDisplayCache)) SDL_UnlockSurface(bitmapDisplayCache);
}

template <class Pixel, Renderer::Zoom zoom>
inline void SDLRenderer<Pixel, zoom>::renderCharacterLines(
	byte line, int count)
{
	// Lock surface, because we will access pixels directly.
	if (SDL_MUSTLOCK(charDisplayCache) && SDL_LockSurface(charDisplayCache) < 0) {
		// Display will be wrong, but this is not really critical.
		return;
	}

	while (count--) {
		// Render this line.
		characterConverter.convertLine(
			getLinePtr(charDisplayCache, line), line);
		line++; // is a byte, so wraps at 256
	}

	// Unlock surface.
	if (SDL_MUSTLOCK(charDisplayCache)) SDL_UnlockSurface(charDisplayCache);
}

template <class Pixel, Renderer::Zoom zoom>
SDLRenderer<Pixel, zoom>::SDLRenderer(
	RendererFactory::RendererID id, VDP *vdp, SDL_Surface *screen)
	: PixelRenderer(id, vdp)
	, characterConverter(vdp, palFg, palBg,
		Blender<Pixel>::createFromFormat(screen->format) )
	, bitmapConverter(palFg, PALETTE256, V9958_COLOURS,
		Blender<Pixel>::createFromFormat(screen->format) )
	, spriteConverter(vdp->getSpriteChecker(),
		Blender<Pixel>::createFromFormat(screen->format) )
{
	this->screen = screen;
	console = new SDLConsole(CommandConsole::instance(), screen);
	debugger = new SDLConsole(DebugConsole::instance(), screen);
	// Allocate screen which will later contain the stored image.
	storedImage = SDL_CreateRGBSurface(
		SDL_SWSURFACE,
		WIDTH, HEIGHT,
		screen->format->BitsPerPixel,
		screen->format->Rmask,
		screen->format->Gmask,
		screen->format->Bmask,
		screen->format->Amask
		);

	// Create display caches.
	charDisplayCache = SDL_CreateRGBSurface(
		SDL_HWSURFACE,
		zoom == Renderer::ZOOM_256 ? 256 : 512,
		vdp->isMSX1VDP() ? 192 : 256,
		screen->format->BitsPerPixel,
		screen->format->Rmask,
		screen->format->Gmask,
		screen->format->Bmask,
		screen->format->Amask
		);
	bitmapDisplayCache = ( vdp->isMSX1VDP()
		? NULL
		: SDL_CreateRGBSurface(
			SDL_HWSURFACE,
			zoom == Renderer::ZOOM_256 ? 256 : 512,
			256 * 4,
			screen->format->BitsPerPixel,
			screen->format->Rmask,
			screen->format->Gmask,
			screen->format->Bmask,
			screen->format->Amask
			)
		);

	// Hide mouse cursor.
	SDL_ShowCursor(SDL_DISABLE);

	// Init the palette.
	precalcPalette(settings->getGamma()->getValue());

}

template <class Pixel, Renderer::Zoom zoom>
SDLRenderer<Pixel, zoom>::~SDLRenderer()
{
	delete console;
	delete debugger;
	SDL_FreeSurface(charDisplayCache);
	SDL_FreeSurface(bitmapDisplayCache);
	SDL_FreeSurface(storedImage);
}

template <class Pixel, Renderer::Zoom zoom>
void SDLRenderer<Pixel, zoom>::precalcPalette(float gamma)
{
	prevGamma = gamma;

	// It's gamma correction, so apply in reverse.
	gamma = 1.0 / gamma;

	if (vdp->isMSX1VDP()) {
		// Fixed palette.
		for (int i = 0; i < 16; i++) {
			const byte *rgb = TMS99X8A_PALETTE[i];
			palFg[i] = palBg[i] = SDL_MapRGB(
				screen->format,
				(int)(::pow((float)rgb[0] / 255.0, gamma) * 255),
				(int)(::pow((float)rgb[1] / 255.0, gamma) * 255),
				(int)(::pow((float)rgb[2] / 255.0, gamma) * 255));
		}
	} else {
		// Precalculate palette for V9938 colours.
		for (int r = 0; r < 8; r++) {
			for (int g = 0; g < 8; g++) {
				for (int b = 0; b < 8; b++) {
					V9938_COLOURS[r][g][b] = SDL_MapRGB(
					    screen->format,
					    (int)(::pow((float)r / 7.0, gamma) * 255),
					    (int)(::pow((float)g / 7.0, gamma) * 255),
					    (int)(::pow((float)b / 7.0, gamma) * 255));
				}
			}
		}
		// Precalculate palette for V9958 colours.
		for (int r = 0; r < 32; r++) {
			for (int g = 0; g < 32; g++) {
				for (int b = 0; b < 32; b++) {
					V9958_COLOURS[(r<<10) + (g<<5) + b] = SDL_MapRGB(
					    screen->format,
					    (int)(::pow((float)r / 31.0, gamma) * 255),
					    (int)(::pow((float)g / 31.0, gamma) * 255),
					    (int)(::pow((float)b / 31.0, gamma) * 255));
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
			word grb = GRAPHIC7_SPRITE_PALETTE[i];
			palGraphic7Sprites[i] =
				V9938_COLOURS[(grb >> 4) & 7][grb >> 8][grb & 7];
		}
	}
}

template <class Pixel, Renderer::Zoom zoom>
void SDLRenderer<Pixel, zoom>::reset(const EmuTime &time)
{
	PixelRenderer::reset(time);

	// Init renderer state.
	setDisplayMode(vdp->getDisplayMode());
	spriteConverter.setTransparency(vdp->getTransparency());

	// Invalidate bitmap cache.
	memset(lineValidInMode, 0xFF, sizeof(lineValidInMode));

	resetPalette();
}

template <class Pixel, Renderer::Zoom zoom>
void SDLRenderer<Pixel, zoom>::resetPalette()
{
	if (!vdp->isMSX1VDP()) {
		// Reset the palette.
		for (int i = 0; i < 16; i++) {
			setPalette(i, vdp->getPalette(i));
		}
	}
}

template <class Pixel, Renderer::Zoom zoom>
bool SDLRenderer<Pixel, zoom>::checkSettings()
{
	// First check this is the right renderer.
	if (!PixelRenderer::checkSettings()) return false;

	// Check full screen setting.
	bool fullScreenState = ((screen->flags & SDL_FULLSCREEN) != 0);
	bool fullScreenTarget = settings->getFullScreen()->getValue();
	if (fullScreenState == fullScreenTarget) return true;

#ifdef __WIN32__
	// Under win32, toggling full screen requires opening a new SDL screen.
	return false;
#else
	// Try to toggle full screen.
	FullScreenToggler toggler(screen);
	toggler.performToggle();
	fullScreenState =
		((((volatile SDL_Surface *)screen)->flags & SDL_FULLSCREEN) != 0);
	return fullScreenState == fullScreenTarget;
#endif
}

template <class Pixel, Renderer::Zoom zoom>
void SDLRenderer<Pixel, zoom>::frameStart(
	const EmuTime &time)
{
	// Call superclass implementation.
	PixelRenderer::frameStart(time);

	// Calculate line to render at top of screen.
	// Make sure the display area is centered.
	// 240 - 212 = 28 lines available for top/bottom border; 14 each.
	// NTSC: display at [32..244),
	// PAL:  display at [59..271).
	lineRenderTop = vdp->isPalTiming() ? 59 - 14 : 32 - 14;
	lineRenderBottom = lineRenderTop + HEIGHT / LINE_ZOOM;

	float gamma = settings->getGamma()->getValue();
	// (gamma != prevGamma) gives compiler warnings
	if ((gamma > prevGamma) || (gamma < prevGamma)) {
		precalcPalette(gamma);
		resetPalette();
	}
}

template <class Pixel, Renderer::Zoom zoom>
void SDLRenderer<Pixel, zoom>::setDisplayMode(DisplayMode mode)
{
	if (mode.isBitmapMode()) {
		bitmapConverter.setDisplayMode(mode);
	} else {
		characterConverter.setDisplayMode(mode);
	}
	spriteConverter.setNarrow(mode.isSpriteNarrow());
	spriteConverter.setPalette(
		mode.getByte() == DisplayMode::GRAPHIC7 ? palGraphic7Sprites : palBg
		);
}

template <class Pixel, Renderer::Zoom zoom>
void SDLRenderer<Pixel, zoom>::updateTransparency(
	bool enabled, const EmuTime &time)
{
	sync(time);
	spriteConverter.setTransparency(enabled);

	// Set the right palette for pixels of colour 0.
	palFg[0] = palBg[enabled ? vdp->getBackgroundColour() : 0];
	// Any line containing pixels of colour 0 must be repainted.
	// We don't know which lines contain such pixels,
	// so we have to repaint them all.
	memset(lineValidInMode, 0xFF, sizeof(lineValidInMode));
}

template <class Pixel, Renderer::Zoom zoom>
void SDLRenderer<Pixel, zoom>::updateForegroundColour(
	int colour, const EmuTime &time)
{
	sync(time);
}

template <class Pixel, Renderer::Zoom zoom>
void SDLRenderer<Pixel, zoom>::updateBackgroundColour(
	int colour, const EmuTime &time)
{
	sync(time);
	if (vdp->getTransparency()) {
		// Transparent pixels have background colour.
		palFg[0] = palBg[colour];
		// Any line containing pixels of colour 0 must be repainted.
		// We don't know which lines contain such pixels,
		// so we have to repaint them all.
		memset(lineValidInMode, 0xFF, sizeof(lineValidInMode));
	}
}

template <class Pixel, Renderer::Zoom zoom>
void SDLRenderer<Pixel, zoom>::updateBlinkForegroundColour(
	int colour, const EmuTime &time)
{
	sync(time);
}

template <class Pixel, Renderer::Zoom zoom>
void SDLRenderer<Pixel, zoom>::updateBlinkBackgroundColour(
	int colour, const EmuTime &time)
{
	sync(time);
}

template <class Pixel, Renderer::Zoom zoom>
void SDLRenderer<Pixel, zoom>::updateBlinkState(
	bool enabled, const EmuTime &time)
{
	// TODO: When the sync call is enabled, the screen flashes on
	//       every call to this method.
	//       I don't know why exactly, but it's probably related to
	//       being called at frame start.
	//sync(time);
}

template <class Pixel, Renderer::Zoom zoom>
void SDLRenderer<Pixel, zoom>::updatePalette(
	int index, int grb, const EmuTime &time)
{
	sync(time);
	setPalette(index, grb);
}

template <class Pixel, Renderer::Zoom zoom>
void SDLRenderer<Pixel, zoom>::setPalette(
	int index, int grb)
{
	// Update SDL colours in palette.
	palFg[index] = palBg[index] =
		V9938_COLOURS[(grb >> 4) & 7][grb >> 8][grb & 7];

	// Is this the background colour?
	if (vdp->getBackgroundColour() == index && vdp->getTransparency()) {
		// Transparent pixels have background colour.
		palFg[0] = palBg[vdp->getBackgroundColour()];
	}

	// Any line containing pixels of this colour must be repainted.
	// We don't know which lines contain which colours,
	// so we have to repaint them all.
	memset(lineValidInMode, 0xFF, sizeof(lineValidInMode));
}

template <class Pixel, Renderer::Zoom zoom>
void SDLRenderer<Pixel, zoom>::updateVerticalScroll(
	int scroll, const EmuTime &time)
{
	sync(time);
}

template <class Pixel, Renderer::Zoom zoom>
void SDLRenderer<Pixel, zoom>::updateHorizontalAdjust(
	int adjust, const EmuTime &time)
{
	sync(time);
}

template <class Pixel, Renderer::Zoom zoom>
void SDLRenderer<Pixel, zoom>::updateDisplayMode(
	DisplayMode mode, const EmuTime &time)
{
	sync(time);
	setDisplayMode(mode);
}

template <class Pixel, Renderer::Zoom zoom>
void SDLRenderer<Pixel, zoom>::updateNameBase(
	int addr, const EmuTime &time)
{
	sync(time);
}

template <class Pixel, Renderer::Zoom zoom>
void SDLRenderer<Pixel, zoom>::updatePatternBase(
	int addr, const EmuTime &time)
{
	sync(time);
}

template <class Pixel, Renderer::Zoom zoom>
void SDLRenderer<Pixel, zoom>::updateColourBase(
	int addr, const EmuTime &time)
{
	sync(time);
}

template <class Pixel, Renderer::Zoom zoom>
void SDLRenderer<Pixel, zoom>::updateVRAMCache(int address)
{
	lineValidInMode[address >> 7] = 0xFF;
}

template <class Pixel, Renderer::Zoom zoom>
void SDLRenderer<Pixel, zoom>::drawBorder(
	int fromX, int fromY, int limitX, int limitY)
{
	// TODO: Only redraw if necessary.
	SDL_Rect rect;
	rect.x = translateX(fromX);
	rect.w = translateX(limitX) - rect.x;
	rect.y = (fromY - lineRenderTop) * LINE_ZOOM;
	rect.h = (limitY - fromY) * LINE_ZOOM;

	// Note: return code ignored.
	SDL_FillRect(screen, &rect, getBorderColour());
}

template <class Pixel, Renderer::Zoom zoom>
void SDLRenderer<Pixel, zoom>::drawDisplay(
	int fromX, int fromY,
	int displayX, int displayY,
	int displayWidth, int displayHeight
) {
	if (zoom == Renderer::ZOOM_256) {
		displayX /= 2;
		displayWidth /= 2;
	}

	// Clip to screen area.
	if (fromY < lineRenderTop) {
		displayHeight -= lineRenderTop - fromY;
		fromY = lineRenderTop;
	}
	if (fromY + displayHeight > lineRenderBottom) {
		displayHeight = lineRenderBottom - fromY;
	}
	if (displayHeight <= 0) return;

	int screenY = (fromY - lineRenderTop) * LINE_ZOOM;
	if (!(settings->getDeinterlace()->getValue())
	&& vdp->isInterlaced()
	&& vdp->getEvenOdd()
	&& zoom != Renderer::ZOOM_256) {
		// Display odd field half a line lower.
		screenY++;
	}
	int screenLimitY = screenY + displayHeight * LINE_ZOOM;

	int leftBackground = translateX(vdp->getLeftBackground());
	// TODO: Find out why this causes 1-pixel jitter:
	//dest.x = translateX(fromX);

	DisplayMode mode = vdp->getDisplayMode();
	int hScroll = 
		  mode.isTextMode()
		? 0
		: 8 * LINE_ZOOM * (vdp->getHorizontalScrollHigh() & 0x1F);
	
	// Page border is display X coordinate where to stop drawing current page.
	// This is either the multi page split point, or the right edge of the
	// rectangle to draw, whichever comes first.
	// Note that it is possible for pageBorder to be to the left of displayX,
	// in that case only the second page should be drawn.
	int pageBorder = displayX + displayWidth;
	int scrollPage1, scrollPage2;
	if (vdp->isMultiPageScrolling()) {
		scrollPage1 = vdp->getHorizontalScrollHigh() >> 5;
		scrollPage2 = scrollPage1 ^ 1;
	} else {
		scrollPage1 = 0;
		scrollPage2 = 0;
	}
	// Because SDL blits do not wrap, unlike GL textures, the pageBorder is
	// also used if multi page is disabled.
	int pageSplit = 256 * LINE_ZOOM - hScroll;
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
		bool deinterlaced = settings->getDeinterlace()->getValue()
			&& zoom != Renderer::ZOOM_256
			&& vdp->isInterlaced() && vdp->isEvenOddEnabled();
		int pageMaskEven, pageMaskOdd;
		if (deinterlaced || vdp->isMultiPageScrolling()) {
			pageMaskEven = mode.isPlanar() ? 0x000 : 0x200;
			pageMaskOdd  = pageMaskEven | 0x100;
		} else {
			pageMaskEven = pageMaskOdd =
				(mode.isPlanar() ? 0x000 : 0x200) | vdp->getEvenOddMask();
		}
		
		// Copy from cache to screen.
		SDL_Rect source, dest;
		for (int y = screenY; y < screenLimitY; y += LINE_ZOOM) {
			int vramLine[2];
			vramLine[0] = (vram->nameTable.getMask() >> 7)
				& (pageMaskEven | displayY);
			vramLine[1] = (vram->nameTable.getMask() >> 7)
				& (pageMaskOdd  | displayY);
			
			// TODO: Can we safely use SDL_LowerBlit?
			if (deinterlaced) {
				// TODO: Allow horizontal scroll during deinterlace.
				source.x = displayX;
				source.y = vramLine[0];
				source.w = displayWidth;
				source.h = 1;
				dest.x = leftBackground + displayX;
				dest.y = y;
				SDL_BlitSurface(bitmapDisplayCache, &source, screen, &dest);
				if (LINE_ZOOM == 2) {
					source.y = vramLine[1];
					dest.y = y + 1;
					SDL_BlitSurface(bitmapDisplayCache, &source, screen, &dest);
				}
			} else {
				int firstPageWidth = pageBorder - displayX;
				if (firstPageWidth > 0) {
					source.x = displayX + hScroll;
					source.y = vramLine[scrollPage1];
					source.w = firstPageWidth;
					source.h = 1;
					dest.x = leftBackground + displayX;
					dest.y = y;
					SDL_BlitSurface(bitmapDisplayCache, &source, screen, &dest);
					if (LINE_ZOOM == 2) {
						dest.y++;
						SDL_BlitSurface(bitmapDisplayCache, &source, screen, &dest);
					}
				} else {
					firstPageWidth = 0;
				}
				if (firstPageWidth < displayWidth) {
					source.x = displayX < pageBorder
						? 0 : displayX + hScroll - 256 * LINE_ZOOM;
					source.y = vramLine[scrollPage2];
					source.w = displayWidth - firstPageWidth;
					source.h = 1;
					dest.x = leftBackground + displayX + firstPageWidth;
					dest.y = y;
					SDL_BlitSurface(bitmapDisplayCache, &source, screen, &dest);
					if (LINE_ZOOM == 2) {
						dest.y++;
						SDL_BlitSurface(bitmapDisplayCache, &source, screen, &dest);
					}
				}
			}
			displayY = (displayY + 1) & 255;
		}
	} else {
		renderCharacterLines(displayY, displayHeight);

		// TODO: Implement horizontal scroll high.
		SDL_Rect source, dest;
		for (int y = screenY; y < screenLimitY; y += LINE_ZOOM) {
			assert(!vdp->isMSX1VDP() || displayY < 192);
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
			SDL_BlitSurface(charDisplayCache, &source, screen, &dest);
			if (LINE_ZOOM == 2) {
				dest.y++;
				SDL_BlitSurface(charDisplayCache, &source, screen, &dest);
			}
			displayY = (displayY + 1) & 255;
		}
	}
}

template <class Pixel, Renderer::Zoom zoom>
void SDLRenderer<Pixel, zoom>::drawSprites(
	int fromX, int fromY,
	int displayX, int displayY,
	int displayWidth, int displayHeight
) {
	// Clip to screen area.
	// TODO: Code duplicated from drawDisplay.
	if (fromY < lineRenderTop) {
		displayHeight -= lineRenderTop - fromY;
		fromY = lineRenderTop;
	}
	if (fromY + displayHeight > lineRenderBottom) {
		displayHeight = lineRenderBottom - fromY;
	}
	if (displayHeight <= 0) return;

	// Render sprites.
	// Lock surface, because we will access pixels directly.
	// TODO: Locking the surface directly after a blit is
	//   probably not the smartest thing to do performance-wise.
	//   Since sprite data will buffered, why not plot them
	//   just before page flip?
	//   Will only work if *all* data required is buffered, including
	//   for example RGB colour (on V9938 palette may change).
	if (SDL_MUSTLOCK(screen) && SDL_LockSurface(screen) < 0) {
		// Display will be wrong, but this is not really critical.
		return;
	}

	// TODO: Code duplicated from drawDisplay.
	int screenY = (fromY - lineRenderTop) * LINE_ZOOM;
	if (!(settings->getDeinterlace()->getValue())
	&& vdp->isInterlaced()
	&& vdp->getEvenOdd()
	&& zoom != Renderer::ZOOM_256) {
		// Display odd field half a line lower.
		screenY++;
	}

	int spriteMode = vdp->getDisplayMode().getSpriteMode();
	int displayLimitX = displayX + displayWidth;
	int limitY = fromY + displayHeight;
	Pixel *pixelPtr0 = (Pixel *)(
		(byte *)screen->pixels
		+ screenY * screen->pitch
		+ translateX(vdp->getLeftSprites()) * sizeof(Pixel)
		);
	for (int y = fromY; y < limitY; y++) {
		Pixel *pixelPtr1 =
			( LINE_ZOOM == 1
			? NULL
			: (Pixel *)(((byte *)pixelPtr0) + screen->pitch)
			);

		if (spriteMode == 1) {
			spriteConverter.drawMode1(
				y, displayX, displayLimitX, pixelPtr0, pixelPtr1 );
		} else {
			spriteConverter.drawMode2(
				y, displayX, displayLimitX, pixelPtr0, pixelPtr1 );
		}

		pixelPtr0 = (Pixel *)(((byte *)pixelPtr0) + screen->pitch * LINE_ZOOM);
	}

	// Unlock surface.
	if (SDL_MUSTLOCK(screen))
		SDL_UnlockSurface(screen);
}
