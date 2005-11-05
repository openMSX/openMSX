// $Id$

#include "RGBTriplet3xScaler.hh"
#include "LineScalers.hh"
#include "FrameSource.hh"
#include "OutputSurface.hh"
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
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY,
	ScaleOp scale)
{
	recalcBlur();

	int scanlineFactor = settings.getScanlineFactor();
	unsigned y = dstStartY;
	Pixel* dummy = 0;
	const Pixel* srcLine = src.getLinePtr(srcStartY++, dummy);
	Pixel* prevDstLine0 = dst.getLinePtr(y + 0, dummy);
	Pixel tmp[320];
	if (IsTagged<ScaleOp, Copy>::result) {
		rgbify(srcLine, prevDstLine0, 320);
	} else {
		scale(srcLine, tmp, 320);
		rgbify(tmp, prevDstLine0, 320);
	}


	Scale_1on1<Pixel> copy;
	Pixel* dstLine1     = dst.getLinePtr(y + 1, dummy);
	copy(prevDstLine0, dstLine1, 960);

	for (/* */; (y + 4) < dstEndY; y += 3, srcStartY += 1) {
		const Pixel* srcLine = src.getLinePtr(srcStartY, dummy);
		Pixel* dstLine0 = dst.getLinePtr(y + 3, dummy);
		if (IsTagged<ScaleOp, Copy>::result) {
			rgbify(srcLine, dstLine0, 320);
		} else {
			scale(srcLine, tmp, 320);
			rgbify(tmp, dstLine0, 320);
		}

		Pixel* dstLine1 = dst.getLinePtr(y + 4, dummy);
		copy(dstLine0, dstLine1, 960);

		Pixel* dstLine2 = dst.getLinePtr(y + 2, dummy);
		scanline.draw(prevDstLine0, dstLine0, dstLine2,
		              scanlineFactor, 960);

		prevDstLine0 = dstLine0;
	}

	// When interlace is enabled, the bottom line can fall off the screen.
	if ((y + 2) < dstEndY) {
		Pixel* dstLine2 = dst.getLinePtr(y + 2, dummy);
		scanline.draw(prevDstLine0, prevDstLine0, dstLine2,
		              scanlineFactor, 960);
	}
}

template <class Pixel>
void RGBTriplet3xScaler<Pixel>::scale1x1to4x3(
	FrameSource& src, unsigned srcStartY, unsigned srcEndY,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	doScale1(src, srcStartY, srcEndY, dst, dstStartY, dstEndY,
	         Scale_2on3<Pixel>(pixelOps));
}

template <class Pixel>
void RGBTriplet3xScaler<Pixel>::scale1x1to3x3(
	FrameSource& src, unsigned srcStartY, unsigned srcEndY,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	doScale1(src, srcStartY, srcEndY, dst, dstStartY, dstEndY,
	         Scale_1on1<Pixel>());
}

template <class Pixel>
void RGBTriplet3xScaler<Pixel>::scale1x1to2x3(
	FrameSource& src, unsigned srcStartY, unsigned srcEndY,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	doScale1(src, srcStartY, srcEndY, dst, dstStartY, dstEndY,
	         Scale_4on3<Pixel>(pixelOps));
}

template <class Pixel>
void RGBTriplet3xScaler<Pixel>::scale2x1to3x3(
	FrameSource& src, unsigned srcStartY, unsigned srcEndY,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	doScale1(src, srcStartY, srcEndY, dst, dstStartY, dstEndY,
	         Scale_2on1<Pixel>(pixelOps));
}

template <class Pixel>
void RGBTriplet3xScaler<Pixel>::scale1x1to1x3(
	FrameSource& src, unsigned srcStartY, unsigned srcEndY,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	doScale1(src, srcStartY, srcEndY, dst, dstStartY, dstEndY,
	         Scale_8on3<Pixel>(pixelOps));
}

template <class Pixel>
void RGBTriplet3xScaler<Pixel>::scale4x1to3x3(
	FrameSource& src, unsigned srcStartY, unsigned srcEndY,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
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
void RGBTriplet3xScaler<Pixel>::scaleBlank(Pixel color, OutputSurface& dst,
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
		Pixel* dummy = 0;
		fillLoop(outNormal,   dst.getLinePtr(y + 0, dummy));
		if ((y + 1) >= endY) break;
		fillLoop(outNormal,   dst.getLinePtr(y + 1, dummy));
		if ((y + 2) >= endY) break;
		fillLoop(outScanline, dst.getLinePtr(y + 2, dummy));
	}
}

// Force template instantiation.
template class RGBTriplet3xScaler<word>;
template class RGBTriplet3xScaler<unsigned>;

} // namespace openmsx
