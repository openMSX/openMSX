// $Id$

#include "V9990SDLRasterizer.hh"
#include "V9990.hh"
#include "V9990VRAM.hh"
#include "openmsx.hh"
#include <SDL.h>


namespace openmsx {

// Force template instantiation.
template class V9990SDLRasterizer<Uint16, Renderer::ZOOM_256>;
template class V9990SDLRasterizer<Uint16, Renderer::ZOOM_REAL>;
template class V9990SDLRasterizer<Uint32, Renderer::ZOOM_256>;
template class V9990SDLRasterizer<Uint32, Renderer::ZOOM_REAL>;

template <class Pixel, Renderer::Zoom zoom>
V9990SDLRasterizer<Pixel, zoom>::V9990SDLRasterizer(
	V9990* vdp, SDL_Surface* screen)
{
	this->vdp = vdp;
	vram = 0; // TODO: vdp->getVRAM();
	this->screen = screen;
}

template <class Pixel, Renderer::Zoom zoom>
V9990SDLRasterizer<Pixel, zoom>::~V9990SDLRasterizer()
{
}

template <class Pixel, Renderer::Zoom zoom>
void V9990SDLRasterizer<Pixel, zoom>::paint()
{
	Pixel step = ((Pixel)-1) / (screen->w * screen->h);
	if (step == 0) step = 1;
	Pixel colour = 0;
	for (int y = 0; y < screen->h; y++) {
		Pixel* linePtr = (Pixel*)
			( (byte*)screen->pixels + y * screen->pitch );
		for (int x = 0; x < screen->w; x++) {
			linePtr[x] = colour;
			colour += step;
		}
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
}

template <class Pixel, Renderer::Zoom zoom>
void V9990SDLRasterizer<Pixel, zoom>::frameStart()
{
}

template <class Pixel, Renderer::Zoom zoom>
void V9990SDLRasterizer<Pixel, zoom>::frameEnd()
{
}

template <class Pixel, Renderer::Zoom zoom>
void V9990SDLRasterizer<Pixel, zoom>::setDisplayMode(
	V9990DisplayMode displayMode )
{
}

template <class Pixel, Renderer::Zoom zoom>
void V9990SDLRasterizer<Pixel, zoom>::setColorMode(
	V9990ColorMode colorMode )
{
}

template <class Pixel, Renderer::Zoom zoom>
void V9990SDLRasterizer<Pixel, zoom>::drawBorder(
	int fromX, int fromY, int limitX, int limitY)
{
}

template <class Pixel, Renderer::Zoom zoom>
void V9990SDLRasterizer<Pixel, zoom>::drawDisplay(
	int fromX, int fromY,
	int displayX, int displayY, int displayWidth, int displayHeight )
{
}

} // namespace openmsx

