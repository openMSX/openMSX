// $Id$

#ifndef HQCOMMON_HH
#define HQCOMMON_HH

#include "FrameSource.hh"
#include "OutputSurface.hh"
#include "LineScalers.hh"
#include "build-info.hh"
#include <cassert>

namespace openmsx {

template <typename Pixel>
static inline unsigned readPixel(const Pixel* pIn)
{
	// TODO: Use surface info instead.
	Pixel p = *pIn;
	if (sizeof(Pixel) == 2) {
		return ((p & 0xF800) << 8) |
		       ((p & 0x07C0) << 5) | // drop lowest green bit
		       ((p & 0x001F) << 3);
	} else {
		return p & 0xF8F8F8;
	}
}

template <typename Pixel>
static inline void pset(Pixel* pOut, unsigned p)
{
	// TODO: Use surface info instead.
	if (sizeof(Pixel) == 2) {
		*pOut = ((p & 0xF80000) >> 8) |
			((p & 0x00FC00) >> 5) |
			((p & 0x0000F8) >> 3);
	} else {
		*pOut = (p & 0xF8F8F8) | ((p & 0xE0E0E0) >> 5);
	}
}

template <int w1, int w2>
static inline unsigned interpolate(unsigned c1, unsigned c2)
{
	enum { wsum = w1 + w2 };
	return (c1 * w1 + c2 * w2) / wsum;
}

template <int w1, int w2, int w3>
static inline unsigned interpolate(unsigned c1, unsigned c2, unsigned c3)
{
	enum { wsum = w1 + w2 + w3 };
	if (wsum <= 8) {
		// Because the lower 3 bits of each colour component (R,G,B) are
		// zeroed out, we can operate on a single integer as if it is
		// a vector.
		return (c1 * w1 + c2 * w2 + c3 * w3) / wsum;
	} else {
		return ((((c1 & 0x00FF00) * w1 +
		          (c2 & 0x00FF00) * w2 +
		          (c3 & 0x00FF00) * w3) & (0x00FF00 * wsum)) |
		        (((c1 & 0xFF00FF) * w1 +
		          (c2 & 0xFF00FF) * w2 +
		          (c3 & 0xFF00FF) * w3) & (0xFF00FF * wsum))) / wsum;
	}
}

static inline bool edge(unsigned c1, unsigned c2)
{
	if (c1 == c2) return false;

	unsigned r1 = c1 >> 16;
	unsigned g1 = (c1 >> 8) & 0xFF;
	unsigned b1 = c1 & 0xFF;
	unsigned y1 = r1 + g1 + b1;

	unsigned r2 = c2 >> 16;
	unsigned g2 = (c2 >> 8) & 0xFF;
	unsigned b2 = c2 & 0xFF;
	unsigned y2 = r2 + g2 + b2;

	int dy = y1 - y2;
	if (dy < -0xC0 || dy > 0xC0) return true;

	int du = r1 - r2 + b2 - b1;
	if (du < -0x1C || du > 0x1C) return true;

	int dv = 3 * (g1 - g2) - dy;
	if (dv < -0x30 || dv > 0x30) return true;

	return false;
}

struct EdgeHQ
{
	inline bool operator()(unsigned c1, unsigned c2) const
	{
		return edge(c1, c2);
	}
};

struct EdgeHQLite
{
	inline bool operator()(unsigned c1, unsigned c2) const
	{
		return c1 != c2;
	}
};

template <typename EdgeOp>
void calcEdgesGL(const unsigned* curr, const unsigned* next,
                 unsigned* edges2, EdgeOp edgeOp)
{
	typedef unsigned Pixel;
	if (OPENMSX_BIGENDIAN) {
		unsigned pattern = 0;
		Pixel c5 = curr[0];
		Pixel c8 = next[0];
		if (edgeOp(c5, c8)) pattern |= 0x1400;

		for (unsigned xx = 0; xx < (320 - 2) / 2; ++xx) {
			pattern = (pattern << (16 + 1)) & 0xA8000000;
			pattern |= ((edges2[xx] >> 5) & 0x01F001F0);

			if (edgeOp(c5, c8)) pattern |= 0x02000000;
			Pixel c6 = curr[2 * xx + 1];
			if (edgeOp(c6, c8)) pattern |= 0x10002000;
			if (edgeOp(c5, c6)) pattern |= 0x40008000;
			Pixel c9 = next[2 * xx + 1];
			if (edgeOp(c5, c9)) pattern |= 0x04000800;

			if (edgeOp(c6, c9)) pattern |= 0x0200;
			c5 = curr[2 * xx + 2];
			if (edgeOp(c5, c9)) pattern |= 0x1000;
			if (edgeOp(c6, c5)) pattern |= 0x4000;
			c8 = next[2 * xx + 2];
			if (edgeOp(c6, c8)) pattern |= 0x0400;

			edges2[xx + 320 / 2] = pattern;
		}

		pattern = (pattern << (16 + 1)) & 0xA8000000;
		pattern |= ((edges2[159] >> 5) & 0x01F001F0);

		if (edgeOp(c5, c8)) pattern |= 0x02000000;
		Pixel c6 = curr[319];
		if (edgeOp(c6, c8)) pattern |= 0x10002000;
		if (edgeOp(c5, c6)) pattern |= 0x40008000;
		Pixel c9 = next[319];
		if (edgeOp(c5, c9)) pattern |= 0x04000800;

		if (edgeOp(c6, c9)) pattern |= 0x1600;

		edges2[159 + 320 / 2] = pattern;
	} else {
		unsigned pattern = 0;
		Pixel c5 = curr[0];
		Pixel c8 = next[0];
		if (edgeOp(c5, c8)) pattern |= 0x14000000;

		for (unsigned xx = 0; xx < (320 - 2) / 2; ++xx) {
			pattern = (pattern >> (16 -1)) & 0xA800;
			pattern |= ((edges2[xx] >> 5) & 0x01F001F0);

			if (edgeOp(c5, c8)) pattern |= 0x0200;
			Pixel c6 = curr[2 * xx + 1];
			if (edgeOp(c6, c8)) pattern |= 0x20001000;
			if (edgeOp(c5, c6)) pattern |= 0x80004000;
			Pixel c9 = next[2 * xx + 1];
			if (edgeOp(c5, c9)) pattern |= 0x08000400;

			if (edgeOp(c6, c9)) pattern |= 0x02000000;
			c5 = curr[2 * xx + 2];
			if (edgeOp(c5, c9)) pattern |= 0x10000000;
			if (edgeOp(c6, c5)) pattern |= 0x40000000;
			c8 = next[2 * xx + 2];
			if (edgeOp(c6, c8)) pattern |= 0x04000000;

			edges2[xx + 320 / 2] = pattern;
		}

		pattern = (pattern >> (16 -1)) & 0xA800;
		pattern |= ((edges2[159] >> 5) & 0x01F001F0);

		if (edgeOp(c5, c8)) pattern |= 0x0200;
		Pixel c6 = curr[319];
		if (edgeOp(c6, c8)) pattern |= 0x20001000;
		if (edgeOp(c5, c6)) pattern |= 0x80004000;
		Pixel c9 = next[319];
		if (edgeOp(c5, c9)) pattern |= 0x08000400;

		if (edgeOp(c6, c9)) pattern |= 0x16000000;

		edges2[159 + 320 / 2] = pattern;
	}
}

template <typename Pixel>
static void calcInitialEdges(const Pixel* srcPrev, const Pixel* srcCurr,
                             unsigned srcWidth, unsigned* edgeBuf)
{
	unsigned x = 0;
	unsigned c1 = readPixel(&srcPrev[x]);
	unsigned c2 = readPixel(&srcCurr[x]);
	unsigned pattern = edge(c1, c2) ? ((1 << 6) | (1 << 7)) : 0;
	for (/* */; x < (srcWidth - 1); ++x) {
		pattern >>= 6;
		unsigned n1 = readPixel(&srcPrev[x + 1]);
		unsigned n2 = readPixel(&srcCurr[x + 1]);
		if (edge(c1, c2)) pattern |= (1 << 5);
		if (edge(c1, n2)) pattern |= (1 << 6);
		if (edge(c2, n1)) pattern |= (1 << 7);
		edgeBuf[x] = pattern;
		c1 = n1; c2 = n2;
	}
	pattern >>= 6;
	if (edge(c1, c2)) pattern |= (1 << 5) | (1 << 6) | (1 << 7);
	edgeBuf[x] = pattern;
}

template <typename Pixel, typename HQScale, typename PostScale>
static void doHQScale2(HQScale hqScale, PostScale postScale, FrameSource& src,
	unsigned srcStartY, unsigned /*srcEndY*/, unsigned srcWidth,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY,
	unsigned dstWidth)
{
	Pixel* const dummy = 0;
	int srcY = srcStartY;
	const Pixel* srcPrev = src.getLinePtr(srcY - 1, srcWidth, dummy);
	const Pixel* srcCurr = src.getLinePtr(srcY + 0, srcWidth, dummy);

	assert(srcWidth <= 1024);
	unsigned edgeBuf[1024];
	calcInitialEdges(srcPrev, srcCurr, srcWidth, edgeBuf);

	for (unsigned dstY = dstStartY; dstY < dstEndY; srcY += 1, dstY += 2) {
		Pixel buf0[2 * 1024], buf1[2 * 1024];
		const Pixel* srcNext = src.getLinePtr(srcY + 1, srcWidth, dummy);
		Pixel* dst0 = dst.getLinePtr(dstY + 0, dummy);
		Pixel* dst1 = dst.getLinePtr(dstY + 1, dummy);
		if (IsTagged<PostScale, Copy>::result) {
			hqScale(srcPrev, srcCurr, srcNext, dst0, dst1,
			      srcWidth, edgeBuf);
		} else {
			hqScale(srcPrev, srcCurr, srcNext, buf0, buf1,
			        srcWidth, edgeBuf);
			postScale(buf0, dst0, dstWidth);
			postScale(buf1, dst1, dstWidth);
		}
		srcPrev = srcCurr;
		srcCurr = srcNext;
	}
}

template <typename Pixel, typename HQScale, typename PostScale>
static void doHQScale3(HQScale hqScale, PostScale postScale, FrameSource& src,
	unsigned srcStartY, unsigned /*srcEndY*/, unsigned srcWidth,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY,
	unsigned dstWidth)
{
	Pixel* const dummy = 0;
	int srcY = srcStartY;
	const Pixel* srcPrev = src.getLinePtr(srcY - 1, srcWidth, dummy);
	const Pixel* srcCurr = src.getLinePtr(srcY + 0, srcWidth, dummy);

	assert(srcWidth <= 1024);
	unsigned edgeBuf[1024];
	calcInitialEdges(srcPrev, srcCurr, srcWidth, edgeBuf);

	for (unsigned dstY = dstStartY; dstY < dstEndY; srcY += 1, dstY += 3) {
		Pixel buf0[3 * 1024], buf1[3 * 1024], buf2[3 * 1024];
		const Pixel* srcNext = src.getLinePtr(srcY + 1, srcWidth, dummy);
		Pixel* dst0 = dst.getLinePtr(dstY + 0, dummy);
		Pixel* dst1 = dst.getLinePtr(dstY + 1, dummy);
		Pixel* dst2 = dst.getLinePtr(dstY + 2, dummy);
		if (IsTagged<PostScale, Copy>::result) {
			hqScale(srcPrev, srcCurr, srcNext, dst0, dst1, dst2,
			        srcWidth, edgeBuf);
		} else {
			hqScale(srcPrev, srcCurr, srcNext, buf0, buf1, buf2,
			        srcWidth, edgeBuf);
			postScale(buf0, dst0, dstWidth);
			postScale(buf1, dst1, dstWidth);
			postScale(buf2, dst2, dstWidth);
		}
		srcPrev = srcCurr;
		srcCurr = srcNext;
	}
}


} // namespace openmsx

#endif
