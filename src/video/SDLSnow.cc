// $Id$

#include "SDLSnow.hh"
#include "OutputSurface.hh"
#include "openmsx.hh"
#include "build-info.hh"
#include <string.h>

namespace openmsx {

template <class Pixel>
SDLSnow<Pixel>::SDLSnow(OutputSurface& output_)
	: Layer(COVER_FULL, Z_BACKGROUND)
	, output(output_)
{
	// Precalc gray values for noise
	for (unsigned i = 0; i < 256; i++) {
		double t = i / 255.0;
		gray[i] = output.mapRGB(t, t, t);
	}
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
	output.lock();
	const unsigned width = output.getWidth();
	const unsigned height = output.getHeight();
	for (unsigned y = 0; y < height; y += 2) {
		Pixel* p0 = output.getLinePtrDirect<Pixel>(y + 0);
		Pixel* p1 = output.getLinePtrDirect<Pixel>(y + 1);
		for (unsigned x = 0; x < width; x += 2) {
			byte a = byte(random());
			p0[x + 0] = p0[x + 1] = gray[a];
		}
		memcpy(p1, p0, width * sizeof(Pixel));
	}

	// TODO: Mark dirty in 100ms.
}

template <class Pixel>
const std::string& SDLSnow<Pixel>::getName()
{
	static const std::string NAME = "SDLSnow";
	return NAME;
}


// Force template instantiation.
#if HAVE_16BPP
template class SDLSnow<word>;
#endif
#if HAVE_32BPP
template class SDLSnow<unsigned>;
#endif

} // namespace openmsx

