// $Id$

#include "RGBTriplet3xScaler.hh"
#include "FrameSource.hh"
#include "RenderSettings.hh"
#include <algorithm>
#include <cassert>

using std::min;

namespace openmsx {

template <class Pixel>
RGBTriplet3xScaler<Pixel>::RGBTriplet3xScaler(
		SDL_PixelFormat* format, const RenderSettings& renderSettings)
	: Scaler3<Pixel>(format)
	, scanline(format)
	, settings(renderSettings)
{
}

template <class Pixel>
void RGBTriplet3xScaler<Pixel>::calcSpil(unsigned x, unsigned& r, unsigned& s)
{
	r = (c2 * x) >> 8;
	s = (c1 * x) >> 8;
	if (r > 255) {
		s += (r - 255) / 2;
		r = 255;
	}
}

template <class Pixel>
void RGBTriplet3xScaler<Pixel>::rgbify(const Pixel* in, Pixel* out)
{
	unsigned r, g, b, rs, gs, bs;
	calcSpil((in[0] >> 16) & 0xFF, r, rs);
	calcSpil((in[0] >>  8) & 0xFF, g, gs);
	calcSpil((in[0] >>  0) & 0xFF, b, bs);
	out[0] = (r  << 16) + (gs << 8);
	out[1] = (rs << 16) + (g  << 8) + (bs << 0);
	calcSpil((in[1] >> 16) & 0xFF, r, rs);
	out[2] = (rs << 16) + (gs << 8) + (b  << 0);
	for (int i = 1; i < 319; ++i) {
		calcSpil((in[i + 0] >>  8) & 0xFF, g, gs);
		out[3 * i + 0] = (r  << 16) + (gs << 8) + (bs << 0);
		calcSpil((in[i + 0] >>  0) & 0xFF, b, bs);
		out[3 * i + 1] = (rs << 16) + (g  << 8) + (bs << 0);
		calcSpil((in[i + 1] >> 16) & 0xFF, r, rs);
		out[3 * i + 2] = (rs << 16) + (gs << 8) + (b  << 0);
	}
	calcSpil((in[319] >>  8) & 0xFF, g, gs);
	out[957] = (r  << 16) + (gs << 8) + (bs << 0);
	calcSpil((in[319] >>  0) & 0xFF, b, bs);
	out[958] = (rs << 16) + (g  << 8) + (bs << 0);
	out[959] =              (gs << 8) + (b  << 0);
}

template <class Pixel>
void RGBTriplet3xScaler<Pixel>::scale256(
	FrameSource& src, unsigned srcStartY, unsigned /*srcEndY*/,
	SDL_Surface* dst, unsigned dstStartY, unsigned dstEndY)
{
	int scanlineFactor = settings.getScanlineFactor();
	c1 = settings.getBlurFactor();
	c2 = (3 * 256) - (2 * c1);
	
	int y = dstStartY;

	const Pixel* srcLine = src.getLinePtr(srcStartY++, (Pixel*)0);
	Pixel* prevDstLine0 = Scaler<Pixel>::linePtr(dst, y + 0);
	rgbify(srcLine, prevDstLine0);

	Pixel* dstLine1     = Scaler<Pixel>::linePtr(dst, y + 1);
	copyLine(prevDstLine0, dstLine1, 960);

	for (/* */; y < ((int)dstEndY - 4); y += 3, srcStartY += 1) {
		const Pixel* srcLine = src.getLinePtr(srcStartY, (Pixel*)0);
		Pixel* dstLine0 = Scaler<Pixel>::linePtr(dst, y + 3);
		rgbify(srcLine, dstLine0);

		Pixel* dstLine1 = Scaler<Pixel>::linePtr(dst, y + 4);
		copyLine(dstLine0, dstLine1, 960);

		Pixel* dstLine2 = Scaler<Pixel>::linePtr(dst, y + 2);
		scanline.draw(prevDstLine0, dstLine0, dstLine2,
		              scanlineFactor, 960);

		prevDstLine0 = dstLine0;
	}

	// When interlace is enabled, the bottom line can fall off the screen.
	if ((y + 2) < (int)dstEndY) {
		Pixel* dstLine2 = Scaler<Pixel>::linePtr(dst, y + 2);
		scanline.draw(prevDstLine0, prevDstLine0, dstLine2,
		              scanlineFactor, 960);
	}
}

template <class Pixel>
/*
void RGBTriplet3xScaler<Pixel>::scale512(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		SDL_Surface* dst, unsigned dstStartY, unsigned dstEndY)
		*/
void RGBTriplet3xScaler<Pixel>::scale512(FrameSource& src, SDL_Surface* dst,
                                 unsigned srcStartY, unsigned srcEndY, bool lower)
{
	unsigned dstStartY = 3 * srcStartY + (lower ? 1 : 0);
        unsigned dstEndY = 3 * srcEndY   + (lower ? 1 : 0);
	int scanlineFactor = settings.getScanlineFactor();
	c1 = settings.getBlurFactor();
	c2 = (3 * 256) - (2 * c1);
	
	int y = dstStartY;

	const Pixel* srcLine = src.getLinePtr(srcStartY++, (Pixel*)0);
	Pixel* prevDstLine0 = Scaler<Pixel>::linePtr(dst, y + 0);
	Pixel tmp[320];
	halve(srcLine, tmp, 320);
	rgbify(tmp, prevDstLine0);

	Pixel* dstLine1     = Scaler<Pixel>::linePtr(dst, y + 1);
	copyLine(prevDstLine0, dstLine1, 960);

	for (/* */; y < ((int)dstEndY - 4); y += 3, srcStartY += 1) {
		const Pixel* srcLine = src.getLinePtr(srcStartY, (Pixel*)0);
		Pixel* dstLine0 = Scaler<Pixel>::linePtr(dst, y + 3);
		halve(srcLine, tmp, 320);
		rgbify(tmp, dstLine0);

		Pixel* dstLine1 = Scaler<Pixel>::linePtr(dst, y + 4);
		copyLine(dstLine0, dstLine1, 960);

		Pixel* dstLine2 = Scaler<Pixel>::linePtr(dst, y + 2);
		scanline.draw(prevDstLine0, dstLine0, dstLine2,
		              scanlineFactor, 960);

		prevDstLine0 = dstLine0;
	}

	// When interlace is enabled, the bottom line can fall off the screen.
	if ((y + 2) < (int)dstEndY) {
		Pixel* dstLine2 = Scaler<Pixel>::linePtr(dst, y + 2);
		scanline.draw(prevDstLine0, prevDstLine0, dstLine2,
		              scanlineFactor, 960);
	}

	/*
        for (unsigned y = startY; y < endY; ++y) {
                const Pixel* srcLine0 = src0.getLinePtr(y, (Pixel*)0);
                Pixel* dstLine0 = Scaler<Pixel>::linePtr(dst, 3 * y + 0);
		Pixel* tmp[320];
                halve(srcLine0, tmp, 320);
		rgbify(tmp, dstLine0);

                Pixel* dstLine1 = Scaler<Pixel>::linePtr(dst, 3 * y + 1);
		copyLine(dstLine0, dstLine1, 960);
                
		Pixel* dstLine2 = Scaler<Pixel>::linePtr(dst, 3 * y + 2);
		copyLine(dstLine0, dstLine2, 960);
        }
	*/
}


// the following is mostly copied from scale256 (with some minor optimizations
// TODO: see if we can avoid code duplication
template <class Pixel>
void RGBTriplet3xScaler<Pixel>::scaleBlank(Pixel color, SDL_Surface* dst,
                               unsigned startY, unsigned endY)
{
	//int scanlineFactor = settings.getScanlineFactor();
	c1 = settings.getBlurFactor();
	c2 = (3 * 256) - (2 * c1);

	for (unsigned y = startY; y < endY; y += 3) {
		Pixel* dstLine0 = Scaler<Pixel>::linePtr(dst, y + 0);

		unsigned r, g, b, rs, gs, bs;
		calcSpil((color >> 16) & 0xFF, r, rs);
		calcSpil((color >>  8) & 0xFF, g, gs);
		calcSpil((color >>  0) & 0xFF, b, bs);
		dstLine0[0] = (r  << 16) + (gs << 8);
		dstLine0[1] = (rs << 16) + (g  << 8) + (bs << 0);
		calcSpil((color >> 16) & 0xFF, r, rs);
		dstLine0[2] = (rs << 16) + (gs << 8) + (b  << 0);

		// calculate the first non-edge pixel
		calcSpil((color >>  8) & 0xFF, g, gs);
		dstLine0[3 * 1 + 0] = (r  << 16) + (gs << 8) + (bs << 0);
		calcSpil((color >>  0) & 0xFF, b, bs);
		dstLine0[3 * 1 + 1] = (rs << 16) + (g  << 8) + (bs << 0);
		calcSpil((color >> 16) & 0xFF, r, rs);
		dstLine0[3 * 1 + 2] = (rs << 16) + (gs << 8) + (b  << 0);
		// copy this for the whole line
		for (int i = 2; i < 319; ++i) {
			dstLine0[3 * i + 0] = dstLine0[3 * 1 + 0];
			dstLine0[3 * i + 1] = dstLine0[3 * 1 + 1];
			dstLine0[3 * i + 2] = dstLine0[3 * 1 + 2];
		}
		calcSpil((color >>  8) & 0xFF, g, gs);
		dstLine0[957] = (r  << 16) + (gs << 8) + (bs << 0);
		calcSpil((color >>  0) & 0xFF, b, bs);
		dstLine0[958] = (rs << 16) + (g  << 8) + (bs << 0);
		dstLine0[959] =              (gs << 8) + (b  << 0);

		Pixel* dstLine1 = Scaler<Pixel>::linePtr(dst, y + 1);
		copyLine(dstLine0, dstLine1, 960);

		Pixel* dstLine2 = Scaler<Pixel>::linePtr(dst, y + 2);
		// emulate a 75% scanline for the 3rd row
		for (int i = 0; i < 960; ++i) {
			dstLine2[i] = ((dstLine0[i] >> 1) & 0x7F7F7F) + ((dstLine0[i] >>2) & 0x3F3F3F);
		}
	}
}

// Force template instantiation.
template class RGBTriplet3xScaler<word>;
template class RGBTriplet3xScaler<unsigned>;

} // namespace openmsx
