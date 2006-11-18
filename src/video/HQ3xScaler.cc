// $Id$

/*
Original code: Copyright (C) 2003 MaxSt ( maxst@hiend3d.com )
openMSX adaptation by Wouter Vermaelen

License: LGPL

Visit the HiEnd3D site for info:
  http://www.hiend3d.com/hq2x.html
*/

#include "HQ3xScaler.hh"
#include "HQCommon.hh"
#include "LineScalers.hh"
#include "openmsx.hh"

namespace openmsx {

template <typename Pixel> struct HQ_1x1on3x3
{
	typedef EdgeHQ EdgeOp;

	void operator()(const Pixel* in0, const Pixel* in1, const Pixel* in2,
	                Pixel* out0, Pixel* out1, Pixel* out2,
	                unsigned srcWidth, unsigned* edgeBuf);
};

template <typename Pixel>
void HQ_1x1on3x3<Pixel>::operator()(
	const Pixel* in0, const Pixel* in1, const Pixel* in2,
	Pixel* out0, Pixel* out1, Pixel* out2, unsigned srcWidth,
	unsigned* edgeBuf)
{
	unsigned c1, c2, c3, c4, c5, c6, c7, c8, c9;
	c2 = c3 = readPixel(in0[0]);
	c5 = c6 = readPixel(in1[0]);
	c8 = c9 = readPixel(in2[0]);

	unsigned pattern = 0;
	if (edge(c5, c8)) pattern |= 3 <<  6;
	if (edge(c5, c2)) pattern |= 3 <<  9;

	for (unsigned x = 0; x < srcWidth; ++x) {
		c1 = c2; c4 = c5; c7 = c8;
		c2 = c3; c5 = c6; c8 = c9;
		if (x != srcWidth - 1) {
			c3 = readPixel(in0[x + 1]);
			c6 = readPixel(in1[x + 1]);
			c9 = readPixel(in2[x + 1]);
		}

		pattern = (pattern >> 6) & 0x001F; // left overlap
		// overlaps with left
		//if (edge(c8, c4)) pattern |= 1 <<  0; // B - l: c5-c9 6
		//if (edge(c5, c7)) pattern |= 1 <<  1; // B - l: c6-c8 7
		//if (edge(c5, c4)) pattern |= 1 <<  2; //     l: c5-c6 8
		// overlaps with top and left
		//if (edge(c5, c1)) pattern |= 1 <<  3; //     l: c2-c6 9,  t: c4-c8 0
		//if (edge(c4, c2)) pattern |= 1 <<  4; //     l: c5-c3 10, t: c5-c7 1
		// non-overlapping pixels
		if (edge(c5, c8)) pattern |= 1 <<  5; // B
		if (edge(c5, c9)) pattern |= 1 <<  6; // BR
		if (edge(c6, c8)) pattern |= 1 <<  7; // BR
		if (edge(c5, c6)) pattern |= 1 <<  8; // R
		// overlaps with top
		//if (edge(c2, c6)) pattern |= 1 <<  9; // R - t: c5-c9 6
		//if (edge(c5, c3)) pattern |= 1 << 10; // R - t: c6-c8 7
		//if (edge(c5, c2)) pattern |= 1 << 11; //     t: c5-c8 5
		pattern |= ((edgeBuf[x] &  (1 << 5)            ) << 6) |
		           ((edgeBuf[x] & ((1 << 6) | (1 << 7))) << 3);
		edgeBuf[x] = pattern;

		unsigned pixel1, pixel2, pixel3, pixel4,
		         pixel6, pixel7, pixel8, pixel9;

#include "HQ3xScaler-1x1to3x3.nn"

		out0[3 * x + 0] = writePixel<Pixel>(pixel1);
		out0[3 * x + 1] = writePixel<Pixel>(pixel2);
		out0[3 * x + 2] = writePixel<Pixel>(pixel3);
		out1[3 * x + 0] = writePixel<Pixel>(pixel4);
		out1[3 * x + 1] = writePixel<Pixel>(c5);
		out1[3 * x + 2] = writePixel<Pixel>(pixel6);
		out2[3 * x + 0] = writePixel<Pixel>(pixel7);
		out2[3 * x + 1] = writePixel<Pixel>(pixel8);
		out2[3 * x + 2] = writePixel<Pixel>(pixel9);
	}
}



template <class Pixel>
HQ3xScaler<Pixel>::HQ3xScaler(const PixelOperations<Pixel>& pixelOps_)
	: Scaler3<Pixel>(pixelOps_)
	, pixelOps(pixelOps_)
{
}

template <class Pixel>
void HQ3xScaler<Pixel>::scale2x1to9x3(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	doHQScale3<Pixel>(HQ_1x1on3x3<Pixel>(), Scale_2on3<Pixel>(pixelOps),
	                  src, srcStartY, srcEndY, srcWidth,
	                  dst, dstStartY, dstEndY, (srcWidth * 9) / 2);
}

template <class Pixel>
void HQ3xScaler<Pixel>::scale1x1to3x3(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	doHQScale3<Pixel>(HQ_1x1on3x3<Pixel>(), Scale_1on1<Pixel>(),
	                  src, srcStartY, srcEndY, srcWidth,
	                  dst, dstStartY, dstEndY, srcWidth * 3);
}

template <class Pixel>
void HQ3xScaler<Pixel>::scale4x1to9x3(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	doHQScale3<Pixel>(HQ_1x1on3x3<Pixel>(), Scale_4on3<Pixel>(pixelOps),
	                  src, srcStartY, srcEndY, srcWidth,
	                  dst, dstStartY, dstEndY, (srcWidth * 9) / 4);
}

template <class Pixel>
void HQ3xScaler<Pixel>::scale2x1to3x3(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	doHQScale3<Pixel>(HQ_1x1on3x3<Pixel>(), Scale_2on1<Pixel>(pixelOps),
	                  src, srcStartY, srcEndY, srcWidth,
	                  dst, dstStartY, dstEndY, (srcWidth * 3) / 2);
}

template <class Pixel>
void HQ3xScaler<Pixel>::scale8x1to9x3(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	doHQScale3<Pixel>(HQ_1x1on3x3<Pixel>(), Scale_8on3<Pixel>(pixelOps),
	                  src, srcStartY, srcEndY, srcWidth,
	                  dst, dstStartY, dstEndY, (srcWidth * 9) / 8);
}

template <class Pixel>
void HQ3xScaler<Pixel>::scale4x1to3x3(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	doHQScale3<Pixel>(HQ_1x1on3x3<Pixel>(), Scale_4on1<Pixel>(pixelOps),
	                  src, srcStartY, srcEndY, srcWidth,
	                  dst, dstStartY, dstEndY, (srcWidth * 3) / 4);
}

// Force template instantiation.
template class HQ3xScaler<word>;
template class HQ3xScaler<unsigned>;

} // namespace openmsx
