// $Id$

#include "SimpleScaler.hh"
#include "openmsx.hh"
#include <cassert>


namespace openmsx {

template <class Pixel>
inline static Pixel* linePtr(SDL_Surface* surface, int y)
{
	if (y < 0) y = 0;
	if (y >= surface->h) y = surface->h - 1;
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
	memcpy(
		(byte*)dst->pixels + dstY * dst->pitch,
		(byte*)src->pixels + srcY * src->pitch,
		src->pitch
		);
}

template <class Pixel>
void SimpleScaler<Pixel>::scaleLine256(
	SDL_Surface* src, int srcY, SDL_Surface* dst, int dstY )
{
	const Pixel* srcLine = linePtr<Pixel>(src, srcY);
	Pixel* dstUpper = linePtr<Pixel>(dst, dstY);
	Pixel* dstLower = linePtr<Pixel>(dst, dstY + 1);
	int width = 320; // TODO: Specify this in a clean way.
	assert(dst->w == width * 2);
	for (int x = 0; x < width; x++) {
		dstUpper[x * 2] = dstUpper[x * 2 + 1] =
		dstLower[x * 2] = dstLower[x * 2 + 1] =
			srcLine[x];
	}
}

template <class Pixel>
void SimpleScaler<Pixel>::scaleLine512(
	SDL_Surface* src, int srcY, SDL_Surface* dst, int dstY )
{
	copyLine(src, srcY, dst, dstY);
	copyLine(src, srcY, dst, dstY + 1);
}

} // namespace openmsx

