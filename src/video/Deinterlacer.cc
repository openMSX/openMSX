// $Id$

#include "Deinterlacer.hh"
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
template class Deinterlacer<byte>;
template class Deinterlacer<word>;
template class Deinterlacer<unsigned int>;

template <class Pixel>
Deinterlacer<Pixel>::Deinterlacer()
{
}

template <class Pixel>
inline void Deinterlacer<Pixel>::copyLine(
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
inline void Deinterlacer<Pixel>::scaleLine(
	SDL_Surface* src, int srcY, SDL_Surface* dst, int dstY )
{
	const Pixel* srcLine = linePtr<Pixel>(src, srcY);
	Pixel* dstLine = linePtr<Pixel>(dst, dstY);
	int width = 320; // TODO: Specify this in a clean way.
	assert(dst->w == width * 2);
	for (int x = 0; x < width; x++) {
		dstLine[x * 2] = dstLine[x * 2 + 1] = srcLine[x];
	}
}

template <class Pixel>
void Deinterlacer<Pixel>::deinterlaceLine256(
	SDL_Surface* src0, SDL_Surface* src1, int srcY,
	SDL_Surface* dst, int dstY )
{
	scaleLine(src0, srcY, dst, dstY);
	scaleLine(src1, srcY, dst, dstY + 1);
}

template <class Pixel>
void Deinterlacer<Pixel>::deinterlaceLine512(
	SDL_Surface* src0, SDL_Surface* src1, int srcY,
	SDL_Surface* dst, int dstY )
{
	copyLine(src0, srcY, dst, dstY);
	copyLine(src1, srcY, dst, dstY + 1);
}

} // namespace openmsx

