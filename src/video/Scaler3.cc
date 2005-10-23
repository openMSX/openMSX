// $Id$

#include "Scaler3.hh"
#include "FrameSource.hh"
#include <algorithm>

namespace openmsx {

template <class Pixel>
Scaler3<Pixel>::Scaler3(SDL_PixelFormat* format)
	: Scaler<Pixel>(format)
{
}

// TODO: See comment for Scaler2::scale256.
/*template <class Pixel>
void Scaler3<Pixel>::scale256(FrameSource& src, SDL_Surface* dst,
                             unsigned startY, unsigned endY, bool lower)
{
	unsigned y1 = 3 * startY + (lower ? 1 : 0);
	unsigned y2 = 3 * endY   + (lower ? 1 : 0);
	for (unsigned y = y1; y < y2; y += 3, ++startY) {
		const Pixel* srcLine = src.getLinePtr(startY, (Pixel*)0);
		Pixel* dstLine0 = Scaler<Pixel>::linePtr(dst, y + 0);
		scale_1on3(srcLine, dstLine0, 320);
		Pixel* dstLine1 = Scaler<Pixel>::linePtr(dst, y + 1);
		copyLine(dstLine0, dstLine1, 960);
		if (y == (720 - 2)) break;
		Pixel* dstLine2 = Scaler<Pixel>::linePtr(dst, y + 2);
		copyLine(dstLine0, dstLine2, 960);
	}
}*/

template <class Pixel>
void Scaler3<Pixel>::scale256(FrameSource& src0, FrameSource& src1, SDL_Surface* dst,
                             unsigned startY, unsigned endY)
{
	for (unsigned y = startY; y < endY; ++y) {
		const Pixel* srcLine0 = src0.getLinePtr(y, (Pixel*)0);
		Pixel* dstLine0 = Scaler<Pixel>::linePtr(dst, 3 * y + 0);
		scale_1on3(srcLine0, dstLine0, 320);

		const Pixel* srcLine1 = src1.getLinePtr(y, (Pixel*)0);
		Pixel* dstLine2 = Scaler<Pixel>::linePtr(dst, 3 * y + 2);
		scale_1on3(srcLine1, dstLine2, 320);

		Pixel* dstLine1 = Scaler<Pixel>::linePtr(dst, 3 * y + 1);
		average(dstLine0, dstLine2, dstLine1, 960);
	}
}

template <class Pixel>
void Scaler3<Pixel>::scale512(FrameSource& src, SDL_Surface* dst,
                                 unsigned startY, unsigned endY, bool lower)
{
	unsigned y1 = 3 * startY + (lower ? 1 : 0);
	unsigned y2 = 3 * endY   + (lower ? 1 : 0);
	for (unsigned y = y1; y < y2; y += 3, ++startY) {
		const Pixel* srcLine = src.getLinePtr(startY, (Pixel*)0);
		Pixel* dstLine0 = Scaler<Pixel>::linePtr(dst, y + 0);
		scale_2on3(srcLine, dstLine0, 640);
		Pixel* dstLine1 = Scaler<Pixel>::linePtr(dst, y + 1);
		copyLine(dstLine0, dstLine1, 960);
		if (y == (720 - 2)) break;
		Pixel* dstLine2 = Scaler<Pixel>::linePtr(dst, y + 2);
		copyLine(dstLine0, dstLine2, 960);
	}
}

template <class Pixel>
void Scaler3<Pixel>::scale512(FrameSource& src0, FrameSource& src1, SDL_Surface* dst,
                             unsigned startY, unsigned endY)
{
	for (unsigned y = startY; y < endY; ++y) {
		const Pixel* srcLine0 = src0.getLinePtr(y, (Pixel*)0);
		Pixel* dstLine0 = Scaler<Pixel>::linePtr(dst, 3 * y + 0);
		scale_2on3(srcLine0, dstLine0, 640);

		const Pixel* srcLine1 = src1.getLinePtr(y, (Pixel*)0);
		Pixel* dstLine2 = Scaler<Pixel>::linePtr(dst, 3 * y + 2);
		scale_2on3(srcLine1, dstLine2, 640);

		Pixel* dstLine1 = Scaler<Pixel>::linePtr(dst, 3 * y + 1);
		average(dstLine0, dstLine2, dstLine1, 960);
	}
}

template <class Pixel>
void Scaler3<Pixel>::scale192(FrameSource& src, SDL_Surface* dst,
                             unsigned startY, unsigned endY, bool lower)
{
	unsigned y1 = 3 * startY + (lower ? 1 : 0);
	unsigned y2 = 3 * endY   + (lower ? 1 : 0);
	for (unsigned y = y1; y < y2; y += 3, ++startY) {
		const Pixel* srcLine = src.getLinePtr(startY, (Pixel*)0);
		Pixel* dstLine0 = Scaler<Pixel>::linePtr(dst, y + 0);
		scale_2on9(srcLine, dstLine0, 212); // TODO
		Pixel* dstLine1 = Scaler<Pixel>::linePtr(dst, y + 1);
		copyLine(dstLine0, dstLine1, 960);
		if (y == (720 - 2)) break;
		Pixel* dstLine2 = Scaler<Pixel>::linePtr(dst, y + 2);
		copyLine(dstLine0, dstLine2, 960);
	}
}

template <class Pixel>
void Scaler3<Pixel>::scale192(FrameSource& src0, FrameSource& src1, SDL_Surface* dst,
                             unsigned startY, unsigned endY)
{
	for (unsigned y = startY; y < endY; ++y) {
		const Pixel* srcLine0 = src0.getLinePtr(y, (Pixel*)0);
		Pixel* dstLine0 = Scaler<Pixel>::linePtr(dst, 3 * y + 0);
		scale_2on9(srcLine0, dstLine0, 212); //

		const Pixel* srcLine1 = src1.getLinePtr(y, (Pixel*)0);
		Pixel* dstLine2 = Scaler<Pixel>::linePtr(dst, 3 * y + 2);
		scale_2on9(srcLine1, dstLine2, 212); //

		Pixel* dstLine1 = Scaler<Pixel>::linePtr(dst, 3 * y + 1);
		average(dstLine0, dstLine2, dstLine1, 960);
	}
}

template <class Pixel>
void Scaler3<Pixel>::scale384(FrameSource& src, SDL_Surface* dst,
                             unsigned startY, unsigned endY, bool lower)
{
	unsigned y1 = 3 * startY + (lower ? 1 : 0);
	unsigned y2 = 3 * endY   + (lower ? 1 : 0);
	for (unsigned y = y1; y < y2; y += 3, ++startY) {
		const Pixel* srcLine = src.getLinePtr(startY, (Pixel*)0);
		Pixel* dstLine0 = Scaler<Pixel>::linePtr(dst, y + 0);
		scale_4on9(srcLine, dstLine0, 426); // TODO
		Pixel* dstLine1 = Scaler<Pixel>::linePtr(dst, y + 1);
		copyLine(dstLine0, dstLine1, 960);
		if (y == (720 - 2)) break;
		Pixel* dstLine2 = Scaler<Pixel>::linePtr(dst, y + 2);
		copyLine(dstLine0, dstLine2, 960);
	}
}

template <class Pixel>
void Scaler3<Pixel>::scale384(FrameSource& src0, FrameSource& src1, SDL_Surface* dst,
                             unsigned startY, unsigned endY)
{
	for (unsigned y = startY; y < endY; ++y) {
		const Pixel* srcLine0 = src0.getLinePtr(y, (Pixel*)0);
		Pixel* dstLine0 = Scaler<Pixel>::linePtr(dst, 3 * y + 0);
		scale_4on9(srcLine0, dstLine0, 424); //

		const Pixel* srcLine1 = src1.getLinePtr(y, (Pixel*)0);
		Pixel* dstLine2 = Scaler<Pixel>::linePtr(dst, 3 * y + 2);
		scale_4on9(srcLine1, dstLine2, 424); //

		Pixel* dstLine1 = Scaler<Pixel>::linePtr(dst, 3 * y + 1);
		average(dstLine0, dstLine2, dstLine1, 960);
	}
}

template <class Pixel>
void Scaler3<Pixel>::scale640(FrameSource& /*src*/, SDL_Surface* /*dst*/,
                             unsigned /*startY*/, unsigned /*endY*/, bool /*lower*/)
{
	// TODO
}

template <class Pixel>
void Scaler3<Pixel>::scale640(FrameSource& /*src0*/, FrameSource& /*src1*/, SDL_Surface* /*dst*/,
                             unsigned /*startY*/, unsigned /*endY*/)
{
	// TODO
}

template <class Pixel>
void Scaler3<Pixel>::scale768(FrameSource& src, SDL_Surface* dst,
                             unsigned startY, unsigned endY, bool lower)
{
	unsigned y1 = 3 * startY + (lower ? 1 : 0);
	unsigned y2 = 3 * endY   + (lower ? 1 : 0);
	for (unsigned y = y1; y < y2; y += 3, ++startY) {
		const Pixel* srcLine = src.getLinePtr(startY, (Pixel*)0);
		Pixel* dstLine0 = Scaler<Pixel>::linePtr(dst, y + 0);
		scale_8on9(srcLine, dstLine0, 848); // TODO
		Pixel* dstLine1 = Scaler<Pixel>::linePtr(dst, y + 1);
		copyLine(dstLine0, dstLine1, 960);
		if (y == (720 - 2)) break;
		Pixel* dstLine2 = Scaler<Pixel>::linePtr(dst, y + 2);
		copyLine(dstLine0, dstLine2, 960);
	}
}

template <class Pixel>
void Scaler3<Pixel>::scale768(FrameSource& src0, FrameSource& src1, SDL_Surface* dst,
                             unsigned startY, unsigned endY)
{
	for (unsigned y = startY; y < endY; ++y) {
		const Pixel* srcLine0 = src0.getLinePtr(y, (Pixel*)0);
		Pixel* dstLine0 = Scaler<Pixel>::linePtr(dst, 3 * y + 0);
		scale_8on9(srcLine0, dstLine0, 848); //

		const Pixel* srcLine1 = src1.getLinePtr(y, (Pixel*)0);
		Pixel* dstLine2 = Scaler<Pixel>::linePtr(dst, 3 * y + 2);
		scale_8on9(srcLine1, dstLine2, 848); //

		Pixel* dstLine1 = Scaler<Pixel>::linePtr(dst, 3 * y + 1);
		average(dstLine0, dstLine2, dstLine1, 960);
	}
}

template <class Pixel>
void Scaler3<Pixel>::scale1024(FrameSource& src, SDL_Surface* dst,
                              unsigned startY, unsigned endY, bool lower)
{
	unsigned y1 = 3 * startY + (lower ? 1 : 0);
	unsigned y2 = 3 * endY   + (lower ? 1 : 0);
	for (unsigned y = y1; y < y2; y += 3, ++startY) {
		const Pixel* srcLine = src.getLinePtr(startY, (Pixel*)0);
		Pixel* dstLine0 = Scaler<Pixel>::linePtr(dst, y + 0);
		scale_4on3(srcLine, dstLine0, 1280);
		Pixel* dstLine1 = Scaler<Pixel>::linePtr(dst, y + 1);
		copyLine(dstLine0, dstLine1, 960);
		if (y == (720 - 2)) break;
		Pixel* dstLine2 = Scaler<Pixel>::linePtr(dst, y + 2);
		copyLine(dstLine0, dstLine2, 960);
	}
}

template <class Pixel>
void Scaler3<Pixel>::scale1024(FrameSource& src0, FrameSource& src1, SDL_Surface* dst,
                              unsigned startY, unsigned endY)
{
	for (unsigned y = startY; y < endY; ++y) {
		const Pixel* srcLine0 = src0.getLinePtr(y, (Pixel*)0);
		Pixel* dstLine0 = Scaler<Pixel>::linePtr(dst, 3 * y + 0);
		scale_4on3(srcLine0, dstLine0, 1280);

		const Pixel* srcLine1 = src1.getLinePtr(y, (Pixel*)0);
		Pixel* dstLine2 = Scaler<Pixel>::linePtr(dst, 3 * y + 2);
		scale_4on3(srcLine1, dstLine2, 1280);

		Pixel* dstLine1 = Scaler<Pixel>::linePtr(dst, 3 * y + 1);
		average(dstLine0, dstLine2, dstLine1, 960);
	}
}


// Force template instantiation.
template class Scaler3<word>;
template class Scaler3<unsigned>;

} // namespace openmsx

