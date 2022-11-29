/*
Original code: Copyright (C) 2003 MaxSt ( maxst@hiend3d.com )
openMSX adaptation by Wouter Vermaelen

License: LGPL

Visit the HiEnd3D site for info:
  http://www.hiend3d.com/hq2x.html
*/

#include "HQ3xLiteScaler.hh"
#include "HQCommon.hh"
#include "LineScalers.hh"
#include "unreachable.hh"
#include "xrange.hh"
#include "build-info.hh"
#include <cstdint>

namespace openmsx {

template<std::unsigned_integral Pixel> struct HQLite_1x1on3x3
{
	void operator()(std::span<const Pixel> in0, std::span<const Pixel> in1, std::span<const Pixel> in2,
	                std::span<Pixel> out0, std::span<Pixel> out1, std::span<Pixel> out2,
	                std::span<uint16_t> edgeBuf, EdgeHQLite edgeOp);
};

template<std::unsigned_integral Pixel>
void HQLite_1x1on3x3<Pixel>::operator()(
	std::span<const Pixel> in0, std::span<const Pixel> in1, std::span<const Pixel> in2,
	std::span<Pixel> out0, std::span<Pixel> out1, std::span<Pixel> out2,
	std::span<uint16_t> edgeBuf,
	EdgeHQLite /*edgeOp*/)
{
	auto srcWidth = edgeBuf.size();
	assert(in0.size() == srcWidth);
	assert(in1.size() == srcWidth);
	assert(in2.size() == srcWidth);
	assert(out0.size() == 3 * srcWidth);
	assert(out1.size() == 3 * srcWidth);
	assert(out2.size() == 3 * srcWidth);

	auto c2 = readPixel(in0[0]);
	auto c5 = readPixel(in1[0]); auto c6 = c5;
	auto c8 = readPixel(in2[0]); auto c9 = c8;

	unsigned pattern = 0;
	if (c5 != c8) pattern |= 3 <<  6;
	if (c5 != c2) pattern |= 3 <<  9;

	for (auto x : xrange(srcWidth)) {
		auto c4 = c5;
		c5 = c6;
		c8 = c9;
		if (x != srcWidth - 1) {
			c6 = readPixel(in1[x + 1]);
			c9 = readPixel(in2[x + 1]);
		}

		pattern = (pattern >> 6) & 0x001F; // left overlap
		// overlaps with left
		//if (c8 != c4) pattern |= 1 <<  0; // B - l: c5-c9 6
		//if (c5 != c7) pattern |= 1 <<  1; // B - l: c6-c8 7
		//if (c5 != c4) pattern |= 1 <<  2; //     l: c5-c6 8
		// overlaps with top and left
		//if (c5 != c1) pattern |= 1 <<  3; //     l: c2-c6 9,  t: c4-c8 0
		//if (c4 != c2) pattern |= 1 <<  4; //     l: c5-c3 10, t: c5-c7 1
		// non-overlapping pixels
		if (c5 != c8) pattern |= 1 <<  5; // B
		if (c5 != c9) pattern |= 1 <<  6; // BR
		if (c6 != c8) pattern |= 1 <<  7; // BR
		if (c5 != c6) pattern |= 1 <<  8; // R
		// overlaps with top
		//if (c2 != c6) pattern |= 1 <<  9; // R - t: c5-c9 6
		//if (c5 != c3) pattern |= 1 << 10; // R - t: c6-c8 7
		//if (c5 != c2) pattern |= 1 << 11; //     t: c5-c8 5
		pattern |= ((edgeBuf[x] &  (1 << 5)            ) << 6) |
		           ((edgeBuf[x] & ((1 << 6) | (1 << 7))) << 3);
		edgeBuf[x] = narrow_cast<uint16_t>(pattern);

		unsigned pixel0, pixel1, pixel2, pixel3, pixel4,
		         pixel5, pixel6, pixel7, pixel8;

#include "HQ3xLiteScaler-1x1to3x3.nn"

		out0[3 * x + 0] = writePixel<Pixel>(pixel0);
		out0[3 * x + 1] = writePixel<Pixel>(pixel1);
		out0[3 * x + 2] = writePixel<Pixel>(pixel2);
		out1[3 * x + 0] = writePixel<Pixel>(pixel3);
		out1[3 * x + 1] = writePixel<Pixel>(pixel4);
		out1[3 * x + 2] = writePixel<Pixel>(pixel5);
		out2[3 * x + 0] = writePixel<Pixel>(pixel6);
		out2[3 * x + 1] = writePixel<Pixel>(pixel7);
		out2[3 * x + 2] = writePixel<Pixel>(pixel8);
	}
}


template<std::unsigned_integral Pixel>
HQ3xLiteScaler<Pixel>::HQ3xLiteScaler(const PixelOperations<Pixel>& pixelOps_)
	: Scaler3<Pixel>(pixelOps_)
	, pixelOps(pixelOps_)
{
}

template<std::unsigned_integral Pixel>
void HQ3xLiteScaler<Pixel>::scale2x1to9x3(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_2on3<Pixel>> postScale(pixelOps);
	EdgeHQLite edgeOp;
	doHQScale3<Pixel>(HQLite_1x1on3x3<Pixel>(), edgeOp, postScale,
	                  src, srcStartY, srcEndY, srcWidth,
	                  dst, dstStartY, dstEndY);
}

template<std::unsigned_integral Pixel>
void HQ3xLiteScaler<Pixel>::scale1x1to3x3(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_1on1<Pixel>> postScale;
	EdgeHQLite edgeOp;
	doHQScale3<Pixel>(HQLite_1x1on3x3<Pixel>(), edgeOp, postScale,
	                  src, srcStartY, srcEndY, srcWidth,
	                  dst, dstStartY, dstEndY);
}

template<std::unsigned_integral Pixel>
void HQ3xLiteScaler<Pixel>::scale4x1to9x3(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_4on3<Pixel>> postScale(pixelOps);
	EdgeHQLite edgeOp;
	doHQScale3<Pixel>(HQLite_1x1on3x3<Pixel>(), edgeOp, postScale,
	                  src, srcStartY, srcEndY, srcWidth,
	                  dst, dstStartY, dstEndY);
}

template<std::unsigned_integral Pixel>
void HQ3xLiteScaler<Pixel>::scale2x1to3x3(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_2on1<Pixel>> postScale(pixelOps);
	EdgeHQLite edgeOp;
	doHQScale3<Pixel>(HQLite_1x1on3x3<Pixel>(), edgeOp, postScale,
	                  src, srcStartY, srcEndY, srcWidth,
	                  dst, dstStartY, dstEndY);
}

template<std::unsigned_integral Pixel>
void HQ3xLiteScaler<Pixel>::scale8x1to9x3(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_8on3<Pixel>> postScale(pixelOps);
	EdgeHQLite edgeOp;
	doHQScale3<Pixel>(HQLite_1x1on3x3<Pixel>(), edgeOp, postScale,
	                  src, srcStartY, srcEndY, srcWidth,
	                  dst, dstStartY, dstEndY);
}

template<std::unsigned_integral Pixel>
void HQ3xLiteScaler<Pixel>::scale4x1to3x3(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_4on1<Pixel>> postScale(pixelOps);
	EdgeHQLite edgeOp;
	doHQScale3<Pixel>(HQLite_1x1on3x3<Pixel>(), edgeOp, postScale,
	                  src, srcStartY, srcEndY, srcWidth,
	                  dst, dstStartY, dstEndY);
}

// Force template instantiation.
#if HAVE_16BPP
template class HQ3xLiteScaler<uint16_t>;
#endif
#if HAVE_32BPP
template class HQ3xLiteScaler<uint32_t>;
#endif

} // namespace openmsx
