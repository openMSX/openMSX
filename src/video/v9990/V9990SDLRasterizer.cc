// $Id$

#include "V9990SDLRasterizer.hh"
#include "V9990VRAM.hh"
#include "V9990.hh"

#include "openmsx.hh"
#include <SDL.h>

//#undef PRT_DEBUG
//#define PRT_DEBUG(m) cout << m << endl

namespace openmsx {

// Force template instantiation.
template class V9990SDLRasterizer<Uint16, Renderer::ZOOM_256>;
template class V9990SDLRasterizer<Uint16, Renderer::ZOOM_REAL>;
template class V9990SDLRasterizer<Uint32, Renderer::ZOOM_256>;
template class V9990SDLRasterizer<Uint32, Renderer::ZOOM_REAL>;

template <class Pixel, Renderer::Zoom zoom>
V9990SDLRasterizer<Pixel, zoom>::V9990SDLRasterizer(
	V9990* vdp_, SDL_Surface* screen_)
	: vdp(vdp_),
	  screen(screen_),
	  bitmapConverter(*(screen_->format),palette64, palette256, palette32768) 
{
	PRT_DEBUG("V9990SDLRasterizer::V9990SDLRasterizer");
	
	vram = vdp->getVRAM();

	/* Create Work screen */
	workScreen = SDL_CreateRGBSurface(
					SDL_SWSURFACE,
					(zoom == Renderer::ZOOM_256)? 320: 640,
					(zoom == Renderer::ZOOM_256)? 240: 480,
					screen->format->BitsPerPixel,
					screen->format->Rmask,
					screen->format->Gmask,
					screen->format->Bmask,
					screen->format->Amask);

	/* Fill palettes */
	precalcPalettes();
}

template <class Pixel, Renderer::Zoom zoom>
V9990SDLRasterizer<Pixel, zoom>::~V9990SDLRasterizer()
{
	PRT_DEBUG("V9990SDLRasterizer::~V9990SDLRasterizer");

	if(workScreen) SDL_FreeSurface(workScreen);
}

template <class Pixel, Renderer::Zoom zoom>
void V9990SDLRasterizer<Pixel, zoom>::paint()
{
	PRT_DEBUG("V9990SDLRasterizer::paint()");

	SDL_BlitSurface(workScreen, NULL, screen, NULL);
}

template <class Pixel, Renderer::Zoom zoom>
const string& V9990SDLRasterizer<Pixel, zoom>::getName()
{
	static const string NAME = "V9990SDLRasterizer";

	PRT_DEBUG("V9990SDLRasterizer::getName() = " << NAME);
	return NAME;
}

template <class Pixel, Renderer::Zoom zoom>
void V9990SDLRasterizer<Pixel, zoom>::reset()
{
	PRT_DEBUG("V9990SDLRasterizer::reset");

	bgColor = vdp->getBackDropColor();
	displayMode = vdp->getDisplayMode();
	colorMode = vdp->getColorMode();
	imageWidth = vdp->getImageWidth();
}

template <class Pixel, Renderer::Zoom zoom>
void V9990SDLRasterizer<Pixel, zoom>::frameStart()
{
	PRT_DEBUG("V9990SDLRasterizer::frameStart()");
}

template <class Pixel, Renderer::Zoom zoom>
void V9990SDLRasterizer<Pixel, zoom>::frameEnd()
{
	PRT_DEBUG("V9990SDLRasterizer::frameEnd()");
}

template <class Pixel, Renderer::Zoom zoom>
void V9990SDLRasterizer<Pixel, zoom>::setDisplayMode(V9990DisplayMode mode)
{
	PRT_DEBUG("V9990SDLRasterizer::setDisplayMode(" << dec <<
	          (int) mode << ")");

	this->displayMode = mode;
	bitmapConverter.setDisplayMode(mode);
}

template <class Pixel, Renderer::Zoom zoom>
void V9990SDLRasterizer<Pixel, zoom>::setColorMode(V9990ColorMode mode)
{
	PRT_DEBUG("V9990SDLRasterizer::setColorMode(" << dec <<
	          (int) mode << ")");

	this->colorMode = mode;
	bitmapConverter.setColorMode(mode);
}

template <class Pixel, Renderer::Zoom zoom>
void V9990SDLRasterizer<Pixel, zoom>::setImageWidth(int width)
{
	// sync?
	imageWidth = width;
}

#define translateX(x)  ((x)/((zoom == Renderer::ZOOM_256)?8:4))
#define translateY(y)  (y*((zoom == Renderer::ZOOM_256)?1:2))

template <class Pixel, Renderer::Zoom zoom>
void V9990SDLRasterizer<Pixel, zoom>::drawBorder(
	int fromX, int fromY, int toX, int toY)
{
	PRT_DEBUG("V9990SDLRasterizer::drawBorder(" << dec <<
	          fromX << "," << fromY << "," << toX << "," << toY << ")");

	int width = toX - fromX;
	int height = toY - fromY;

	if((width > 0) && (height > 0)) {
		SDL_Rect rect;

		rect.x = translateX(fromX); rect.y = translateY(fromY);
		rect.w = translateX(width); rect.h = translateY(height);

		SDL_FillRect(workScreen, &rect, bgColor);
	}
}

template <class Pixel, Renderer::Zoom zoom>
void V9990SDLRasterizer<Pixel, zoom>::drawDisplay(
	int fromX, int fromY,
	int displayX, int displayY, int displayWidth, int displayHeight )
{
	PRT_DEBUG("V9990SDLRasterizer::drawDisplay(" << dec <<
	          fromX << "," << fromY << "," <<
	          displayX << "," << displayY << "," <<
	          displayWidth << "," << displayHeight << ")");

	if((displayWidth > 0) && (displayHeight > 0)) {
		SDL_Rect rect;
		rect.x = translateX(fromX);
		rect.y = translateY(fromY);
		rect.w = translateX(displayWidth);
		rect.h = translateY(displayHeight);

		if(displayMode == P1 || displayMode == P2) {
			// not implemented yet

			SDL_FillRect(workScreen, &rect, bgColor);
		} else {
			SDL_FillRect(workScreen, &rect, (Pixel) 0);
			int vramStep;
			Pixel* pixelPtr = (Pixel*)(
		                   (byte*)workScreen->pixels +
		                  + translateY(fromY) * workScreen->pitch
		                  + translateX(fromX) * sizeof(Pixel));

			displayX = V9990::UCtoX(displayX, displayMode);
			displayWidth = V9990::UCtoX(displayWidth, displayMode);
			byte* vramPtr = vram->getData() +
			                V9990::XtoVRAM(&displayX, colorMode);
			switch(colorMode) {
				case BP2:
					vramStep = imageWidth / 4;
					break;
				case BP4:
				case PP:
					vramStep = imageWidth / 2;
					break;
				case BD16:
					vramStep = imageWidth * 2;
					break;
				default:
					vramStep = imageWidth;
			}
				
			while(displayHeight--) {
				bitmapConverter.convertLine(pixelPtr, vramPtr, displayWidth);
				
				// next line
				vramPtr += vramStep;
				pixelPtr += workScreen->w * translateY(1);
			}
		}
	}
}


template <class Pixel, Renderer::Zoom zoom>
void V9990SDLRasterizer<Pixel, zoom>::precalcPalettes(void)
{
	PRT_DEBUG("V9990SDLRasterizer::precalcPalettes()");

	int mapRG[] = { 0, 4, 9, 13, 18, 22, 27, 31 };
	int mapB[]  = { 0, 11, 21, 31 };

	// the 32768 color palette
	for(int r = 0; r < 32; r++) {
		for(int g = 0; g < 32; g++)
			for(int b = 0; b < 32; b++) 
				palette32768[(r<<10) + (g<<5) + b] =
					SDL_MapRGB(screen->format,
					           (int)(((float) r/31.0) * 255),
							   (int)(((float) g/31.0) * 255),
							   (int)(((float) b/31.0) * 255));
	}
	
	// the 256 color palette
	for(int r = 0; r < 8; r++)
		for(int g = 0; g < 8; g++)
			for(int b=0; b < 4; b++)
				palette256[(r<<5) + (g<<2) + b] = 
					palette32768[(mapRG[r]<<10) + (mapRG[g]<<5) + mapB[b]];
	
	// TODO Get 64 color palette from VDP
}

template <class Pixel, Renderer::Zoom zoom>
void V9990SDLRasterizer<Pixel, zoom>::setPalette(int index,
                                                 byte r, byte g, byte b)
{
	palette64[index & 63] = palette32768[((r & 31) << 10) + 
	                                     ((g & 31) << 5)  +
										  (b & 31)]; 
}


template <class Pixel, Renderer::Zoom zoom>
void V9990SDLRasterizer<Pixel, zoom>::setBackgroundColor(int index)
{
	bgColor = palette64[index & 63];
}
} // namespace openmsx

