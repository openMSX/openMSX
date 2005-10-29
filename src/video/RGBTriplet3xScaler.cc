// $Id$

#include "RGBTriplet3xScaler.hh"
#include "LineScalers.hh"
#include "FrameSource.hh"
#include "RenderSettings.hh"

namespace openmsx {

template <class Pixel>
RGBTriplet3xScaler<Pixel>::RGBTriplet3xScaler(
		SDL_PixelFormat* format, const RenderSettings& renderSettings)
	: Scaler3<Pixel>(format)
	, pixelOps(format)
	, scanline(format)
	, settings(renderSettings)
{
}

template <class Pixel>
void RGBTriplet3xScaler<Pixel>::recalcBlur()
{
	c1 = settings.getBlurFactor();
	c2 = (3 * 256) - (2 * c1);
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
void RGBTriplet3xScaler<Pixel>::rgbify(
		const Pixel* in, Pixel* out, unsigned inwidth)
{
	unsigned r, g, b, rs, gs, bs;
	unsigned i = 0;

	calcSpil(pixelOps.red256  (in[i + 0]), r, rs);
	calcSpil(pixelOps.green256(in[i + 0]), g, gs);
	out[3 * i + 0] = pixelOps.combine256(r , gs, 0 );
	calcSpil(pixelOps.blue256 (in[i + 0]), b, bs);
	out[3 * i + 1] = pixelOps.combine256(rs, g , bs);
	calcSpil(pixelOps.red256  (in[i + 1]), r, rs);
	out[3 * i + 2] = pixelOps.combine256(rs, gs, b );

	for (++i; i < (inwidth - 1); ++i) {
		calcSpil(pixelOps.green256(in[i + 0]), g, gs);
		out[3 * i + 0] = pixelOps.combine256(r , gs, bs);
		calcSpil(pixelOps.blue256 (in[i + 0]), b, bs);
		out[3 * i + 1] = pixelOps.combine256(rs, g , bs);
		calcSpil(pixelOps.red256  (in[i + 1]), r, rs);
		out[3 * i + 2] = pixelOps.combine256(rs, gs, b );
	}

	calcSpil(pixelOps.green256(in[i + 0]), g, gs);
	out[3 * i + 0] = pixelOps.combine256(r , gs, bs);
	calcSpil(pixelOps.blue256 (in[i + 0]), b, bs);
	out[3 * i + 1] = pixelOps.combine256(rs, g , bs);
	out[3 * i + 2] = pixelOps.combine256(0 , gs, b );
}


// Note: the idea is that this method RGBifies a line that is first scaled
// to 256 (320) width. So, when calling this, keep this in mind and pass a
// scale functor that scales the input with to 256 (320).
template <typename Pixel>
template <typename ScaleOp>
void RGBTriplet3xScaler<Pixel>::doScale1(
	FrameSource& src, unsigned srcStartY, unsigned /*srcEndY*/,
	SDL_Surface* dst, unsigned dstStartY, unsigned dstEndY,
	ScaleOp scale)
{
	recalcBlur();

	int scanlineFactor = settings.getScanlineFactor();
	unsigned y = dstStartY;
	const Pixel* srcLine = src.getLinePtr(srcStartY++, (Pixel*)0);
	Pixel* prevDstLine0 = Scaler<Pixel>::linePtr(dst, y + 0);
	Pixel tmp[320];
	scale(srcLine, tmp, 320);
	rgbify(tmp, prevDstLine0, 320);

	Scale_1on1<Pixel> copy;
	Pixel* dstLine1     = Scaler<Pixel>::linePtr(dst, y + 1);
	copy(prevDstLine0, dstLine1, 960);

	for (/* */; (y + 4) < dstEndY; y += 3, srcStartY += 1) {
		const Pixel* srcLine = src.getLinePtr(srcStartY, (Pixel*)0);
		Pixel* dstLine0 = Scaler<Pixel>::linePtr(dst, y + 3);
		scale(srcLine, tmp, 320);
		rgbify(tmp, dstLine0, 320);

		Pixel* dstLine1 = Scaler<Pixel>::linePtr(dst, y + 4);
		copy(dstLine0, dstLine1, 960);

		Pixel* dstLine2 = Scaler<Pixel>::linePtr(dst, y + 2);
		scanline.draw(prevDstLine0, dstLine0, dstLine2,
		              scanlineFactor, 960);

		prevDstLine0 = dstLine0;
	}

	// When interlace is enabled, the bottom line can fall off the screen.
	if ((y + 2) < dstEndY) {
		Pixel* dstLine2 = Scaler<Pixel>::linePtr(dst, y + 2);
		scanline.draw(prevDstLine0, prevDstLine0, dstLine2,
		              scanlineFactor, 960);
	}
}

template <class Pixel>
void RGBTriplet3xScaler<Pixel>::scale192(
	FrameSource& src, unsigned srcStartY, unsigned srcEndY,
	SDL_Surface* dst, unsigned dstStartY, unsigned dstEndY)
{
	doScale1(src, srcStartY, srcEndY, dst, dstStartY, dstEndY,
	         Scale_2on3<Pixel>(pixelOps));
}

template <class Pixel>
void RGBTriplet3xScaler<Pixel>::scale256(
	FrameSource& src, unsigned srcStartY, unsigned srcEndY,
	SDL_Surface* dst, unsigned dstStartY, unsigned dstEndY)
{
	doScale1(src, srcStartY, srcEndY, dst, dstStartY, dstEndY,
	         Scale_1on1<Pixel>());
}

template <class Pixel>
void RGBTriplet3xScaler<Pixel>::scale384(
	FrameSource& src, unsigned srcStartY, unsigned srcEndY,
	SDL_Surface* dst, unsigned dstStartY, unsigned dstEndY)
{
	doScale1(src, srcStartY, srcEndY, dst, dstStartY, dstEndY,
	         Scale_4on3<Pixel>(pixelOps));
}

template <class Pixel>
void RGBTriplet3xScaler<Pixel>::scale512(
	FrameSource& src, unsigned srcStartY, unsigned srcEndY,
	SDL_Surface* dst, unsigned dstStartY, unsigned dstEndY)
{
	doScale1(src, srcStartY, srcEndY, dst, dstStartY, dstEndY,
	         Scale_2on1<Pixel>(pixelOps));
}

template <class Pixel>
void RGBTriplet3xScaler<Pixel>::scale768(
	FrameSource& src, unsigned srcStartY, unsigned srcEndY,
	SDL_Surface* dst, unsigned dstStartY, unsigned dstEndY)
{
	doScale1(src, srcStartY, srcEndY, dst, dstStartY, dstEndY,
	         Scale_8on3<Pixel>(pixelOps));
}

template <class Pixel>
void RGBTriplet3xScaler<Pixel>::scale1024(
	FrameSource& src, unsigned srcStartY, unsigned srcEndY,
	SDL_Surface* dst, unsigned dstStartY, unsigned dstEndY)
{
	doScale1(src, srcStartY, srcEndY, dst, dstStartY, dstEndY,
	         Scale_4on1<Pixel>(pixelOps));
}

template <typename Pixel>
static void fillLoop(const Pixel* in, Pixel* out)
{
	out[0] = in[0];
	out[1] = in[1];
	out[2] = in[2];
	for (unsigned x = 1; x < (960 - 3); x += 3) {
		out[x + 0] = in[3];
		out[x + 1] = in[4];
		out[x + 2] = in[5];
	}
	out[957] = in[6];
	out[958] = in[7];
	out[959] = in[8];
}

template <class Pixel>
void RGBTriplet3xScaler<Pixel>::scaleBlank(Pixel color, SDL_Surface* dst,
                               unsigned startY, unsigned endY)
{
	recalcBlur();

	Pixel inNormal   [3];
	Pixel outNormal  [3 * 3];
	inNormal[0] = inNormal[1] = inNormal[2] = color;
	rgbify(inNormal, outNormal, 3);

	Pixel outScanline[3 * 3];
	int scanlineFactor = settings.getScanlineFactor();
	for (int i = 0; i < (3 * 3); ++i) {
		outScanline[i] = scanline.darken(outNormal[i], scanlineFactor);
	}

	for (unsigned y = startY; y < endY; y += 3) {
		fillLoop(outNormal,   Scaler<Pixel>::linePtr(dst, y + 0));
		if ((y + 1) >= endY) break;
		fillLoop(outNormal,   Scaler<Pixel>::linePtr(dst, y + 1));
		if ((y + 2) >= endY) break;
		fillLoop(outScanline, Scaler<Pixel>::linePtr(dst, y + 2));
	}
}

// Force template instantiation.
template class RGBTriplet3xScaler<word>;
template class RGBTriplet3xScaler<unsigned>;

} // namespace openmsx
