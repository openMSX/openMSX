// $Id$

#include "BlurScaler.hh"
#include "RenderSettings.hh"
#include "IntegerSetting.hh"
#include "openmsx.hh"
#include <cassert>

namespace openmsx {

// Force template instantiation.
template class BlurScaler<word>;
template class BlurScaler<unsigned int>;

template <class Pixel>
BlurScaler<Pixel>::BlurScaler(SDL_PixelFormat* format_)
	: scanlineSetting(*RenderSettings::instance().getScanlineAlpha())
	, blurSetting(*RenderSettings::instance().getHorizontalBlur())
	, blender(Blender<Pixel>::createFromFormat(format_))
	, format(format_)
{
}

template <class Pixel>
BlurScaler<Pixel>::~BlurScaler()
{
}

template <>
word BlurScaler<word>::multiply(word pixel, unsigned factor)
{
	return ((((pixel & format->Rmask) * factor) >> 8) & format->Rmask) |
	       ((((pixel & format->Gmask) * factor) >> 8) & format->Gmask) |
	       ((((pixel & format->Bmask) * factor) >> 8) & format->Bmask);
}

template <>
unsigned BlurScaler<unsigned>::multiply(unsigned pixel, unsigned factor)
{
	return ((((pixel & 0x00FF00FF) * factor) & 0xFF00FF00) |
                (((pixel & 0x0000FF00) * factor) & 0x00FF0000)) >> 8;
}

template <class Pixel>
void BlurScaler<Pixel>::scaleBlank(Pixel colour, SDL_Surface* dst,
                                   int dstY, int endDstY)
{
	if (colour == 0) {
		// No need to draw scanlines if border is black.
		// This is a special case that occurs very often.
		Scaler<Pixel>::scaleBlank(colour, dst, dstY, endDstY);
	} else {
		int scanline = 256 - (scanlineSetting.getValue() * 256) / 100;
		Pixel scanlineColour = multiply(colour, scanline);
		// Note: SDL_FillRect is generally not allowed on locked surfaces.
		//       However, we're using a software surface, which doesn't
		//       have locking.
		// TODO: But it would be more generic to just write bytes.
		assert(!SDL_MUSTLOCK(dst));

		SDL_Rect rect;
		rect.x = 0;
		rect.w = dst->w;
		rect.h = 1;
		for (int y = dstY; y < endDstY; y += 2) {
			rect.y = y;
			// Note: return code ignored.
			SDL_FillRect(dst, &rect, colour);
			if (y + 1 == endDstY) break;
			rect.y = y + 1;
			// Note: return code ignored.
			SDL_FillRect(dst, &rect, scanlineColour);
		}
	}
}

template <class Pixel>
void BlurScaler<Pixel>::blur256(const Pixel* pIn, Pixel* pOut, unsigned alpha)
{
	assert(alpha <= 256);
	unsigned c1 = alpha / 2;
	unsigned c2 = 256 - c1;

	Pixel p0 = pIn[0];
	Pixel p1;
	unsigned f0 = multiply(p0, c1);
	unsigned f1 = f0;
	unsigned tmp;

	unsigned x;
	for (x = 0; x < (320 - 2); x += 2) {
		tmp = multiply(p0, c2);
		pOut[2 * x + 0] = f1 + tmp;

		p1 = pIn[x + 1];
		f1 = multiply(p1, c1);
		pOut[2 * x + 1] = f1 + tmp;

		tmp = multiply(p1, c2);
		pOut[2 * x + 2] = f0 + tmp;

		p0 = pIn[x + 2];
		f0 = multiply(p0, c1);
		pOut[2 * x + 3] = f0 + tmp;
	}

	tmp = multiply(p0, c2);
	pOut[2 * x + 0] = f1 + tmp;

	p1 = pIn[x + 1];
	f1 = multiply(p1, c1);
	pOut[2 * x + 1] = f1 + tmp;

	tmp = multiply(p1, c2);
	pOut[2 * x + 2] = f0 + tmp;

	pOut[2 * x + 3] = p1;
}

template <class Pixel>
void BlurScaler<Pixel>::blur512(const Pixel* pIn, Pixel* pOut, unsigned alpha)
{
	unsigned c1 = alpha / 2;
	unsigned c2 = 256 - alpha;

	Pixel p0 = pIn[0];
	Pixel p1;
	unsigned f0 = multiply(p0, c1);
	unsigned f1 = f0;

	unsigned x;
	for (x = 0; x < (640 - 2); x += 2) {
		p1 = pIn[x + 1];
		unsigned t0 = multiply(p1, c1);
		pOut[x] = f0 + multiply(p0, c2) + t0;
		f0 = t0;

		p0 = pIn[x + 2];
		unsigned t1 = multiply(p0, c1);
		pOut[x + 1] = f1 + multiply(p1, c2) + t1;
		f1 = t1;
	}

	p1 = pIn[x + 1];
	unsigned t0 = multiply(p1, c1);
	pOut[x] = f0 + multiply(p0, c2) + t0;

	pOut[x + 1] = f1 + multiply(p1, c2) + t0;
}

template <class Pixel>
void BlurScaler<Pixel>::average(
	const Pixel* src1, const Pixel* src2, Pixel* dst, unsigned alpha)
{
	for (unsigned x = 0; x < 640; ++x) {
		dst[x] = multiply(blender.blend(src1[x], src2[x]), alpha);
	}
}

template <class Pixel>
void BlurScaler<Pixel>::scale256(
	SDL_Surface* src, int srcY, int endSrcY,
	SDL_Surface* dst, int dstY )
{
	int blur = (blurSetting.getValue() * 256) / 100;
	int scanline = 256 - (scanlineSetting.getValue() * 256) / 100;

	Pixel* srcLine  = Scaler<Pixel>::linePtr(src, srcY++);
	Pixel* dstLine0 = Scaler<Pixel>::linePtr(dst, dstY++);
	Pixel* prevDstLine0 = dstLine0;
	blur256(srcLine, dstLine0, blur);

	while (srcY < endSrcY) {
		srcLine = Scaler<Pixel>::linePtr(src, srcY++);
		dstLine0 = Scaler<Pixel>::linePtr(dst, dstY + 1);
		blur256(srcLine, dstLine0, blur);
		
		Pixel* dstLine1 = Scaler<Pixel>::linePtr(dst, dstY);
		average(prevDstLine0, dstLine0, dstLine1, scanline);
		prevDstLine0 = dstLine0;

		dstY += 2;
	}

	Pixel* dstLine1 = Scaler<Pixel>::linePtr(dst, dstY);
	average(prevDstLine0, dstLine0, dstLine1, scanline);
}

template <class Pixel>
void BlurScaler<Pixel>::scale512(
	SDL_Surface* src, int srcY, int endSrcY,
	SDL_Surface* dst, int dstY )
{
	int blur = (blurSetting.getValue() * 256) / 100;
	int scanline = 256 - (scanlineSetting.getValue() * 256) / 100;

	Pixel* srcLine  = Scaler<Pixel>::linePtr(src, srcY++);
	Pixel* dstLine0 = Scaler<Pixel>::linePtr(dst, dstY++);
	Pixel* prevDstLine0 = dstLine0;
	blur512(srcLine, dstLine0, blur);

	while (srcY < endSrcY) {
		srcLine = Scaler<Pixel>::linePtr(src, srcY++);
		dstLine0 = Scaler<Pixel>::linePtr(dst, dstY + 1);
		blur512(srcLine, dstLine0, blur);
		
		Pixel* dstLine1 = Scaler<Pixel>::linePtr(dst, dstY);
		average(prevDstLine0, dstLine0, dstLine1, scanline);
		prevDstLine0 = dstLine0;

		dstY += 2;
	}

	Pixel* dstLine1 = Scaler<Pixel>::linePtr(dst, dstY);
	average(prevDstLine0, dstLine0, dstLine1, scanline);
}

} // namespace openmsx

