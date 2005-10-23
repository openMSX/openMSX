// $Id$

#include "Scaler2.hh"
#include "FrameSource.hh"
#include <algorithm>

namespace openmsx {

template <class Pixel>
Scaler2<Pixel>::Scaler2(SDL_PixelFormat* format)
	: Scaler<Pixel>(format)
{
}

// TODO: In theory it's nice to have this as a fallback method, but in practice
//       all subclasses override this method, so should we keep it or not?
//       And if we keep it, should it be commented out like this until we
//       need it to reduce the executable size?
//       See also Scaler3::scale256.
/*template <class Pixel>
void Scaler2<Pixel>::scale256(
	FrameSource& src, unsigned srcStartY, unsigned srcEndY,
	SDL_Surface* dst, unsigned dstStartY, unsigned dstEndY
) {
	for (unsigned y = dstStartY; y < dstEndY; y += 2, ++srcStartY) {
		const Pixel* srcLine = src.getLinePtr(srcStartY, (Pixel*)0);
		Pixel* dstLine1 = Scaler<Pixel>::linePtr(dst, y + 0);
		scaleLine(srcLine, dstLine1, 320);
		if (y == (dstEndY - 1)) break;
		Pixel* dstLine2 = Scaler<Pixel>::linePtr(dst, y + 1);
		scaleLine(srcLine, dstLine2, 320);
	}
}*/

template <class Pixel>
void Scaler2<Pixel>::scale256(FrameSource& src0, FrameSource& src1, SDL_Surface* dst,
                             unsigned startY, unsigned endY)
{
	for (unsigned y = startY; y < endY; ++y) {
		const Pixel* srcLine0 = src0.getLinePtr(y, (Pixel*)0);
		Pixel* dstLine0 = Scaler<Pixel>::linePtr(dst, 2 * y + 0);
		scaleLine(srcLine0, dstLine0, 320);

		const Pixel* srcLine1 = src1.getLinePtr(y, (Pixel*)0);
		Pixel* dstLine1 = Scaler<Pixel>::linePtr(dst, 2 * y + 1);
		scaleLine(srcLine1, dstLine1, 320);
	}
}

template <class Pixel>
void Scaler2<Pixel>::scale512(FrameSource& src, SDL_Surface* dst,
                             unsigned startY, unsigned endY, bool lower)
{
	unsigned y1 = 2 * startY + (lower ? 1 : 0);
	unsigned y2 = 2 * endY   + (lower ? 1 : 0);
	for (unsigned y = y1; y < y2; y += 2, ++startY) {
		const Pixel* srcLine = src.getLinePtr(startY, (Pixel*)0);
		Pixel* dstLine1 = Scaler<Pixel>::linePtr(dst, y + 0);
		copyLine(srcLine, dstLine1, 640);
		if (y == (480 - 1)) break;
		Pixel* dstLine2 = Scaler<Pixel>::linePtr(dst, y + 1);
		copyLine(srcLine, dstLine2, 640);
	}
}

template <class Pixel>
void Scaler2<Pixel>::scale512(FrameSource& src0, FrameSource& src1, SDL_Surface* dst,
                             unsigned startY, unsigned endY)
{
	for (unsigned y = startY; y < endY; ++y) {
		const Pixel* srcLine0 = src0.getLinePtr(y, (Pixel*)0);
		Pixel* dstLine0 = Scaler<Pixel>::linePtr(dst, 2 * y + 0);
		copyLine(srcLine0, dstLine0, 640);

		const Pixel* srcLine1 = src1.getLinePtr(y, (Pixel*)0);
		Pixel* dstLine1 = Scaler<Pixel>::linePtr(dst, 2 * y + 1);
		copyLine(srcLine1, dstLine1, 640);
	}
}

template <class Pixel>
void Scaler2<Pixel>::scale192(FrameSource& src, SDL_Surface* dst,
                             unsigned startY, unsigned endY, bool lower)
{
	unsigned y1 = 2 * startY + (lower ? 1 : 0);
	unsigned y2 = 2 * endY   + (lower ? 1 : 0);
	for (unsigned y = y1; y < y2; y += 2, ++startY) {
		const Pixel* srcLine = src.getLinePtr(startY, (Pixel*)0);
		Pixel* dstLine1 = Scaler<Pixel>::linePtr(dst, y + 0);
		scale_1on3(srcLine, dstLine1, 213); // TODO
		if (y == (480 - 1)) break;
		Pixel* dstLine2 = Scaler<Pixel>::linePtr(dst, y + 1);
		copyLine(dstLine1, dstLine2, 640);
	}
}

template <class Pixel>
void Scaler2<Pixel>::scale192(FrameSource& src0, FrameSource& src1, SDL_Surface* dst,
                             unsigned startY, unsigned endY)
{
	for (unsigned y = startY; y < endY; ++y) {
		const Pixel* srcLine0 = src0.getLinePtr(y, (Pixel*)0);
		Pixel* dstLine0 = Scaler<Pixel>::linePtr(dst, 2 * y + 0);
		scale_1on3(srcLine0, dstLine0, 213); // TODO

		const Pixel* srcLine1 = src1.getLinePtr(y, (Pixel*)0);
		Pixel* dstLine1 = Scaler<Pixel>::linePtr(dst, 2 * y + 1);
		scale_1on3(srcLine1, dstLine1, 213); // TODO
	}
}

template <class Pixel>
void Scaler2<Pixel>::scale384(FrameSource& src, SDL_Surface* dst,
                             unsigned startY, unsigned endY, bool lower)
{
	unsigned y1 = 2 * startY + (lower ? 1 : 0);
	unsigned y2 = 2 * endY   + (lower ? 1 : 0);
	for (unsigned y = y1; y < y2; y += 2, ++startY) {
		const Pixel* srcLine = src.getLinePtr(startY, (Pixel*)0);
		Pixel* dstLine1 = Scaler<Pixel>::linePtr(dst, y + 0);
		scale_2on3(srcLine, dstLine1, 426); // TODO
		if (y == (480 - 1)) break;
		Pixel* dstLine2 = Scaler<Pixel>::linePtr(dst, y + 1);
		copyLine(dstLine1, dstLine2, 640);
	}
}

template <class Pixel>
void Scaler2<Pixel>::scale384(FrameSource& src0, FrameSource& src1, SDL_Surface* dst,
                             unsigned startY, unsigned endY)
{
	for (unsigned y = startY; y < endY; ++y) {
		const Pixel* srcLine0 = src0.getLinePtr(y, (Pixel*)0);
		Pixel* dstLine0 = Scaler<Pixel>::linePtr(dst, 2 * y + 0);
		scale_2on3(srcLine0, dstLine0, 426); // TODO

		const Pixel* srcLine1 = src1.getLinePtr(y, (Pixel*)0);
		Pixel* dstLine1 = Scaler<Pixel>::linePtr(dst, 2 * y + 1);
		scale_2on3(srcLine1, dstLine1, 426); // TODO
	}
}

template <class Pixel>
void Scaler2<Pixel>::scale640(FrameSource& /*src*/, SDL_Surface* /*dst*/,
                             unsigned /*startY*/, unsigned /*endY*/, bool /*lower*/)
{
	// TODO
}

template <class Pixel>
void Scaler2<Pixel>::scale640(FrameSource& /*src0*/, FrameSource& /*src1*/, SDL_Surface* /*dst*/,
                             unsigned /*startY*/, unsigned /*endY*/)
{
	// TODO
}

template <class Pixel>
void Scaler2<Pixel>::scale768(FrameSource& src, SDL_Surface* dst,
                             unsigned startY, unsigned endY, bool lower)
{
	unsigned y1 = 2 * startY + (lower ? 1 : 0);
	unsigned y2 = 2 * endY   + (lower ? 1 : 0);
	for (unsigned y = y1; y < y2; y += 2, ++startY) {
		const Pixel* srcLine = src.getLinePtr(startY, (Pixel*)0);
		Pixel* dstLine1 = Scaler<Pixel>::linePtr(dst, y + 0);
		scale_4on3(srcLine, dstLine1, 853); // TODO
		if (y == (480 - 1)) break;
		Pixel* dstLine2 = Scaler<Pixel>::linePtr(dst, y + 1);
		copyLine(dstLine1, dstLine2, 640);
	}
}

template <class Pixel>
void Scaler2<Pixel>::scale768(FrameSource& src0, FrameSource& src1, SDL_Surface* dst,
                             unsigned startY, unsigned endY)
{
	for (unsigned y = startY; y < endY; ++y) {
		const Pixel* srcLine0 = src0.getLinePtr(y, (Pixel*)0);
		Pixel* dstLine0 = Scaler<Pixel>::linePtr(dst, 2 * y + 0);
		scale_4on3(srcLine0, dstLine0, 853); // TODO

		const Pixel* srcLine1 = src1.getLinePtr(y, (Pixel*)0);
		Pixel* dstLine1 = Scaler<Pixel>::linePtr(dst, 2 * y + 1);
		scale_4on3(srcLine1, dstLine1, 853); // TODO
	}
}

template <class Pixel>
void Scaler2<Pixel>::scale1024(FrameSource& src, SDL_Surface* dst,
                              unsigned startY, unsigned endY, bool lower)
{
	unsigned y1 = 2 * startY + (lower ? 1 : 0);
	unsigned y2 = 2 * endY   + (lower ? 1 : 0);
	for (unsigned y = y1; y < y2; y += 2, ++startY) {
		const Pixel* srcLine = src.getLinePtr(startY, (Pixel*)0);
		Pixel* dstLine1 = Scaler<Pixel>::linePtr(dst, y + 0);
		scale_2on1(srcLine, dstLine1, 1280);
		if (y == (480 - 1)) break;
		Pixel* dstLine2 = Scaler<Pixel>::linePtr(dst, y + 1);
		copyLine(dstLine1, dstLine2, 640);
	}
}

template <class Pixel>
void Scaler2<Pixel>::scale1024(FrameSource& src0, FrameSource& src1, SDL_Surface* dst,
                              unsigned startY, unsigned endY)
{
	for (unsigned y = startY; y < endY; ++y) {
		const Pixel* srcLine0 = src0.getLinePtr(y, (Pixel*)0);
		Pixel* dstLine0 = Scaler<Pixel>::linePtr(dst, 2 * y + 0);
		scale_2on1(srcLine0, dstLine0, 1280); // TODO

		const Pixel* srcLine1 = src1.getLinePtr(y, (Pixel*)0);
		Pixel* dstLine1 = Scaler<Pixel>::linePtr(dst, 2 * y + 1);
		scale_2on1(srcLine1, dstLine1, 1280); // TODO
	}
}


// Force template instantiation.
template class Scaler2<word>;
template class Scaler2<unsigned>;

} // namespace openmsx

