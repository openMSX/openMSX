// $Id$

#include "SDLSnow.hh"
#include "Display.hh"
#include <SDL.h>

using std::string;

namespace openmsx {

// Force template instantiation.
template class SDLSnow<Uint16>;
template class SDLSnow<Uint32>;

template <class Pixel>
SDLSnow<Pixel>::SDLSnow(SDL_Surface* screen)
	: Layer(COVER_FULL, Z_BACKGROUND)
	, screen(screen)
{
	// Precalc gray values for noise
	for (unsigned i = 0; i < 256; i++) {
		gray[i] = SDL_MapRGB(screen->format, i, i, i);
	}

	// Register as display layer.
	Display::instance().addLayer(this);
}

template <class Pixel>
SDLSnow<Pixel>::~SDLSnow()
{
}

// random routine, less random than libc rand(), but a lot faster
static int random() {
	static int seed = 1;

	const int IA = 16807;
	const int IM = 2147483647;
	const int IQ = 127773;
	const int IR = 2836;

	int k = seed / IQ;
	seed = IA * (seed - k * IQ) - IR * k;
	if (seed < 0) seed += IM;
	return seed;
}

template <class Pixel>
void SDLSnow<Pixel>::paint()
{
	const unsigned WIDTH = screen->w;
	const unsigned HEIGHT = screen->h;
	Pixel* pixels = (Pixel*)screen->pixels;
	for (unsigned y = 0; y < HEIGHT; y += 2) {
		Pixel* p = &pixels[WIDTH * y];
		for (unsigned x = 0; x < WIDTH; x += 2) {
			byte a = (byte)random();
			p[x + 0] = p[x + 1] = gray[a];
		}
		memcpy(p + WIDTH, p, WIDTH * sizeof(Pixel));
	}

	// TODO: Mark dirty in 100ms.
}

template <class Pixel>
const string& SDLSnow<Pixel>::getName()
{
	static const string NAME = "SDLSnow";
	return NAME;
}

} // namespace openmsx

