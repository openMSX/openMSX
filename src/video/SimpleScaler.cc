// $Id$

#include "SimpleScaler.hh"
#include "openmsx.hh"
#include <cassert>


namespace openmsx {

template <class Pixel>
inline static Pixel* linePtr(SDL_Surface* surface, int y)
{
	assert(0 <= y && y < surface->h);
	return (Pixel*)((byte*)surface->pixels + y * surface->pitch);
}

// Force template instantiation.
template class SimpleScaler<byte>;
template class SimpleScaler<word>;
template class SimpleScaler<unsigned int>;

template <class Pixel>
SimpleScaler<Pixel>::SimpleScaler()
{
}

template <class Pixel>
inline void SimpleScaler<Pixel>::copyLine(
	SDL_Surface* src, int srcY, SDL_Surface* dst, int dstY )
{
	assert(src->w == dst->w);
	memcpy(
		(byte*)linePtr<Pixel>(dst, dstY),
		(byte*)linePtr<Pixel>(src, srcY),
		dst->w * sizeof(Pixel)
		);
}

template <class Pixel>
void SimpleScaler<Pixel>::scale256(
	SDL_Surface* src, int srcY, int endSrcY,
	SDL_Surface* dst, int dstY )
{
	int width = 320; // TODO: Specify this in a clean way.
	assert(dst->w == width * 2);
	while (srcY < endSrcY) {
		const Pixel* srcLine = linePtr<Pixel>(src, srcY++);
		Pixel* dstUpper = linePtr<Pixel>(dst, dstY++);
		Pixel* dstLower =
			dstY == dst->h ? dstUpper : linePtr<Pixel>(dst, dstY++);
		for (int x = 0; x < width; x++) {
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
		copyLine(src, srcY, dst, dstY++);
		if (dstY == dst->h) break;
		copyLine(src, srcY, dst, dstY++);
		srcY++;
	}
}

} // namespace openmsx

