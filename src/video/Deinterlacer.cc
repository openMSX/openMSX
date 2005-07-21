// $Id$

#include "Deinterlacer.hh"
#include "RawFrame.hh"
#include "Scaler.hh"

namespace openmsx {

template <class Pixel>
void Deinterlacer<Pixel>::deinterlaceLine256(
	RawFrame& src0, RawFrame& src1, SDL_Surface* dst, unsigned y)
{
	Pixel* srcLine0 = src0.getPixelPtr(0, y, (Pixel*)0);
	Pixel* dstLine0 = Scaler<Pixel>::linePtr(dst, 2 * y + 0);
	Scaler<Pixel>::scaleLine(srcLine0, dstLine0, 320);

	Pixel* srcLine1 = src1.getPixelPtr(0, y, (Pixel*)0);
	Pixel* dstLine1 = Scaler<Pixel>::linePtr(dst, 2 * y + 1);
	Scaler<Pixel>::scaleLine(srcLine1, dstLine1, 320);
}

template <class Pixel>
void Deinterlacer<Pixel>::deinterlaceLine512(
	RawFrame& src0, RawFrame& src1, SDL_Surface* dst, unsigned y)
{
	Pixel* srcLine0 = src0.getPixelPtr(0, y, (Pixel*)0);
	Pixel* dstLine0 = Scaler<Pixel>::linePtr(dst, 2 * y + 0);
	Scaler<Pixel>::copyLine(srcLine0, dstLine0, 640);

	Pixel* srcLine1 = src1.getPixelPtr(0, y, (Pixel*)0);
	Pixel* dstLine1 = Scaler<Pixel>::linePtr(dst, 2 * y + 1);
	Scaler<Pixel>::copyLine(srcLine1, dstLine1, 640);
}


// Force template instantiation.
template class Deinterlacer<word>;
template class Deinterlacer<unsigned int>;

} // namespace openmsx

