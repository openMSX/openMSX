// $Id$

#include "V9990.hh"
#include "V9990SDLRasterizer.hh"
#include "BooleanSetting.hh"
#include "RenderSettings.hh"
#include <SDL.h>

using std::string;

namespace openmsx {

template <class Pixel, Renderer::Zoom zoom>
V9990SDLRasterizer<Pixel, zoom>::V9990SDLRasterizer(
	V9990* vdp_, SDL_Surface* screen_)
	: vdp(vdp_)
	, screen(screen_)
	, bitmapConverter(vdp, *screen->format,
	                  palette64, palette256, palette32768)
	, p1Converter(vdp, palette64)
	, p2Converter(vdp, palette64)
	, deinterlaceSetting(RenderSettings::instance().getDeinterlace())
{
	PRT_DEBUG("V9990SDLRasterizer::V9990SDLRasterizer");

	static const int width = (zoom == Renderer::ZOOM_256)
	                       ? SCREEN_WIDTH
	                       : SCREEN_WIDTH * 2;
	static const int height = (zoom == Renderer::ZOOM_256)
	                        ? SCREEN_HEIGHT
	                        : SCREEN_HEIGHT * 2;
	vram = vdp->getVRAM();

	// Create Work screen
	SDL_PixelFormat* format = screen->format;
	for (int i = 0; i < NB_WORKSCREENS; ++i) {
		workScreens[i] = SDL_CreateRGBSurface(
			SDL_SWSURFACE, width, height, format->BitsPerPixel,
			format->Rmask, format->Gmask, format->Bmask, format->Amask);
		if (!workScreens[i]) {
			throw FatalError("Can't allocate workscreens for V9990.");
		}
	}

	// Fill palettes
	precalcPalettes();
}

template <class Pixel, Renderer::Zoom zoom>
V9990SDLRasterizer<Pixel, zoom>::~V9990SDLRasterizer()
{
	PRT_DEBUG("V9990SDLRasterizer::~V9990SDLRasterizer");

	for (int i = 0; i < NB_WORKSCREENS; ++i) {
		SDL_FreeSurface(workScreens[i]);
	}
}

template <class Pixel, Renderer::Zoom zoom>
SDL_Surface* V9990SDLRasterizer<Pixel, zoom>::getWorkScreen(bool prev) const
{
	if (zoom == Renderer::ZOOM_256) {
		return workScreens[0];
	} else {
		if (!vdp->isEvenOddEnabled() ||
		    !deinterlaceSetting->getValue()) {
			return workScreens[0];
		} else {
			if (!prev) {
				return (vdp->getEvenOdd()) ? workScreens[1]
				                           : workScreens[0];
			} else {
				return (vdp->getEvenOdd()) ? workScreens[0]
				                           : workScreens[1];
			}
		}
	}
}

template <class Pixel, Renderer::Zoom zoom>
void V9990SDLRasterizer<Pixel, zoom>::paint()
{
	PRT_DEBUG("V9990SDLRasterizer::paint()");

	// Simple scaler for SDLHi
	if (zoom == Renderer::ZOOM_REAL) {
		bool even = vdp->getEvenOdd();
		SDL_Surface* workScreen0 = getWorkScreen(even);
		SDL_Surface* workScreen1 = getWorkScreen(!even);
		for (int y = 0; y < SCREEN_HEIGHT; ++y) {
			Scaler<Pixel>::copyLine(workScreen0, y, screen, 2 * y + 0);
			Scaler<Pixel>::copyLine(workScreen1, y, screen, 2 * y + 1);
		}
	} else {
		SDL_BlitSurface(getWorkScreen(), NULL, screen, NULL);
	}
}

template <class Pixel, Renderer::Zoom zoom>
const string& V9990SDLRasterizer<Pixel, zoom>::getName()
{
	static const string NAME = "V9990SDLRasterizer";
	return NAME;
}

template <class Pixel, Renderer::Zoom zoom>
void V9990SDLRasterizer<Pixel, zoom>::reset()
{
	PRT_DEBUG("V9990SDLRasterizer::reset()");

	setDisplayMode(vdp->getDisplayMode());
	setColorMode(vdp->getColorMode());
	imageWidth = vdp->getImageWidth();
}

template <class Pixel, Renderer::Zoom zoom>
void V9990SDLRasterizer<Pixel, zoom>::frameStart()
{
	PRT_DEBUG("V9990SDLRasterizer::frameStart()");

	const V9990DisplayPeriod& horTiming = vdp->getHorizontalTiming();
	const V9990DisplayPeriod& verTiming = vdp->getVerticalTiming();

	// Center image on the window.

	// In SDLLo, one window pixel represents 8 UC clockticks, so the
	// window = 320 * 8 UC ticks wide. In SDLHi, one pixel is 4 clock-
	// ticks and the window 640 pixels wide -- same amount of UC ticks.
	colZero = horTiming.border1 + (horTiming.display - SCREEN_WIDTH * 8) / 2;

	// 240 display lines can be shown. In SDLHi, we can do interlace,
	// but still 240 lines per frame.
	lineZero = verTiming.border1 + (verTiming.display - SCREEN_HEIGHT) / 2;
}

template <class Pixel, Renderer::Zoom zoom>
void V9990SDLRasterizer<Pixel, zoom>::frameEnd()
{
	PRT_DEBUG("V9990SDLRasterizer::frameEnd()");
}

template <class Pixel, Renderer::Zoom zoom>
void V9990SDLRasterizer<Pixel, zoom>::setDisplayMode(V9990DisplayMode mode)
{
	PRT_DEBUG("V9990SDLRasterizer::setDisplayMode(" << std::dec <<
	          (int) mode << ")");

	displayMode = mode;
	bitmapConverter.setDisplayMode(mode);
}

template <class Pixel, Renderer::Zoom zoom>
void V9990SDLRasterizer<Pixel, zoom>::setColorMode(V9990ColorMode mode)
{
	PRT_DEBUG("V9990SDLRasterizer::setColorMode(" << std::dec <<
	          (int) mode << ")");

	colorMode = mode;
	bitmapConverter.setColorMode(mode);
}

template <class Pixel, Renderer::Zoom zoom>
void V9990SDLRasterizer<Pixel, zoom>::setImageWidth(int width)
{
	// sync?
	imageWidth = width;
}

template <Renderer::Zoom zoom>
static inline int translateX(int x)
{
	return x / ((zoom == Renderer::ZOOM_256) ? 8 : 4);
}

template <class Pixel, Renderer::Zoom zoom>
void V9990SDLRasterizer<Pixel, zoom>::drawBorder(
	int fromX, int fromY, int toX, int toY)
{
	PRT_DEBUG("V9990SDLRasterizer::drawBorder(" << std::dec <<
	          fromX << "," << fromY << "," << toX << "," << toY << ")");

	static int const screenW = SCREEN_WIDTH * 8;
	static int const screenH = SCREEN_HEIGHT;

	// From vdp coordinates (UC ticks/lines) to screen coordinates
	fromX -= colZero;
	fromY -= lineZero;
	toX   -= colZero;
	toY   -= lineZero;

	// Clip or expand to screen edges
	if ((fromX <= -colZero) || (fromX < 0)) {
		fromX = 0;
	}
	if ((fromY <= -lineZero) || (fromY < 0)) {
		fromY = 0;
	}
	if ((toX >= screenW + colZero) || (toX > screenW)) {
		toX = screenW;
	}
	if ((toY >= screenH + lineZero) || (toY > screenH)) {
		toY = screenH;
	}

	int width = toX - fromX;
	int height = toY - fromY;

	if ((width > 0) && (height > 0)) {
		SDL_Rect rect;
		rect.x = translateX<zoom>(fromX);
		rect.w = translateX<zoom>(width);
		rect.y = fromY;
		rect.h = height;
		Pixel bgColor = palette64[vdp->getBackDropColor() & 63];
		SDL_FillRect(getWorkScreen(), &rect, bgColor);
	}
}

template <class Pixel, Renderer::Zoom zoom>
void V9990SDLRasterizer<Pixel, zoom>::drawDisplay(
	int fromX, int fromY,
	int displayX, int displayY, int displayWidth, int displayHeight)
{
	PRT_DEBUG("V9990SDLRasterizer::drawDisplay(" << std::dec <<
	          fromX << "," << fromY << "," <<
	          displayX << "," << displayY << "," <<
	          displayWidth << "," << displayHeight << ")");

	static int const screenW = SCREEN_WIDTH * 8;
	static int const screenH = SCREEN_HEIGHT;

	if ((displayWidth > 0) && (displayHeight > 0)) {
		// from VDP coordinates to screen coordinates
		fromX -= colZero;
		fromY -= lineZero;

		// Clip to screen
		if (fromX < 0) {
			displayX -= fromX;
			displayWidth += fromX;
			fromX = 0;
		}
		if ((fromX + displayWidth) > screenW) {
			displayWidth = screenW - fromX;
		}
		if (fromY < 0) {
			displayY -= fromY;
			displayHeight += fromY;
			fromY = 0;
		}
		if ((fromY + displayHeight) > screenH) {
			displayHeight = screenH - fromY;
		}

		if (displayHeight > 0) {
			displayX = V9990::UCtoX(displayX, displayMode);
			displayWidth = V9990::UCtoX(displayWidth, displayMode);

			if (displayMode == P1) {
				// TODO
				drawP1Mode(fromX, fromY, displayX, displayY,
						   displayWidth, displayHeight);
			} else if (displayMode == P2) {
				// TODO
				drawP2Mode(fromX, fromY, displayX, displayY,
						   displayWidth, displayHeight);
			} else {
				drawBxMode(fromX, fromY, displayX, displayY,
					   displayWidth, displayHeight);
			}
		}
	}
}

template <class Pixel, Renderer::Zoom zoom>
void V9990SDLRasterizer<Pixel, zoom>::drawP1Mode(
	int fromX, int fromY, int displayX, int displayY,
	int displayWidth, int displayHeight)
{
	SDL_Surface* workScreen = getWorkScreen();
	Pixel* pixelPtr = (Pixel*)(
			(byte*)workScreen->pixels +
			fromY * workScreen->pitch +
			translateX<zoom>(fromX) * sizeof(Pixel));
	while (displayHeight--) {
		p1Converter.convertLine(pixelPtr, displayX, displayWidth,
		                        displayY);
		displayY++;
		pixelPtr += workScreen->w;
	}
}

template <class Pixel, Renderer::Zoom zoom>
void V9990SDLRasterizer<Pixel, zoom>::drawP2Mode(
	int fromX, int fromY, int displayX, int displayY,
	int displayWidth, int displayHeight)
{
	SDL_Surface* workScreen = getWorkScreen();
	Pixel* pixelPtr = (Pixel*)(
			(byte*)workScreen->pixels +
			fromY * workScreen->pitch +
			translateX<zoom>(fromX) * sizeof(Pixel));
	while (displayHeight--) {
		p2Converter.convertLine(pixelPtr, displayX, displayWidth,
		                        displayY);
		displayY++;
		pixelPtr += workScreen->w;
	}
}

static unsigned rollMasks[4] = {
	0x1FFF, // no rolling
	0x00FF,
	0x01FF,
	0x00FF // TODO check this (undocumented)
};

template <class Pixel, Renderer::Zoom zoom>
void V9990SDLRasterizer<Pixel, zoom>::drawBxMode(
	int fromX, int fromY, int displayX, int displayY,
	int displayWidth, int displayHeight)
{
	SDL_Surface* workScreen = getWorkScreen();
	Pixel* pixelPtr = (Pixel*)(
		  (byte*)workScreen->pixels +
		  fromY * workScreen->pitch +
		  translateX<zoom>(fromX) * sizeof(Pixel));

	unsigned scrollX = vdp->getScrollAX();
	unsigned x = displayX + scrollX;

	unsigned scrollY = vdp->getScrollAY();
	int lineStep = 1;
	if (vdp->isEvenOddEnabled()) {
		if (vdp->getEvenOdd()) {
			++displayY;
		}
		lineStep = 2;
	}

	unsigned rollMask = rollMasks[scrollY >> 14];
	unsigned scrollYBase = scrollY & ~rollMask;
	while (displayHeight--) {
		unsigned y = scrollYBase + (displayY + scrollY) & rollMask;
		unsigned address = vdp->XYtoVRAM(&x, y, colorMode);
		bitmapConverter.convertLine(pixelPtr, address, displayWidth,
		                            displayY);
		displayY += lineStep;
		pixelPtr += workScreen->w;
	}
}


template <class Pixel, Renderer::Zoom zoom>
void V9990SDLRasterizer<Pixel, zoom>::precalcPalettes()
{
	PRT_DEBUG("V9990SDLRasterizer::precalcPalettes()");

	// the 32768 color palette
	for (int g = 0; g < 32; ++g) {
		for (int r = 0; r < 32; ++r) {
			for (int b = 0; b < 32; ++b) {
				palette32768[(g << 10) + (r << 5) + b] =
					SDL_MapRGB(screen->format,
					           (int)(r * (255.0 / 31.0)),
					           (int)(g * (255.0 / 31.0)),
					           (int)(b * (255.0 / 31.0)));
			}
		}
	}

	// the 256 color palette
	int mapRG[8] = { 0, 4, 9, 13, 18, 22, 27, 31 };
	int mapB [4] = { 0, 11, 21, 31 };
	for (int g = 0; g < 8; ++g) {
		for (int r = 0; r < 8; ++r) {
			for (int b = 0; b < 4; ++b) {
				palette256[(g << 5) + (r << 2) + b] =
					palette32768[(mapRG[g] << 10) +
					             (mapRG[r] <<  5) +
					              mapB [b]];
			}
		}
	}

	// get 64 color palette from VDP
	for (int i = 0; i < 64; ++i) {
		byte r, g, b;
		vdp->getPalette(i, r, g, b);
		setPalette(i, r, g, b);
	}
}

template <class Pixel, Renderer::Zoom zoom>
void V9990SDLRasterizer<Pixel, zoom>::setPalette(int index,
                                                 byte r, byte g, byte b)
{
	palette64[index & 63] = palette32768[((g & 31) << 10) +
	                                     ((r & 31) <<  5) +
	                                      (b & 31)];
}


// Force template instantiation.
template class V9990SDLRasterizer<Uint16, Renderer::ZOOM_256>;
template class V9990SDLRasterizer<Uint16, Renderer::ZOOM_REAL>;
template class V9990SDLRasterizer<Uint32, Renderer::ZOOM_256>;
template class V9990SDLRasterizer<Uint32, Renderer::ZOOM_REAL>;

} // namespace openmsx

