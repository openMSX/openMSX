#ifndef HQCOMMON_HH
#define HQCOMMON_HH

#include "endian.hh"
#include "narrow.hh"
#include "xrange.hh"
#include <cassert>
#include <cstdint>
#include <span>

namespace openmsx {

struct EdgeHQ
{
	[[nodiscard]] inline bool operator()(uint32_t c1, uint32_t c2) const
	{
		if (c1 == c2) return false;

		unsigned r1 = (c1 >>  0) & 0xFF;
		unsigned g1 = (c1 >>  8) & 0xFF;
		unsigned b1 = (c1 >> 16) & 0xFF;

		unsigned r2 = (c2 >>  0) & 0xFF;
		unsigned g2 = (c2 >>  8) & 0xFF;
		unsigned b2 = (c2 >> 16) & 0xFF;

		auto dr = narrow_cast<int>(r1 - r2);
		auto dg = narrow_cast<int>(g1 - g2);
		auto db = narrow_cast<int>(b1 - b2);

		int dy = dr + dg + db;
		if (dy < -0xC0 || dy > 0xC0) return true;

		if (int du = dr - db;
		    du < -0x1C || du > 0x1C) return true;

		if (int dv = 3 * dg - dy;
		    dv < -0x30 || dv > 0x30) return true;

		return false;
	}
};

struct EdgeHQLite
{
	[[nodiscard]] inline bool operator()(uint32_t c1, uint32_t c2) const
	{
		return c1 != c2;
	}
};

template<typename EdgeOp>
void calcEdgesGL(std::span<const uint32_t> curr, std::span<const uint32_t> next,
                 std::span<Endian::L32> edges2, EdgeOp edgeOp)
{
	// Consider a grid of 3x3 pixels, numbered like this:
	//    1 | 2 | 3
	//   ---A---B---
	//    4 | 5 | 6
	//   ---C---D---
	//    7 | 8 | 9
	// Then we calculate 12 'edges':
	// * 8 star-edges, from the central pixel '5' to the 8 neighboring pixels.
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

	auto size = curr.size();
	assert(next.size() == size);
	assert(2 * edges2.size() == size);

	using Pixel = uint32_t;

	uint32_t pattern = 0;
	Pixel c5 = curr[0];
	Pixel c8 = next[0];
	if (edgeOp(c5, c8)) pattern |= 0x1800'0000; // edges: 9,D (right pixel)

	auto size2 = size / 2;
	for (auto xx : xrange(size2 - 1)) {
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
	pattern |= (edges2[size2 - 1] << 3) & 0xC460'C460; // edges: C,8,D,7,9 -> 1,2,3,A,B (left and right)

	if (edgeOp(c5, c8)) pattern |= 0x0000'0080;  // edge: 8 (left)
	Pixel c6 = curr[size - 1];
	if (edgeOp(c6, c8)) pattern |= 0x0004'0800;  // edge: D (left), 7 (right)
	if (edgeOp(c5, c6)) pattern |= 0x0010'2000;  // edge: 6 (left), 4 (right)
	Pixel c9 = next[size - 1];
	if (edgeOp(c5, c9)) pattern |= 0x0008'1000;  // edge: 9 (left), C (right)

	if (edgeOp(c6, c9)) pattern |= 0x1880'0000;  // edges:      8,9,D (right)

	edges2[size2 - 1] = pattern;
}

} // namespace openmsx

#endif
