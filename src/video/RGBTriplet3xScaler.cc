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
	// TODO: adjust for 16bpp (spill earlier than 255)
	r = (c2 * x) >> 8;
	s = (c1 * x) >> 8;
	if (r > 255) {
		s += (r - 255) / 2;
		r = 255;
	}
}

template <class Pixel>
void RGBTriplet3xScaler<Pixel>::rgbify(const Pixel* in, Pixel* out, unsigned inwidth)
{
	unsigned r, g, b, rs, gs, bs;
	calcSpil(Scaler<Pixel>::pixelOps.red(in[0]), r, rs);
	calcSpil(Scaler<Pixel>::pixelOps.green(in[0]), g, gs);
	calcSpil(Scaler<Pixel>::pixelOps.blue(in[0]), b, bs);
	/* this is old code to process the edges correctly.
	 * We're skipping it now for efficiency and simplicity.
	 * Note that it is 32bpp only. It's left here in case we want to use
	 * it after all.
	out[0] = (r  << 16) + (gs << 8);
	out[1] = (rs << 16) + (g  << 8) + (bs << 0);
	calcSpil((in[1] >> 16) & 0xFF, r, rs);
	out[2] = (rs << 16) + (gs << 8) + (b  << 0);
	*/
	for (unsigned i = 0; i < inwidth; ++i) {
		calcSpil(Scaler<Pixel>::pixelOps.green(in[i + 0]), g, gs);
		out[3 * i + 0] = Scaler<Pixel>::pixelOps.combine(r, gs, bs);
		calcSpil(Scaler<Pixel>::pixelOps.blue(in[i + 0]), b, bs);
		out[3 * i + 1] = Scaler<Pixel>::pixelOps.combine(rs, g, bs);
		calcSpil(Scaler<Pixel>::pixelOps.red(in[(i + 1) % inwidth]), r, rs);
		out[3 * i + 2] = Scaler<Pixel>::pixelOps.combine(rs, gs, b);
	}
	/* see above
	calcSpil((in[319] >>  8) & 0xFF, g, gs);
	out[957] = (r  << 16) + (gs << 8) + (bs << 0);
	calcSpil((in[319] >>  0) & 0xFF, b, bs);
	out[958] = (rs << 16) + (g  << 8) + (bs << 0);
	out[959] =              (gs << 8) + (b  << 0);
	*/
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
	rgbify(srcLine, prevDstLine0, 320);

	Pixel* dstLine1     = Scaler<Pixel>::linePtr(dst, y + 1);
	copyLine(prevDstLine0, dstLine1, 960);

	for (/* */; y < ((int)dstEndY - 4); y += 3, srcStartY += 1) {
		const Pixel* srcLine = src.getLinePtr(srcStartY, (Pixel*)0);
		Pixel* dstLine0 = Scaler<Pixel>::linePtr(dst, y + 3);
		rgbify(srcLine, dstLine0, 320);

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
void RGBTriplet3xScaler<Pixel>::scale512(
	FrameSource& src, unsigned srcStartY, unsigned /*srcEndY*/,
	SDL_Surface* dst, unsigned dstStartY, unsigned dstEndY)
{
	int scanlineFactor = settings.getScanlineFactor();
	c1 = settings.getBlurFactor();
	c2 = (3 * 256) - (2 * c1);
	
	int y = dstStartY;

	const Pixel* srcLine = src.getLinePtr(srcStartY++, (Pixel*)0);
	Pixel* prevDstLine0 = Scaler<Pixel>::linePtr(dst, y + 0);
	Pixel tmp[320];
	halve(srcLine, tmp, 320);
	rgbify(tmp, prevDstLine0, 320);

	Pixel* dstLine1     = Scaler<Pixel>::linePtr(dst, y + 1);
	copyLine(prevDstLine0, dstLine1, 960);

	for (/* */; y < ((int)dstEndY - 4); y += 3, srcStartY += 1) {
		const Pixel* srcLine = src.getLinePtr(srcStartY, (Pixel*)0);
		Pixel* dstLine0 = Scaler<Pixel>::linePtr(dst, y + 3);
		halve(srcLine, tmp, 320);
		rgbify(tmp, dstLine0, 320);

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


// the following is mostly copied from scale256 (with some minor optimizations
// TODO: see if we can avoid code duplication
template <class Pixel>
void RGBTriplet3xScaler<Pixel>::scaleBlank(Pixel color, SDL_Surface* dst,
                               unsigned startY, unsigned endY)
{
	int scanlineFactor = settings.getScanlineFactor();
	c1 = settings.getBlurFactor();
	c2 = (3 * 256) - (2 * c1);

	Pixel outNormal[3];
	Pixel outScanline[3];
	
	rgbify(&color, outNormal, 1);
	Pixel scanlineColor = scanline.darken(color, scanlineFactor);
	rgbify(&scanlineColor, outScanline, 1);
	
	for (unsigned y = startY; y < endY; y += 3)
	{
		Pixel* dstLine = Scaler<Pixel>::linePtr(dst, y + 0);
		for (unsigned x = 0; x < 960; x += 3) {
			dstLine[x + 0] = outNormal[0];
			dstLine[x + 1] = outNormal[1];
			dstLine[x + 2] = outNormal[2];
		}
		if ((y + 1) >= endY) break;
		dstLine = Scaler<Pixel>::linePtr(dst, y + 1);
		for (unsigned x = 0; x < 960; x += 3) {
			dstLine[x + 0] = outNormal[0];
			dstLine[x + 1] = outNormal[1];
			dstLine[x + 2] = outNormal[2];
		} 
		if ((y + 2) >= endY) break;
		dstLine = Scaler<Pixel>::linePtr(dst, y + 2);
		for (unsigned x = 0; x < 960; x += 3) {
			dstLine[x + 0] = outScanline[0];
			dstLine[x + 1] = outScanline[1];
			dstLine[x + 2] = outScanline[2];
		}
	}
}

// Force template instantiation.
template class RGBTriplet3xScaler<word>;
template class RGBTriplet3xScaler<unsigned>;

} // namespace openmsx
