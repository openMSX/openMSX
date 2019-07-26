#ifndef HQCOMMON_HH
#define HQCOMMON_HH

#include "FrameSource.hh"
#include "ScalerOutput.hh"
#include "LineScalers.hh"
#include "PixelOperations.hh"
#include "endian.hh"
#include "vla.hh"
#include "xrange.hh"
#include "build-info.hh"
#include <algorithm>
#include <cassert>
#include <cstdint>

namespace openmsx {

template<typename Pixel>
[[nodiscard]] inline uint32_t readPixel(Pixel p)
{
	// TODO: Use surface info instead.
	if constexpr (sizeof(Pixel) == 2) {
		return ((p & 0xF800) << 8) |
		       ((p & 0x07C0) << 5) | // drop lowest green bit
		       ((p & 0x001F) << 3);
	} else {
		return p & 0xF8F8F8F8;
	}
}

template<typename Pixel>
inline Pixel writePixel(uint32_t p)
{
	// TODO: Use surface info instead.
	if constexpr (sizeof(Pixel) == 2) {
		return ((p & 0xF80000) >> 8) |
		       ((p & 0x00FC00) >> 5) |
		       ((p & 0x0000F8) >> 3);
	} else {
		return (p & 0xF8F8F8F8) | ((p & 0xE0E0E0E0) >> 5);
	}
}

class EdgeHQ
{
public:
	EdgeHQ(unsigned shiftR_, unsigned shiftG_, unsigned shiftB_)
		: shiftR(shiftR_), shiftG(shiftG_), shiftB(shiftB_)
	{
	}

	[[nodiscard]] inline bool operator()(uint32_t c1, uint32_t c2) const
	{
		if (c1 == c2) return false;

		unsigned r1 = (c1 >> shiftR) & 0xFF;
		unsigned g1 = (c1 >> shiftG) & 0xFF;
		unsigned b1 = (c1 >> shiftB) & 0xFF;

		unsigned r2 = (c2 >> shiftR) & 0xFF;
		unsigned g2 = (c2 >> shiftG) & 0xFF;
		unsigned b2 = (c2 >> shiftB) & 0xFF;

		int dr = r1 - r2;
		int dg = g1 - g2;
		int db = b1 - b2;

		int dy = dr + dg + db;
		if (dy < -0xC0 || dy > 0xC0) return true;

		int du = dr - db;
		if (du < -0x1C || du > 0x1C) return true;

		int dv = 3 * dg - dy;
		if (dv < -0x30 || dv > 0x30) return true;

		return false;
	}
private:
	const unsigned shiftR;
	const unsigned shiftG;
	const unsigned shiftB;
};

template<typename Pixel>
EdgeHQ createEdgeHQ(const PixelOperations<Pixel>& pixelOps)
{
	if constexpr (sizeof(Pixel) == 2) {
		return EdgeHQ(0, 8, 16);
	} else {
		return EdgeHQ(pixelOps.getRshift(),
		              pixelOps.getGshift(),
		              pixelOps.getBshift());
	}
}

struct EdgeHQLite
{
	[[nodiscard]] inline bool operator()(uint32_t c1, uint32_t c2) const
	{
		return c1 != c2;
	}
};

template<typename EdgeOp>
void calcEdgesGL(const uint32_t* __restrict curr, const uint32_t* __restrict next,
                 Endian::L32* __restrict edges2, EdgeOp edgeOp)
{
	// Consider a grid of 3x3 pixels, numbered like this:
	//    1 | 2 | 3
	//   ---A---B---
	//    4 | 5 | 6
	//   ---C---D---
	//    7 | 8 | 9
	// Then we calculate 12 'edges':
	// * 8 star-edges, from the central pixel '5' to the 8 neighbouring pixels.
	//   Let's call these edges 1, 2, 3, 4, 6, 7, 8, 9 (note: 5 is skipped).
	// * 4 cross-edges, between pixels (2,4), (2,6), (4,8), (6,8).
	//   Let's call these respectively A, B, C, D.
	// An edge between two pixels means the color of the two pixels is sufficiently distant.
	// * For the HQ scaler see 'EdgeHQ' for the definition of this distance function.
	// * The HQlite scaler uses a much simpler distance function.
	//
	// We store these 12 edges in a 16-bit value and order them like this:
	// (MSB (bit 15) on the left, LSB (bit 0) on the left, 'x' means bit is not used)
	//   || B 3 6 9 | D 2 x x || 8 1 A 4 | C 7 x x ||
	// This order has two important properties:
	// * The 12 bits are split in 2 groups of 6 bits and each group is MSB
	//   aligned within a byte. This allows to upload this data as a
	//   openGL texture and interpret each texel as a vec2 which
	//   represents a texture coordinate in another 64x64 texture.
	// * This order allows to calculate the edges incrementally:
	//   Suppose we already calculated the edges for the pixel immediate
	//   above and immediately to the left of the current pixel. Then the edges
	//        (1, 2, 3, A, B) can be calculated as:  (upper << 3) & 0xc460
	//    and (4, 7, C)       can be calculated as:  (left  >> 9) & 0x001C
	//   And only edges (6, 8, 9, D) must be newly calculated for this pixel.
	//   So only 4 new edges per pixel instead of all 12.
	//
	// This function takes as input:
	// * an in/out-array 'edges2':
	//   This contains the edge information for the upper row of pixels.
	//   And it gets update in-place to the edge information of the current
	//   row of pixels.
	// * 2 rows of input pixels: the middle and the lower pixel rows.
	// * An edge-function (to distinguish 'hq' from 'hqlite').

	using Pixel = uint32_t;

	uint32_t pattern = 0;
	Pixel c5 = curr[0];
	Pixel c8 = next[0];
	if (edgeOp(c5, c8)) pattern |= 0x1800'0000; // edges: 9,D (right pixel)

	for (auto xx : xrange((320 - 2) / 2)) {
		pattern = (pattern >> (16 + 9)) & 0x001C;   // edges: 6,D,9 -> 4,7,C        (left pixel)
		pattern |= (edges2[xx] << 3) & 0xC460'C460; // edges C,8,D,7,9 -> 1,2,3,A,B (left and right)

		if (edgeOp(c5, c8)) pattern |= 0x0000'0080; // edge: 8 (left)
		Pixel c6 = curr[2 * xx + 1];
		if (edgeOp(c6, c8)) pattern |= 0x0004'0800; // edge: D (left), 7 (right)
		if (edgeOp(c5, c6)) pattern |= 0x0010'2000; // edge: 6 (left), 4 (right)
		Pixel c9 = next[2 * xx + 1];
		if (edgeOp(c5, c9)) pattern |= 0x0008'1000; // edge: 9 (left), C (right)

		if (edgeOp(c6, c9)) pattern |= 0x0080'0000; // edge:           8 (right)
		c5 = curr[2 * xx + 2];
		if (edgeOp(c5, c9)) pattern |= 0x0800'0000; // edge:           D (right)
		if (edgeOp(c6, c5)) pattern |= 0x2000'0000; // edge:           6 (right)
		c8 = next[2 * xx + 2];
		if (edgeOp(c6, c8)) pattern |= 0x1000'0000; // edge:           9 (right)

		edges2[xx] = pattern;
	}

	pattern = (pattern >> (16 + 9)) & 0x001C;    // edges: 6,D,9 -> 4,7,C         (left pixel)
	pattern |= (edges2[159] << 3) & 0xC460'C460; // edges: C,8,D,7,9 -> 1,2,3,A,B (left and right)

	if (edgeOp(c5, c8)) pattern |= 0x0000'0080;  // edge: 8 (left)
	Pixel c6 = curr[319];
	if (edgeOp(c6, c8)) pattern |= 0x0004'0800;  // edge: D (left), 7 (right)
	if (edgeOp(c5, c6)) pattern |= 0x0010'2000;  // edge: 6 (left), 4 (right)
	Pixel c9 = next[319];
	if (edgeOp(c5, c9)) pattern |= 0x0008'1000;  // edge: 9 (left), C (right)

	if (edgeOp(c6, c9)) pattern |= 0x1880'0000;  // edges:      8,9,D (right)

	edges2[159] = pattern;
}

template<typename Pixel, typename EdgeOp>
void calcInitialEdges(
	const Pixel* __restrict srcPrev, const Pixel* __restrict srcCurr,
	unsigned srcWidth, unsigned* __restrict edgeBuf, EdgeOp edgeOp)
{
	unsigned x = 0;
	uint32_t c1 = readPixel(srcPrev[x]);
	uint32_t c2 = readPixel(srcCurr[x]);
	unsigned pattern = edgeOp(c1, c2) ? ((1 << 6) | (1 << 7)) : 0;
	for (/* */; x < (srcWidth - 1); ++x) {
		pattern >>= 6;
		uint32_t n1 = readPixel(srcPrev[x + 1]);
		uint32_t n2 = readPixel(srcCurr[x + 1]);
		if (edgeOp(c1, c2)) pattern |= (1 << 5);
		if (edgeOp(c1, n2)) pattern |= (1 << 6);
		if (edgeOp(c2, n1)) pattern |= (1 << 7);
		edgeBuf[x] = pattern;
		c1 = n1; c2 = n2;
	}
	pattern >>= 6;
	if (edgeOp(c1, c2)) pattern |= (1 << 5) | (1 << 6) | (1 << 7);
	edgeBuf[x] = pattern;
}

template<typename Pixel, typename HQScale, typename EdgeOp>
void doHQScale2(HQScale hqScale, EdgeOp edgeOp, PolyLineScaler<Pixel>& postScale,
	FrameSource& src, unsigned srcStartY, unsigned /*srcEndY*/, unsigned srcWidth,
	ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY, unsigned dstWidth)
{
	VLA(unsigned, edgeBuf, srcWidth);
	VLA_SSE_ALIGNED(Pixel, buf1_, srcWidth); auto* buf1 = buf1_;
	VLA_SSE_ALIGNED(Pixel, buf2_, srcWidth); auto* buf2 = buf2_;
	VLA_SSE_ALIGNED(Pixel, buf3_, srcWidth); auto* buf3 = buf3_;
	VLA_SSE_ALIGNED(Pixel, bufA, 2 * srcWidth);
	VLA_SSE_ALIGNED(Pixel, bufB, 2 * srcWidth);

	int srcY = srcStartY;
	auto* srcPrev = src.getLinePtr(srcY - 1, srcWidth, buf1);
	auto* srcCurr = src.getLinePtr(srcY + 0, srcWidth, buf2);

	calcInitialEdges(srcPrev, srcCurr, srcWidth, edgeBuf, edgeOp);

	bool isCopy = postScale.isCopy();
	for (unsigned dstY = dstStartY; dstY < dstEndY; srcY += 1, dstY += 2) {
		auto* srcNext = src.getLinePtr(srcY + 1, srcWidth, buf3);
		auto* dst0 = dst.acquireLine(dstY + 0);
		auto* dst1 = dst.acquireLine(dstY + 1);
		if (isCopy) {
			hqScale(srcPrev, srcCurr, srcNext, dst0, dst1,
			      srcWidth, edgeBuf, edgeOp);
		} else {
			hqScale(srcPrev, srcCurr, srcNext, bufA, bufB,
			        srcWidth, edgeBuf, edgeOp);
			postScale(bufA, dst0, dstWidth);
			postScale(bufB, dst1, dstWidth);
		}
		dst.releaseLine(dstY + 0, dst0);
		dst.releaseLine(dstY + 1, dst1);
		srcPrev = srcCurr;
		srcCurr = srcNext;
		std::swap(buf1, buf2);
		std::swap(buf2, buf3);
	}
}

template<typename Pixel, typename HQScale, typename EdgeOp>
void doHQScale3(HQScale hqScale, EdgeOp edgeOp, PolyLineScaler<Pixel>& postScale,
	FrameSource& src, unsigned srcStartY, unsigned /*srcEndY*/, unsigned srcWidth,
	ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY, unsigned dstWidth)
{
	VLA(unsigned, edgeBuf, srcWidth);
	VLA_SSE_ALIGNED(Pixel, buf1_, srcWidth); auto* buf1 = buf1_;
	VLA_SSE_ALIGNED(Pixel, buf2_, srcWidth); auto* buf2 = buf2_;
	VLA_SSE_ALIGNED(Pixel, buf3_, srcWidth); auto* buf3 = buf3_;
	VLA_SSE_ALIGNED(Pixel, bufA, 3 * srcWidth);
	VLA_SSE_ALIGNED(Pixel, bufB, 3 * srcWidth);
	VLA_SSE_ALIGNED(Pixel, bufC, 3 * srcWidth);

	int srcY = srcStartY;
	auto* srcPrev = src.getLinePtr(srcY - 1, srcWidth, buf1);
	auto* srcCurr = src.getLinePtr(srcY + 0, srcWidth, buf2);

	calcInitialEdges(srcPrev, srcCurr, srcWidth, edgeBuf, edgeOp);

	bool isCopy = postScale.isCopy();
	for (unsigned dstY = dstStartY; dstY < dstEndY; srcY += 1, dstY += 3) {
		auto* srcNext = src.getLinePtr(srcY + 1, srcWidth, buf3);
		auto* dst0 = dst.acquireLine(dstY + 0);
		auto* dst1 = dst.acquireLine(dstY + 1);
		auto* dst2 = dst.acquireLine(dstY + 2);
		if (isCopy) {
			hqScale(srcPrev, srcCurr, srcNext, dst0, dst1, dst2,
			        srcWidth, edgeBuf, edgeOp);
		} else {
			hqScale(srcPrev, srcCurr, srcNext, bufA, bufB, bufC,
			        srcWidth, edgeBuf, edgeOp);
			postScale(bufA, dst0, dstWidth);
			postScale(bufB, dst1, dstWidth);
			postScale(bufC, dst2, dstWidth);
		}
		dst.releaseLine(dstY + 0, dst0);
		dst.releaseLine(dstY + 1, dst1);
		dst.releaseLine(dstY + 2, dst2);
		srcPrev = srcCurr;
		srcCurr = srcNext;
		std::swap(buf1, buf2);
		std::swap(buf2, buf3);
	}
}

} // namespace openmsx

#endif
