// $Id$

#include "SimpleScaler.hh"
#include "openmsx.hh"
#include <cassert>


namespace openmsx {

// Force template instantiation.
template class SimpleScaler<byte>;
template class SimpleScaler<word>;
template class SimpleScaler<unsigned int>;

template <class Pixel>
SimpleScaler<Pixel>::SimpleScaler()
{
}

template <class Pixel>
void SimpleScaler<Pixel>::scale256(
	SDL_Surface* src, int srcY, int endSrcY,
	SDL_Surface* dst, int dstY )
{
	const int WIDTH = 320; // TODO: Specify this in a clean way.
	assert(dst->w == WIDTH * 2);
	while (srcY < endSrcY) {
		const Pixel* srcLine = Scaler<Pixel>::linePtr(src, srcY++);
		Pixel* dstUpper = Scaler<Pixel>::linePtr(dst, dstY++);
		Pixel* dstLower =
			dstY == dst->h ? dstUpper : Scaler<Pixel>::linePtr(dst, dstY++);
		for (int x = 0; x < WIDTH; x++) {
			dstUpper[x * 2] = dstUpper[x * 2 + 1] =
			dstLower[x * 2] = dstLower[x * 2 + 1] =
				srcLine[x];
		}
	}
}

template <class Pixel>
void SimpleScaler<Pixel>::scale512(
	SDL_Surface* src, int srcY, int endSrcY,
	SDL_Surface* dst, int dstY )
{
	while (srcY < endSrcY) {
		Scaler<Pixel>::copyLine(src, srcY, dst, dstY++);
		if (dstY == dst->h) break;
		Scaler<Pixel>::copyLine(src, srcY, dst, dstY++);
		srcY++;
	}
}

} // namespace openmsx

