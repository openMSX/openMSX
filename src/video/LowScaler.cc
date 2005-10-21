// $Id$

#include "LowScaler.hh"
#include "FrameSource.hh"
#include "HostCPU.hh"
#include "openmsx.hh"
#include <cassert>

namespace openmsx {

template <typename Pixel>
LowScaler<Pixel>::LowScaler(SDL_PixelFormat* format)
	: Scaler<Pixel>(format)
{
}

template <typename Pixel>
void LowScaler<Pixel>::averageHalve(const Pixel* pIn0, const Pixel* pIn1, Pixel* pOut)
{
	// TODO MMX/SSE optimizations
	// pure C++ version
	for (int i = 0; i < 320; ++i) {
		Pixel tmp0 = blend(pIn0[2 * i + 0], pIn0[2 * i + 1]);
		Pixel tmp1 = blend(pIn1[2 * i + 0], pIn1[2 * i + 1]);
		pOut[i] = blend(tmp0, tmp1);
	}
}

template <typename Pixel>
void LowScaler<Pixel>::scaleBlank(Pixel color, SDL_Surface* dst,
                                  unsigned startY, unsigned endY, bool /*lower*/)
{
	for (unsigned y = startY; y < endY; ++y) {
		Pixel* dstLine = Scaler<Pixel>::linePtr(dst, y);
		Scaler<Pixel>::fillLine(dstLine, color, 320);
	}
}

template <typename Pixel>
void LowScaler<Pixel>::scale256(FrameSource& src, SDL_Surface* dst,
                                unsigned startY, unsigned endY, bool /*lower*/)
{
	for (unsigned y = startY; y < endY; ++y) {
		const Pixel* srcLine = src.getLinePtr(y, (Pixel*)0);
		Pixel* dstLine = Scaler<Pixel>::linePtr(dst, y);
		Scaler<Pixel>::copyLine(srcLine, dstLine, 320, false);
	}
}

template <typename Pixel>
void LowScaler<Pixel>::scale256(FrameSource& src0, FrameSource& src1, SDL_Surface* dst,
                                unsigned startY, unsigned endY)
{
	for (unsigned y = startY; y < endY; ++y) {
		const Pixel* srcLine0 = src0.getLinePtr(y, (Pixel*)0);
		const Pixel* srcLine1 = src1.getLinePtr(y, (Pixel*)0);
		Pixel* dstLine = Scaler<Pixel>::linePtr(dst, y);
		average(srcLine0, srcLine1, dstLine, 320);
	}
}

template <typename Pixel>
void LowScaler<Pixel>::scale512(FrameSource& src, SDL_Surface* dst,
                                unsigned startY, unsigned endY, bool /*lower*/)
{
	for (unsigned y = startY; y < endY; ++y) {
		const Pixel* srcLine = src.getLinePtr(y, (Pixel*)0);
		Pixel* dstLine = Scaler<Pixel>::linePtr(dst, y);
		halve(srcLine, dstLine, 320);
	}
}

template <typename Pixel>
void LowScaler<Pixel>::scale512(FrameSource& src0, FrameSource& src1, SDL_Surface* dst,
                                unsigned startY, unsigned endY)
{
	for (unsigned y = startY; y < endY; ++y) {
		const Pixel* srcLine0 = src0.getLinePtr(y, (Pixel*)0);
		const Pixel* srcLine1 = src1.getLinePtr(y, (Pixel*)0);
		Pixel* dstLine = Scaler<Pixel>::linePtr(dst, y);
		averageHalve(srcLine0, srcLine1, dstLine);
	}
}


template <class Pixel>
void LowScaler<Pixel>::scale192(FrameSource& src, SDL_Surface* dst,
                             unsigned startY, unsigned endY, bool /*lower*/)
{
	for (unsigned y = startY; y < endY; ++y) {
		const Pixel* srcLine = src.getLinePtr(y, (Pixel*)0);
		Pixel* dstLine = Scaler<Pixel>::linePtr(dst, y);
		scale_2on3(srcLine, dstLine, 213); // TODO
	}
}

template <class Pixel>
void LowScaler<Pixel>::scale192(FrameSource& src0, FrameSource& src1, SDL_Surface* dst,
                             unsigned startY, unsigned endY)
{
	for (unsigned y = startY; y < endY; ++y) {
		const Pixel* srcLine0 = src0.getLinePtr(y, (Pixel*)0);
		const Pixel* srcLine1 = src1.getLinePtr(y, (Pixel*)0);
		Pixel buf0[320], buf1[320];
		scale_2on3(srcLine0, buf0, 213); // TODO
		scale_2on3(srcLine1, buf1, 213); // TODO
		Pixel* dstLine = Scaler<Pixel>::linePtr(dst, y);
		average(buf0, buf1, dstLine, 320);
	}
}

template <class Pixel>
void LowScaler<Pixel>::scale384(FrameSource& src, SDL_Surface* dst,
                             unsigned startY, unsigned endY, bool /*lower*/)
{
	for (unsigned y = startY; y < endY; ++y) {
		const Pixel* srcLine = src.getLinePtr(y, (Pixel*)0);
		Pixel* dstLine = Scaler<Pixel>::linePtr(dst, y);
		scale_4on3(srcLine, dstLine, 426); // TODO
	}
}

template <class Pixel>
void LowScaler<Pixel>::scale384(FrameSource& src0, FrameSource& src1, SDL_Surface* dst,
                             unsigned startY, unsigned endY)
{
	for (unsigned y = startY; y < endY; ++y) {
		const Pixel* srcLine0 = src0.getLinePtr(y, (Pixel*)0);
		const Pixel* srcLine1 = src1.getLinePtr(y, (Pixel*)0);
		Pixel buf0[320], buf1[320];
		scale_4on3(srcLine0, buf0, 426); // TODO
		scale_4on3(srcLine1, buf1, 426); // TODO
		Pixel* dstLine = Scaler<Pixel>::linePtr(dst, y);
		average(buf0, buf1, dstLine, 320);
	}
}

template <class Pixel>
void LowScaler<Pixel>::scale640(FrameSource& /*src*/, SDL_Surface* /*dst*/,
                             unsigned /*startY*/, unsigned /*endY*/, bool /*lower*/)
{
	// TODO
}

template <class Pixel>
void LowScaler<Pixel>::scale640(FrameSource& /*src0*/, FrameSource& /*src1*/, SDL_Surface* /*dst*/,
                             unsigned /*startY*/, unsigned /*endY*/)
{
	// TODO
}

template <class Pixel>
void LowScaler<Pixel>::scale768(FrameSource& src, SDL_Surface* dst,
                             unsigned startY, unsigned endY, bool /*lower*/)
{
	for (unsigned y = startY; y < endY; ++y) {
		const Pixel* srcLine = src.getLinePtr(y, (Pixel*)0);
		Pixel* dstLine = Scaler<Pixel>::linePtr(dst, y);
		scale_8on3(srcLine, dstLine, 853); // TODO
	}
}

template <class Pixel>
void LowScaler<Pixel>::scale768(FrameSource& src0, FrameSource& src1, SDL_Surface* dst,
                             unsigned startY, unsigned endY)
{
	for (unsigned y = startY; y < endY; ++y) {
		const Pixel* srcLine0 = src0.getLinePtr(y, (Pixel*)0);
		const Pixel* srcLine1 = src1.getLinePtr(y, (Pixel*)0);
		Pixel buf0[320], buf1[320];
		scale_8on3(srcLine0, buf0, 853); // TODO
		scale_8on3(srcLine1, buf1, 853); // TODO
		Pixel* dstLine = Scaler<Pixel>::linePtr(dst, y);
		average(buf0, buf1, dstLine, 320);
	}
}

template <class Pixel>
void LowScaler<Pixel>::scale1024(FrameSource& src, SDL_Surface* dst,
                              unsigned startY, unsigned endY, bool /*lower*/)
{
	for (unsigned y = startY; y < endY; ++y) {
		const Pixel* srcLine = src.getLinePtr(y, (Pixel*)0);
		Pixel* dstLine = Scaler<Pixel>::linePtr(dst, y);
		scale_4on1(srcLine, dstLine, 1280);
	}
}

template <class Pixel>
void LowScaler<Pixel>::scale1024(FrameSource& src0, FrameSource& src1, SDL_Surface* dst,
                              unsigned startY, unsigned endY)
{
	for (unsigned y = startY; y < endY; ++y) {
		const Pixel* srcLine0 = src0.getLinePtr(y, (Pixel*)0);
		const Pixel* srcLine1 = src1.getLinePtr(y, (Pixel*)0);
		Pixel buf0[320], buf1[320];
		scale_4on1(srcLine0, buf0, 1280);
		scale_4on1(srcLine1, buf1, 1280);
		Pixel* dstLine = Scaler<Pixel>::linePtr(dst, y);
		average(buf0, buf1, dstLine, 320);
	}
}


// Force template instantiation.
template class LowScaler<word>;
template class LowScaler<unsigned int>;

} // namespace openmsx
