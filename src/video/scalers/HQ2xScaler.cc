/*
Original code: Copyright (C) 2003 MaxSt ( maxst@hiend3d.com )
openMSX adaptation by Maarten ter Huurne

License: LGPL

Visit the HiEnd3D site for info:
  http://www.hiend3d.com/hq2x.html
*/

#include "HQ2xScaler.hh"
#include "HQCommon.hh"
#include "LineScalers.hh"
#include "unreachable.hh"
#include "xrange.hh"
#include "build-info.hh"
#include <cstdint>

namespace openmsx {

template<std::unsigned_integral Pixel> struct HQ_1x1on2x2
{
	void operator()(const Pixel* in0, const Pixel* in1, const Pixel* in2,
	                Pixel* out0, Pixel* out1,
	                std::span<uint16_t> edgeBuf, EdgeHQ edgeOp) __restrict;
};

template<std::unsigned_integral Pixel> struct HQ_1x1on1x2
{
	void operator()(const Pixel* in0, const Pixel* in1, const Pixel* in2,
	                Pixel* out0, Pixel* out1,
	                std::span<uint16_t> edgeBuf, EdgeHQ edgeOp) __restrict;
};

template<std::unsigned_integral Pixel>
void HQ_1x1on2x2<Pixel>::operator()(
	const Pixel* __restrict in0, const Pixel* __restrict in1,
	const Pixel* __restrict in2,
	Pixel* __restrict out0, Pixel* __restrict out1,
	std::span<uint16_t> edgeBuf,
	EdgeHQ edgeOp) __restrict
{
	unsigned c2 = readPixel(in0[0]); unsigned c3 = c2;
	unsigned c5 = readPixel(in1[0]); unsigned c6 = c5;
	unsigned c8 = readPixel(in2[0]); unsigned c9 = c8;

	unsigned pattern = 0;
	if (edgeOp(c5, c8)) pattern |= 3 <<  6;
	if (edgeOp(c5, c2)) pattern |= 3 <<  9;

	unsigned srcWidth = edgeBuf.size();
	for (auto x : xrange(srcWidth)) {
		unsigned c1 = c2;
		unsigned c4 = c5;
		unsigned c7 = c8;
		c2 = c3;
		c5 = c6;
		c8 = c9;
		if (x != srcWidth - 1) {
			c3 = readPixel(in0[x + 1]);
			c6 = readPixel(in1[x + 1]);
			c9 = readPixel(in2[x + 1]);
		}

		pattern = (pattern >> 6) & 0x001F; // left overlap
		// overlaps with left
		//if (edgeOp(c8, c4)) pattern |= 1 <<  0; // B - l: c5-c9 6
		//if (edgeOp(c5, c7)) pattern |= 1 <<  1; // B - l: c6-c8 7
		//if (edgeOp(c5, c4)) pattern |= 1 <<  2; //     l: c5-c6 8
		// overlaps with top and left
		//if (edgeOp(c5, c1)) pattern |= 1 <<  3; //     l: c2-c6 9,  t: c4-c8 0
		//if (edgeOp(c4, c2)) pattern |= 1 <<  4; //     l: c5-c3 10, t: c5-c7 1
		// non-overlapping pixels
		if (edgeOp(c5, c8)) pattern |= 1 <<  5; // B
		if (edgeOp(c5, c9)) pattern |= 1 <<  6; // BR
		if (edgeOp(c6, c8)) pattern |= 1 <<  7; // BR
		if (edgeOp(c5, c6)) pattern |= 1 <<  8; // R
		// overlaps with top
		//if (edgeOp(c2, c6)) pattern |= 1 <<  9; // R - t: c5-c9 6
		//if (edgeOp(c5, c3)) pattern |= 1 << 10; // R - t: c6-c8 7
		//if (edgeOp(c5, c2)) pattern |= 1 << 11; //     t: c5-c8 5
		pattern |= ((edgeBuf[x] &  (1 << 5)            ) << 6) |
		           ((edgeBuf[x] & ((1 << 6) | (1 << 7))) << 3);
		edgeBuf[x] = pattern;

		unsigned pixel0, pixel1, pixel2, pixel3;

#include "HQ2xScaler-1x1to2x2.nn"

		out0[2 * x + 0] = writePixel<Pixel>(pixel0);
		out0[2 * x + 1] = writePixel<Pixel>(pixel1);
		out1[2 * x + 0] = writePixel<Pixel>(pixel2);
		out1[2 * x + 1] = writePixel<Pixel>(pixel3);
	}
}

template<std::unsigned_integral Pixel>
void HQ_1x1on1x2<Pixel>::operator()(
	const Pixel* __restrict in0, const Pixel* __restrict in1,
	const Pixel* __restrict in2,
	Pixel* __restrict out0, Pixel* __restrict out1,
	std::span<uint16_t> edgeBuf,
	EdgeHQ edgeOp) __restrict
{
	//  +---+---+---+
	//  | 1 | 2 | 3 |
	//  +---+---+---+
	//  | 4 | 5 | 6 |
	//  +---+---+---+
	//  | 7 | 8 | 9 |
	//  +---+---+---+

	unsigned c2 = readPixel(in0[0]); unsigned c3 = c2;
	unsigned c5 = readPixel(in1[0]); unsigned c6 = c5;
	unsigned c8 = readPixel(in2[0]); unsigned c9 = c8;

	unsigned pattern = 0;
	if (edgeOp(c5, c8)) pattern |= 3 <<  6;
	if (edgeOp(c5, c2)) pattern |= 3 <<  9;

	unsigned srcWidth = edgeBuf.size();
	for (auto x : xrange(srcWidth)) {
		unsigned c1 = c2;
		unsigned c4 = c5;
		unsigned c7 = c8;
		c2 = c3;
		c5 = c6;
		c8 = c9;
		if (x != srcWidth - 1) {
			c3 = readPixel(in0[x + 1]);
			c6 = readPixel(in1[x + 1]);
			c9 = readPixel(in2[x + 1]);
		}

		pattern = (pattern >> 6) & 0x001F; // left overlap
		// overlaps with left
		//if (edgeOp(c8, c4)) pattern |= 1 <<  0; // B - l: c5-c9 6
		//if (edgeOp(c5, c7)) pattern |= 1 <<  1; // B - l: c6-c8 7
		//if (edgeOp(c5, c4)) pattern |= 1 <<  2; //     l: c5-c6 8
		// overlaps with top and left
		//if (edgeOp(c5, c1)) pattern |= 1 <<  3; //     l: c2-c6 9,  t: c4-c8 0
		//if (edgeOp(c4, c2)) pattern |= 1 <<  4; //     l: c5-c3 10, t: c5-c7 1
		// non-overlapping pixels
		if (edgeOp(c5, c8)) pattern |= 1 <<  5; // B
		if (edgeOp(c5, c9)) pattern |= 1 <<  6; // BR
		if (edgeOp(c6, c8)) pattern |= 1 <<  7; // BR
		if (edgeOp(c5, c6)) pattern |= 1 <<  8; // R
		// overlaps with top
		//if (edgeOp(c2, c6)) pattern |= 1 <<  9; // R - t: c5-c9 6
		//if (edgeOp(c5, c3)) pattern |= 1 << 10; // R - t: c6-c8 7
		//if (edgeOp(c5, c2)) pattern |= 1 << 11; //     t: c5-c8 5
		pattern |= ((edgeBuf[x] &  (1 << 5)            ) << 6) |
		           ((edgeBuf[x] & ((1 << 6) | (1 << 7))) << 3);
		edgeBuf[x] = pattern;

		unsigned pixel0, pixel1;

#include "HQ2xScaler-1x1to1x2.nn"

		out0[x] = writePixel<Pixel>(pixel0);
		out1[x] = writePixel<Pixel>(pixel1);
	}
}



template<std::unsigned_integral Pixel>
HQ2xScaler<Pixel>::HQ2xScaler(const PixelOperations<Pixel>& pixelOps_)
	: Scaler2<Pixel>(pixelOps_)
	, pixelOps(pixelOps_)
{
}

template<std::unsigned_integral Pixel>
void HQ2xScaler<Pixel>::scale1x1to3x2(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_2on3<Pixel>> postScale(pixelOps);
	EdgeHQ edgeOp = createEdgeHQ(pixelOps);
	doHQScale2<Pixel>(HQ_1x1on2x2<Pixel>(), edgeOp, postScale,
	                  src, srcStartY, srcEndY, srcWidth,
	                  dst, dstStartY, dstEndY, srcWidth * 3);
}

template<std::unsigned_integral Pixel>
void HQ2xScaler<Pixel>::scale1x1to2x2(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_1on1<Pixel>> postScale;
	EdgeHQ edgeOp = createEdgeHQ(pixelOps);
	doHQScale2<Pixel>(HQ_1x1on2x2<Pixel>(), edgeOp, postScale,
	                  src, srcStartY, srcEndY, srcWidth,
	                  dst, dstStartY, dstEndY, srcWidth * 2);
}

template<std::unsigned_integral Pixel>
void HQ2xScaler<Pixel>::scale2x1to3x2(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_4on3<Pixel>> postScale(pixelOps);
	EdgeHQ edgeOp = createEdgeHQ(pixelOps);
	doHQScale2<Pixel>(HQ_1x1on2x2<Pixel>(), edgeOp, postScale,
	                  src, srcStartY, srcEndY, srcWidth,
	                  dst, dstStartY, dstEndY, (srcWidth * 3) / 2);
}

template<std::unsigned_integral Pixel>
void HQ2xScaler<Pixel>::scale1x1to1x2(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_1on1<Pixel>> postScale;
	EdgeHQ edgeOp = createEdgeHQ(pixelOps);
	doHQScale2<Pixel>(HQ_1x1on1x2<Pixel>(), edgeOp, postScale,
	                  src, srcStartY, srcEndY, srcWidth,
	                  dst, dstStartY, dstEndY, srcWidth);
}

template<std::unsigned_integral Pixel>
void HQ2xScaler<Pixel>::scale4x1to3x2(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_4on3<Pixel>> postScale(pixelOps);
	EdgeHQ edgeOp = createEdgeHQ(pixelOps);
	doHQScale2<Pixel>(HQ_1x1on1x2<Pixel>(), edgeOp, postScale,
	                  src, srcStartY, srcEndY, srcWidth,
	                  dst, dstStartY, dstEndY, (srcWidth * 3) / 4);
}

template<std::unsigned_integral Pixel>
void HQ2xScaler<Pixel>::scale2x1to1x2(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	PolyScale<Pixel, Scale_2on1<Pixel>> postScale(pixelOps);
	EdgeHQ edgeOp = createEdgeHQ(pixelOps);
	doHQScale2<Pixel>(HQ_1x1on1x2<Pixel>(), edgeOp, postScale,
	                  src, srcStartY, srcEndY, srcWidth,
	                  dst, dstStartY, dstEndY, srcWidth / 2);
}

// Force template instantiation.
#if HAVE_16BPP
template class HQ2xScaler<uint16_t>;
#endif
#if HAVE_32BPP
template class HQ2xScaler<uint32_t>;
#endif

} // namespace openmsx
