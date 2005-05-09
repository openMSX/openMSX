// $Id$

#include "Deinterlacer.hh"
#include "Scaler.hh"

namespace openmsx {

template <class Pixel>
void Deinterlacer<Pixel>::deinterlaceLine256(
	SDL_Surface* src0, SDL_Surface* src1, int srcY,
	SDL_Surface* dst, int dstY )
{
	Scaler<Pixel>::scaleLine(src0, srcY, dst, dstY);
	Scaler<Pixel>::scaleLine(src1, srcY, dst, dstY + 1);
}

template <class Pixel>
void Deinterlacer<Pixel>::deinterlaceLine512(
	SDL_Surface* src0, SDL_Surface* src1, int srcY,
	SDL_Surface* dst, int dstY )
{
	Scaler<Pixel>::copyLine(src0, srcY, dst, dstY);
	Scaler<Pixel>::copyLine(src1, srcY, dst, dstY + 1);
}


// Force template instantiation.
template class Deinterlacer<word>;
template class Deinterlacer<unsigned int>;

} // namespace openmsx

