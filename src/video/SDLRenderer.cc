// $Id$

/*
TODO:
- Implement blinking (of page mask) in bitmap modes.
- Spot references to EmuTime: if implemented correctly this class should
  not need EmuTime, since it uses absolute VDP coordinates instead.
*/

#include <cmath>
#include <cassert>
#include <algorithm>
#include "SDLRenderer.hh"
#include "VDP.hh"
#include "VDPVRAM.hh"
#include "RenderSettings.hh"
#include "Scaler.hh"
#include "ScreenShotSaver.hh"
#include "EventDistributor.hh"
#include "FloatSetting.hh"
#include "Scheduler.hh"

#ifdef __WIN32__
#include <windows.h>
static int lastWindowX = 0;
static int lastWindowY = 0;
#endif 

using std::max;
using std::min;


namespace openmsx {

// Force template instantiation:
template class SDLRenderer<Uint8, Renderer::ZOOM_256>;
template class SDLRenderer<Uint8, Renderer::ZOOM_REAL>;
template class SDLRenderer<Uint16, Renderer::ZOOM_256>;
template class SDLRenderer<Uint16, Renderer::ZOOM_REAL>;
template class SDLRenderer<Uint32, Renderer::ZOOM_256>;
template class SDLRenderer<Uint32, Renderer::ZOOM_REAL>;

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
inline int SDLRenderer<Pixel, zoom>::translateX(int absoluteX, bool narrow)
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
void SDLRenderer<Pixel, zoom>::update(const SettingLeafNode* setting)
{
	if (setting == settings.getDeinterlace()) {
		initWorkScreens();
	} else if (setting == &powerSetting) {
		Display::INSTANCE->setAlpha(this, powerSetting.getValue() ? 255 : 0);
	} else {
		PixelRenderer::update(setting);
	}
}

template <class Pixel, Renderer::Zoom zoom>
void SDLRenderer<Pixel, zoom>::finishFrame()
{
	Event* finishFrameEvent = new SimpleEvent<FINISH_FRAME_EVENT>();
	EventDistributor::instance().distributeEvent(finishFrameEvent);
}

template <class Pixel, Renderer::Zoom zoom>
void SDLRenderer<Pixel, zoom>::paint()
{
	// All of the current postprocessing steps require hi-res.
	if (LINE_ZOOM != 2) {
		// Just copy the image as-is.
		SDL_BlitSurface(workScreen, NULL, screen, NULL);
		return;
	}

	const bool deinterlace =
		interlaced && settings.getDeinterlace()->getValue();
	assert(!deinterlace || workScreens[1]);

	// New scaler algorithm selected?
	ScalerID scalerID = settings.getScaler()->getValue();
	if (currScalerID != scalerID) {
		currScaler = Scaler<Pixel>::createScaler(scalerID, screen->format);
		currScalerID = scalerID;
	}

	// Scale image.
	const unsigned deltaY = interlaced && vdp->getEvenOdd() ? 1 : 0;
	unsigned startY = 0;
	while (startY < HEIGHT / 2) {
		const LineContent content = lineContent[startY];
		unsigned endY = startY + 1;
		while (endY < HEIGHT / 2 && lineContent[endY] == content) endY++;

		switch (content) {
		case LINE_BLANK: {
			// Reduce area to same-colour starting segment.
			const Pixel colour = *reinterpret_cast<Pixel*>(
				reinterpret_cast<byte*>(workScreen->pixels) +
				workScreen->pitch * startY
				);
			for (unsigned y = startY + 1; y < endY; y++) {
				const Pixel colour2 = *reinterpret_cast<Pixel*>(
					reinterpret_cast<byte*>(workScreen->pixels) +
					workScreen->pitch * y
					);
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
		case LINE_256:
			if (deinterlace) {
				for (unsigned y = startY; y < endY; y++) {
					deinterlacer.deinterlaceLine256(
						workScreens[0], workScreens[1], y, screen, y * 2 );
				}
			} else {
				currScaler->scale256(
					workScreen, startY, endY, screen, startY * 2 + deltaY );
			}
			break;
		case LINE_512:
			if (deinterlace) {
				for (unsigned y = startY; y < endY; y++) {
					deinterlacer.deinterlaceLine512(
						workScreens[0], workScreens[1], y, screen, y * 2 );
				}
			} else {
				currScaler->scale512(
					workScreen, startY, endY, screen, startY * 2 + deltaY );
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
const string& SDLRenderer<Pixel, zoom>::getName()
{
	static const string NAME = "SDLRenderer";
	return NAME;
}

template <class Pixel, Renderer::Zoom zoom>
inline Pixel* SDLRenderer<Pixel, zoom>::getLinePtr(
	SDL_Surface* displayCache, int line)
{
	return (Pixel*)( (byte*)displayCache->pixels
		+ line * displayCache->pitch );
}

// TODO: Cache this?
template <class Pixel, Renderer::Zoom zoom>
inline Pixel SDLRenderer<Pixel, zoom>::getBorderColour()
{
	int mode = vdp->getDisplayMode().getByte();
	int bgColour = vdp->getBackgroundColour();
	if (vdp->getDisplayMode().getBase() == DisplayMode::GRAPHIC5) {
		// TODO: Border in SCREEN6 has separate colour for even and odd pixels.
		//       Until that is supported, only use odd pixel colour.
		bgColour &= 0x03;
	}
	return (mode == DisplayMode::GRAPHIC7 ? PALETTE256 : palBg)[bgColour];
}

template <class Pixel, Renderer::Zoom zoom>
inline void SDLRenderer<Pixel, zoom>::renderBitmapLine(
	byte mode, int vramLine)
{
	if (lineValidInMode[vramLine] != mode) {
		const byte* vramPtr =
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
	byte mode = vdp->getDisplayMode().getByte();
	// Which bits in the name mask determine the page?
	int pageMask = 0x200 | vdp->getEvenOddMask();
	int nameMask = vram->nameTable.getMask() >> 7;
	
	while (count--) {
		// TODO: Optimise addr and line; too many conversions right now.
		int vramLine = nameMask & (pageMask | line);
		renderBitmapLine(mode, vramLine);
		if (vdp->isMultiPageScrolling()) {
			vramLine &= ~0x100;
			renderBitmapLine(mode, vramLine);
		}
		line++; // is a byte, so wraps at 256
	}
}

template <class Pixel, Renderer::Zoom zoom>
inline void SDLRenderer<Pixel, zoom>::renderPlanarBitmapLine(
	byte mode, int vramLine)
{
	if ( lineValidInMode[vramLine] != mode
	|| lineValidInMode[vramLine | 512] != mode ) {
		int addr0 = vramLine << 7;
		int addr1 = addr0 | 0x10000;
		const byte* vramPtr0 =
			vram->bitmapCacheWindow.readArea(addr0);
		const byte* vramPtr1 =
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
}

template <class Pixel, Renderer::Zoom zoom>
inline void SDLRenderer<Pixel, zoom>::renderCharacterLines(
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
SDLRenderer<Pixel, zoom>::SDLRenderer(
	RendererFactory::RendererID id, VDP* vdp, SDL_Surface* screen)
	: PixelRenderer(id, vdp)
	, characterConverter(vdp, palFg, palBg,
		Blender<Pixel>::createFromFormat(screen->format) )
	, bitmapConverter(palFg, PALETTE256, V9958_COLOURS,
		Blender<Pixel>::createFromFormat(screen->format) )
	, spriteConverter(vdp->getSpriteChecker(),
		Blender<Pixel>::createFromFormat(screen->format) )
	, powerSetting(Scheduler::instance().getPowerSetting())
{
	this->screen = screen;
	currScalerID = (ScalerID)-1; // not a valid scaler

	// Allocate work surface.
	initWorkScreens(true);

	// Create display caches.
	charDisplayCache = SDL_CreateRGBSurface(
		SDL_SWSURFACE,
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

	// Hide mouse cursor.
	SDL_ShowCursor(SDL_DISABLE);

	// Init the palette.
	precalcPalette(settings.getGamma()->getValue());

	settings.getDeinterlace()->addListener(this);

#ifdef __WIN32__
	// Find our current location
	HWND handle = GetActiveWindow();
	RECT windowRect;
	GetWindowRect (handle, &windowRect);
	// and adjust if needed
	if ((windowRect.right < 0) || (windowRect.bottom < 0)){
		SetWindowPos(handle, HWND_TOP,lastWindowX,lastWindowY,0,0,SWP_NOSIZE);
	}
#endif 

	powerSetting.addListener(this);
}

template <class Pixel, Renderer::Zoom zoom>
SDLRenderer<Pixel, zoom>::~SDLRenderer()
{
	powerSetting.removeListener(this);
	settings.getDeinterlace()->removeListener(this);

#ifdef __WIN32__
	// Find our current location
	if ((screen->flags & SDL_FULLSCREEN) == 0){
		HWND handle = GetActiveWindow();
		RECT windowRect;
		GetWindowRect (handle, &windowRect);
		lastWindowX = windowRect.left;
		lastWindowY = windowRect.top;
	}
#endif	 

	SDL_FreeSurface(charDisplayCache);
	if (bitmapDisplayCache) SDL_FreeSurface(bitmapDisplayCache);
	for (int i = 0; i < 2; i++) {
		if (workScreens[i]) SDL_FreeSurface(workScreens[i]);
	}

	SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

template <class Pixel, Renderer::Zoom zoom>
void SDLRenderer<Pixel, zoom>::initWorkScreens(bool first)
{
	const bool deinterlace = settings.getDeinterlace()->getValue();

	// Make sure current frame is in workScreens[0].
	if (!first && workScreen == workScreens[1]) {
		// Swap workScreens index 0 and 1.
		workScreens[1] = workScreens[0];
		workScreens[0] = workScreen;
	}

	const int nrScreens = deinterlace ? 2 : 1;
	for (int i = 0; i < 2; i++) {
		const bool shouldExist = i < nrScreens;
		const bool doesExist = !first && workScreens[i];
		if (shouldExist) {
			if (!doesExist) {
				workScreens[i] = SDL_CreateRGBSurface(
					SDL_SWSURFACE,
					WIDTH, 240, // TODO: Vertical size should be borders + display.
					screen->format->BitsPerPixel,
					screen->format->Rmask,
					screen->format->Gmask,
					screen->format->Bmask,
					screen->format->Amask
					);
			}
		} else { // !shouldExist
			if (doesExist) SDL_FreeSurface(workScreens[i]);
			workScreens[i] = NULL;
		}
	}

	calcWorkScreen(vdp->getEvenOdd());

	// Make sure that the surface which contains the current frame's image
	// is the one pointed to by workScreen.
	if (workScreen == workScreens[1]) {
		// Swap workScreens index 0 and 1.
		workScreens[1] = workScreens[0];
		workScreens[0] = workScreen;
		workScreen = workScreens[1];
	}
}

template <class Pixel, Renderer::Zoom zoom>
void SDLRenderer<Pixel, zoom>::calcWorkScreen(bool oddField)
{
	workScreen = workScreens[
		settings.getDeinterlace()->getValue() && oddField ? 1 : 0
		];
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
			const byte* rgb = TMS99X8A_PALETTE[i];
			palFg[i] = palBg[i] = SDL_MapRGB(
				screen->format,
				(int)(::pow((float)rgb[0] / 255.0, gamma) * 255),
				(int)(::pow((float)rgb[1] / 255.0, gamma) * 255),
				(int)(::pow((float)rgb[2] / 255.0, gamma) * 255)
				);
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
						(int)(::pow((float)b / 7.0, gamma) * 255)
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
						(int)(::pow((float)r / 31.0, gamma) * 255),
						(int)(::pow((float)g / 31.0, gamma) * 255),
						(int)(::pow((float)b / 31.0, gamma) * 255)
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
			word grb = GRAPHIC7_SPRITE_PALETTE[i];
			palGraphic7Sprites[i] =
				V9938_COLOURS[(grb >> 4) & 7][grb >> 8][grb & 7];
		}
	}
}

template <class Pixel, Renderer::Zoom zoom>
void SDLRenderer<Pixel, zoom>::reset(const EmuTime& time)
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
	bool fullScreenTarget = settings.getFullScreen()->getValue();
	if (fullScreenState == fullScreenTarget) return true;

#ifdef __WIN32__
	// Under win32, toggling full screen requires opening a new SDL screen.
	return false;
#else
	// Try to toggle full screen.
	SDL_WM_ToggleFullScreen(screen);
	fullScreenState =
		((((volatile SDL_Surface*)screen)->flags & SDL_FULLSCREEN) != 0);
	return fullScreenState == fullScreenTarget;
#endif
}

template <class Pixel, Renderer::Zoom zoom>
void SDLRenderer<Pixel, zoom>::frameStart(const EmuTime& time)
{
	// Call superclass implementation.
	PixelRenderer::frameStart(time);

	calcWorkScreen(vdp->getEvenOdd());

	// Calculate line to render at top of screen.
	// Make sure the display area is centered.
	// 240 - 212 = 28 lines available for top/bottom border; 14 each.
	// NTSC: display at [32..244),
	// PAL:  display at [59..271).
	lineRenderTop = vdp->isPalTiming() ? 59 - 14 : 32 - 14;

	float gamma = settings.getGamma()->getValue();
	// (gamma != prevGamma) gives compiler warnings
	if ((gamma > prevGamma) || (gamma < prevGamma)) {
		precalcPalette(gamma);
		resetPalette();
	}
}

template <class Pixel, Renderer::Zoom zoom>
void SDLRenderer<Pixel, zoom>::frameEnd(const EmuTime& time)
{
	// Remember interlace status of the completed frame,
	// for use in paint().
	interlaced = vdp->isInterlaced();

	PixelRenderer::frameEnd(time);
}

template <class Pixel, Renderer::Zoom zoom>
void SDLRenderer<Pixel, zoom>::setDisplayMode(DisplayMode mode)
{
	if (mode.isBitmapMode()) {
		bitmapConverter.setDisplayMode(mode);
	} else {
		characterConverter.setDisplayMode(mode);
	}
	precalcColourIndex0(mode, vdp->getTransparency());
	spriteConverter.setDisplayMode(mode);
	spriteConverter.setPalette(
		mode.getByte() == DisplayMode::GRAPHIC7 ? palGraphic7Sprites : palBg
		);
}

template <class Pixel, Renderer::Zoom zoom>
void SDLRenderer<Pixel, zoom>::updateTransparency(
	bool enabled, const EmuTime& time)
{
	sync(time);
	spriteConverter.setTransparency(enabled);
	precalcColourIndex0(vdp->getDisplayMode(), enabled);
}

template <class Pixel, Renderer::Zoom zoom>
void SDLRenderer<Pixel, zoom>::updateForegroundColour(
	int /*colour*/, const EmuTime& time)
{
	sync(time);
}

template <class Pixel, Renderer::Zoom zoom>
void SDLRenderer<Pixel, zoom>::updateBackgroundColour(
	int colour, const EmuTime& time)
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
	int /*colour*/, const EmuTime& time)
{
	sync(time);
}

template <class Pixel, Renderer::Zoom zoom>
void SDLRenderer<Pixel, zoom>::updateBlinkBackgroundColour(
	int /*colour*/, const EmuTime& time)
{
	sync(time);
}

template <class Pixel, Renderer::Zoom zoom>
void SDLRenderer<Pixel, zoom>::updateBlinkState(
	bool /*enabled*/, const EmuTime& /*time*/)
{
	// TODO: When the sync call is enabled, the screen flashes on
	//       every call to this method.
	//       I don't know why exactly, but it's probably related to
	//       being called at frame start.
	//sync(time);
}

template <class Pixel, Renderer::Zoom zoom>
void SDLRenderer<Pixel, zoom>::updatePalette(
	int index, int grb, const EmuTime& time)
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
	if (vdp->getTransparency() && vdp->getBackgroundColour() == index) {
		// Transparent pixels have background colour.
		precalcColourIndex0(vdp->getDisplayMode());
		// Note: Relies on the fact that precalcColourIndex0 flushes the cache.
	} else {
		// Any line containing pixels of this colour must be repainted.
		// We don't know which lines contain which colours,
		// so we have to repaint them all.
		memset(lineValidInMode, 0xFF, sizeof(lineValidInMode));
	}
}

template <class Pixel, Renderer::Zoom zoom>
void SDLRenderer<Pixel, zoom>::precalcColourIndex0(
	DisplayMode mode, bool transparency)
{
	// Graphic7 mode doesn't use transparency.
	if (mode.getByte() == DisplayMode::GRAPHIC7) {
		transparency = false;
	}
	
	int bgColour = 0;
	if (transparency) {
		bgColour = vdp->getBackgroundColour();
		if (mode.getBase() == DisplayMode::GRAPHIC5) {
			// TODO: Transparent pixels should be rendered in separate
			//       colours for even/odd x, just like the border.
			bgColour &= 0x03;
		}
	}
	palFg[0] = palBg[bgColour];
	
	// Any line containing pixels of colour 0 must be repainted.
	// We don't know which lines contain such pixels,
	// so we have to repaint them all.
	memset(lineValidInMode, 0xFF, sizeof(lineValidInMode));
}

template <class Pixel, Renderer::Zoom zoom>
void SDLRenderer<Pixel, zoom>::updateVerticalScroll(
	int /*scroll*/, const EmuTime& time)
{
	sync(time);
}

template <class Pixel, Renderer::Zoom zoom>
void SDLRenderer<Pixel, zoom>::updateHorizontalAdjust(
	int /*adjust*/, const EmuTime& time)
{
	sync(time);
}

template <class Pixel, Renderer::Zoom zoom>
void SDLRenderer<Pixel, zoom>::updateDisplayMode(
	DisplayMode mode, const EmuTime& time)
{
	sync(time, true);
	setDisplayMode(mode);
}

template <class Pixel, Renderer::Zoom zoom>
void SDLRenderer<Pixel, zoom>::updateNameBase(
	int /*addr*/, const EmuTime& time)
{
	sync(time);
}

template <class Pixel, Renderer::Zoom zoom>
void SDLRenderer<Pixel, zoom>::updatePatternBase(
	int /*addr*/, const EmuTime& time)
{
	sync(time);
}

template <class Pixel, Renderer::Zoom zoom>
void SDLRenderer<Pixel, zoom>::updateColourBase(
	int /*addr*/, const EmuTime& time)
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
	if (fromX == 0 && limitX == VDP::TICKS_PER_LINE) {
		// TODO: Disabled this optimisation during interlace, because 
		//       lineContent administration is available only for current
		//       frame, not for previous frame, which led to glitches.
		if (LINE_ZOOM == 2 && !interlaced) {
			//fprintf(stderr, "saved: %d-%d\n", fromY, limitY);
			//int endY = rect.y + rect.h;
			//for (int y = rect.y; y < endY; y++) {
			int startY = max(fromY - lineRenderTop, 0);
			int endY = min(limitY - lineRenderTop, (int)HEIGHT / 2);
			for (int y = startY; y < endY; y++) {
				*reinterpret_cast<Pixel*>(
					reinterpret_cast<byte*>(workScreen->pixels) +
					workScreen->pitch * y
					) = getBorderColour();
				lineContent[y] = LINE_BLANK;
			}
			return;
		}
	}

	bool narrow = vdp->getDisplayMode().getLineWidth() == 512;

	SDL_Rect rect;
	rect.x = translateX(fromX, narrow);
	rect.w = translateX(limitX, narrow) - rect.x;
	rect.y = fromY - lineRenderTop;
	rect.h = limitY - fromY;
	// Note: return code ignored.
	SDL_FillRect(workScreen, &rect, getBorderColour());

	if (LINE_ZOOM == 2) {
		LineContent lineType = narrow ? LINE_512 : LINE_256;
		int endY = rect.y + rect.h;
		for (int y = rect.y; y < endY; y++) {
			lineContent[y] = lineType;
 		}
 	}
}

template <class Pixel, Renderer::Zoom zoom>
void SDLRenderer<Pixel, zoom>::drawDisplay(
	int /*fromX*/, int fromY,
	int displayX, int displayY,
	int displayWidth, int displayHeight
) {
	DisplayMode mode = vdp->getDisplayMode();
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
		translateX(vdp->getLeftBackground(), lineWidth == 512);
	// TODO: Find out why this causes 1-pixel jitter:
	//dest.x = translateX(fromX);
	LineContent lineType = lineWidth == 256 ? LINE_256 : LINE_512;
	int hScroll =
		  mode.isTextMode()
		? 0
		: 8 * (lineWidth / 256) * (vdp->getHorizontalScrollHigh() & 0x1F);

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
		if (vdp->isMultiPageScrolling()) {
			pageMaskEven = mode.isPlanar() ? 0x000 : 0x200;
			pageMaskOdd  = pageMaskEven | 0x100;
		} else {
			pageMaskEven = pageMaskOdd =
				(mode.isPlanar() ? 0x000 : 0x200) | vdp->getEvenOddMask();
		}

		// Copy from cache to screen.
		SDL_Rect source, dest;
		for (int y = screenY; y < screenLimitY; y++) {
			const int vramLine[2] = {
				(vram->nameTable.getMask() >> 7) & (pageMaskEven | displayY),
				(vram->nameTable.getMask() >> 7) & (pageMaskOdd  | displayY)
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
					bitmapDisplayCache, &source, workScreen, &dest
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
					bitmapDisplayCache, &source, workScreen, &dest
					);
			}
			if (LINE_ZOOM == 2) lineContent[y] = lineType;

			displayY = (displayY + 1) & 255;
		}
	} else {
		renderCharacterLines(displayY, displayHeight);

		// TODO: Implement horizontal scroll high.
		SDL_Rect source, dest;
		for (int y = screenY; y < screenLimitY; y++) {
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
			SDL_BlitSurface(charDisplayCache, &source, workScreen, &dest);
			if (LINE_ZOOM == 2) lineContent[y] = lineType;
			displayY = (displayY + 1) & 255;
		}
	}
}

template <class Pixel, Renderer::Zoom zoom>
void SDLRenderer<Pixel, zoom>::drawSprites(
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
	int spriteMode = vdp->getDisplayMode().getSpriteMode();
	int displayLimitX = displayX + displayWidth;
	int limitY = fromY + displayHeight;
	Pixel* pixelPtr = (Pixel*)(
		(byte*)workScreen->pixels
		+ screenY * workScreen->pitch
		+ translateX(vdp->getLeftSprites(),
			vdp->getDisplayMode().getLineWidth() == 512) * sizeof(Pixel)
		);
	for (int y = fromY; y < limitY; y++) {
		if (spriteMode == 1) {
			spriteConverter.drawMode1(y, displayX, displayLimitX, pixelPtr);
		} else {
			spriteConverter.drawMode2(y, displayX, displayLimitX, pixelPtr);
		}

		pixelPtr = (Pixel*)(((byte*)pixelPtr) + workScreen->pitch);
	}
}

} // namespace openmsx
