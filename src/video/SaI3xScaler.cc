// $Id$

// 2xSaI is Copyright (c) 1999-2001 by Derek Liauw Kie Fa.
//   http://elektron.its.tudelft.nl/~dalikifa/
// 2xSaI is free under GPL.
//
// Modified for use in openMSX by Maarten ter Huurne.

#include "SaI3xScaler.hh"
#include "FrameSource.hh"
#include "OutputSurface.hh"
#include "openmsx.hh"
#include <cassert>

using std::min;
using std::max;

namespace openmsx {

template <typename Pixel>
SaI3xScaler<Pixel>::SaI3xScaler(SDL_PixelFormat* format)
	: Scaler3<Pixel>(format)
	, pixelOps(format)
{
}

template <typename Pixel>
inline Pixel SaI3xScaler<Pixel>::blend(Pixel p1, Pixel p2)
{
	return pixelOps.template blend<1, 1>(p1, p2);
}

static const unsigned redblueMask = 0xF81F;
static const unsigned greenMask = 0x7E0;

template <typename Pixel>
static Pixel bilinear(unsigned a, unsigned b, unsigned x);

template <>
static word bilinear<word>(unsigned a, unsigned b, unsigned x)
{
	if (a == b) return a;

	const unsigned areaB = x >> 11; //reduce 16 bit fraction to 5 bits
	const unsigned areaA = 0x20 - areaB;

	a = (a & redblueMask) | ((a & greenMask) << 16);
	b = (b & redblueMask) | ((b & greenMask) << 16);
	const unsigned result = ((areaA * a) + (areaB * b)) >> 5;
	return (result & redblueMask) | ((result >> 16) & greenMask);
}

template <>
static unsigned bilinear<unsigned>(unsigned a, unsigned b, unsigned x)
{
	if (a == b) return a;

	const unsigned areaB = x >> 8; //reduce 16 bit fraction to 8 bits
	const unsigned areaA = 0x100 - areaB;

	const unsigned result0 =
		(a & 0x00FF00FF) * areaA + (b & 0x00FF00FF) * areaB;
	const unsigned result1 =
		(a & 0x0000FF00) * areaA + (b & 0x0000FF00) * areaB;
	return ((result0 & 0xFF00FF00) | (result1 & 0x00FF0000)) >> 8;
}

template <typename Pixel>
static Pixel bilinear4(
	unsigned a, unsigned b, unsigned c, unsigned d, unsigned x, unsigned y );

template <>
static word bilinear4<word>(
	unsigned a, unsigned b, unsigned c, unsigned d, unsigned x, unsigned y
) {
	x >>= 11;
	y >>= 11;
	const unsigned xy = (x * y) >> 5;

	const unsigned areaA = 0x20 + xy - x - y;
	const unsigned areaB = x - xy;
	const unsigned areaC = y - xy;
	const unsigned areaD = xy;

	a = (a & redblueMask) | ((a & greenMask) << 16);
	b = (b & redblueMask) | ((b & greenMask) << 16);
	c = (c & redblueMask) | ((c & greenMask) << 16);
	d = (d & redblueMask) | ((d & greenMask) << 16);
	unsigned result = (
		(areaA * a) + (areaB * b) + (areaC * c) + (areaD * d)
		) >> 5;
	return (result & redblueMask) | ((result >> 16) & greenMask);
}

template <>
static unsigned bilinear4<unsigned>(
	unsigned a, unsigned b, unsigned c, unsigned d, unsigned x, unsigned y
) {
	x >>= 8;
	y >>= 8;
	const unsigned xy = (x * y) >> 8;

	const unsigned areaA = 0x100 + xy - x - y;
	const unsigned areaB = x - xy;
	const unsigned areaC = y - xy;
	const unsigned areaD = xy;

	const unsigned result0 =
		(a & 0x00FF00FF) * areaA + (b & 0x00FF00FF) * areaB +
		(c & 0x00FF00FF) * areaC + (d & 0x00FF00FF) * areaD;
	const unsigned result1 =
		(a & 0x0000FF00) * areaA + (b & 0x0000FF00) * areaB +
		(c & 0x0000FF00) * areaC + (d & 0x0000FF00) * areaD;
	return ((result0 & 0xFF00FF00) | (result1 & 0x00FF0000)) >> 8;
}

template <typename Pixel>
class Blender
{
public:
	template <unsigned x>
	inline static Pixel blend(unsigned a, unsigned b);

	template <unsigned x, unsigned y>
	inline static Pixel blend(unsigned a, unsigned b, unsigned c, unsigned d);

private:
	static const unsigned fpbits = sizeof(Pixel) == 2 ? 5 : 8;
};

template <typename Pixel>
template <unsigned x>
inline Pixel Blender<Pixel>::blend(unsigned a, unsigned b)
{
	if (a == b) return a;

	const unsigned areaB = x >> (16 - fpbits);
	const unsigned areaA = (1 << fpbits) - areaB;

	if (sizeof(Pixel) == 2) {
		a = (a & redblueMask) | ((a & greenMask) << 16);
		b = (b & redblueMask) | ((b & greenMask) << 16);
		const unsigned result = ((areaA * a) + (areaB * b)) >> 5;
		return (result & redblueMask) | ((result >> 16) & greenMask);
	} else {
		const unsigned result0 =
			(a & 0x00FF00FF) * areaA + (b & 0x00FF00FF) * areaB;
		const unsigned result1 =
			(a & 0x0000FF00) * areaA + (b & 0x0000FF00) * areaB;
		return ((result0 & 0xFF00FF00) | (result1 & 0x00FF0000)) >> 8;
	}
}


template <typename Pixel>
template <unsigned wx, unsigned wy>
inline Pixel Blender<Pixel>::blend(
	unsigned a, unsigned b, unsigned c, unsigned d
) {
	const unsigned x = wx >> (16 - fpbits);
	const unsigned y = wy >> (16 - fpbits);
	const unsigned xy = (x * y) >> fpbits;

	const unsigned areaA = (1 << fpbits) + xy - x - y;
	const unsigned areaB = x - xy;
	const unsigned areaC = y - xy;
	const unsigned areaD = xy;

	if (sizeof(Pixel) == 2) {
		a = (a & redblueMask) | ((a & greenMask) << 16);
		b = (b & redblueMask) | ((b & greenMask) << 16);
		c = (c & redblueMask) | ((c & greenMask) << 16);
		d = (d & redblueMask) | ((d & greenMask) << 16);
		unsigned result = (
			(areaA * a) + (areaB * b) + (areaC * c) + (areaD * d)
			) >> 5;
		return (result & redblueMask) | ((result >> 16) & greenMask);
	} else {
		const unsigned result0 =
			(a & 0x00FF00FF) * areaA + (b & 0x00FF00FF) * areaB +
			(c & 0x00FF00FF) * areaC + (d & 0x00FF00FF) * areaD;
		const unsigned result1 =
			(a & 0x0000FF00) * areaA + (b & 0x0000FF00) * areaB +
			(c & 0x0000FF00) * areaC + (d & 0x0000FF00) * areaD;
		return ((result0 & 0xFF00FF00) | (result1 & 0x00FF0000)) >> 8;
	}
}

template <unsigned i>
class PixelStripRepeater
{
public:
	template <typename Pixel>
	inline static void fill(Pixel* &dp, unsigned sa) {
		*dp++ = sa;
		PixelStripRepeater<i - 1>::template fill<Pixel>(dp, sa);
	}

	template <unsigned NX, unsigned y, typename Pixel>
	inline static void blendBackslash(
		Pixel* &dp,
		unsigned sa, unsigned sb, unsigned sc, unsigned sd,
		unsigned se, unsigned sg, unsigned sj, unsigned sl
	) {
		// Fractional parts of the fixed point X coordinates.
		const unsigned x1 = ((NX - i) << 16) / NX;
		const unsigned y1 = y;
		const unsigned f1 = (x1 >> 1) + (0x10000 >> 2);
		const unsigned f2 = (y1 >> 1) + (0x10000 >> 2);
		if (y1 <= f1 && sa == sj && sa != se) {
			*dp++ = Blender<Pixel>::template blend<f1 - y1>(sa, sb);
		} else if (y1 >= f1 && sa == sg && sa != sl) {
			*dp++ = Blender<Pixel>::template blend<y1 - f1>(sa, sc);
		} else if (x1 >= f2 && sa == se && sa != sj) {
			*dp++ = Blender<Pixel>::template blend<x1 - f2>(sa, sb);
		} else if (x1 <= f2 && sa == sl && sa != sg) {
			*dp++ = Blender<Pixel>::template blend<f2 - x1>(sa, sc);
		} else if (y1 >= x1) {
			*dp++ = Blender<Pixel>::template blend<y1 - x1>(sa, sc);
		} else if (y1 <= x1) {
			*dp++ = Blender<Pixel>::template blend<x1 - y1>(sa, sb);
		}
		PixelStripRepeater<i - 1>::template blendBackslash<NX, y, Pixel>(
			dp, sa, sb, sc, sd, se, sg, sj, sl );
	}

	template <unsigned NX, unsigned y, typename Pixel>
	inline static void blendSlash(
		Pixel* &dp,
		unsigned sa, unsigned sb, unsigned sc, unsigned sd,
		unsigned sf, unsigned sh, unsigned si, unsigned sk
	) {
		// Fractional parts of the fixed point X coordinates.
		const unsigned x1 = ((NX - i) << 16) / NX;
		const unsigned x2 = 0x10000 - x1;
		const unsigned y1 = y;
		const unsigned y2 = 0x10000 - y1;
		const unsigned f1 = (x1 >> 1) + (0x10000 >> 2);
		const unsigned f2 = (y1 >> 1) + (0x10000 >> 2);
		if (y2 >= f1 && sb == sh && sb != sf) {
			*dp++ = Blender<Pixel>::template blend<y2 - f1>(sb, sa);
		} else if (y2 <= f1 && sb == si && sb != sk) {
			*dp++ = Blender<Pixel>::template blend<f1 - y2>(sb, sd);
		} else if (x2 >= f2 && sb == sf && sb != sh) {
			*dp++ = Blender<Pixel>::template blend<x2 - f2>(sb, sa);
		} else if (x2 <= f2 && sb == sk && sb != si) {
			*dp++ = Blender<Pixel>::template blend<f2 - x2>(sb, sd);
		} else if (y2 >= x1) {
			*dp++ = Blender<Pixel>::template blend<y2 - x1>(sb, sa);
		} else if (y2 <= x1) {
			*dp++ = Blender<Pixel>::template blend<x1 - y2>(sb, sd);
		}
		PixelStripRepeater<i - 1>::template blendSlash<NX, y, Pixel>(
			dp, sa, sb, sc, sd, sf, sh, si, sk );
	}

	template <unsigned NX, unsigned y, typename Pixel>
	inline static void blend4(
		Pixel* &dp, unsigned sa, unsigned sb, unsigned sc, unsigned sd
	) {
		const unsigned x = ((NX - i) << 16) / NX;
		*dp++ = Blender<Pixel>::template blend<x, y>(sa, sb, sc, sd);
		PixelStripRepeater<i - 1>::template blend4<NX, y, Pixel>(dp, sa, sb, sc, sd);
	}
};
template <>
class PixelStripRepeater<0>
{
public:
	template <typename Pixel>
	inline static void fill(Pixel* &dp, unsigned sa) { }

	template <unsigned NX, unsigned y, typename Pixel>
	inline static void blendBackslash(
		Pixel* &dp,
		unsigned sa, unsigned sb, unsigned sc, unsigned sd,
		unsigned se, unsigned sg, unsigned sj, unsigned sl
	) { }

	template <unsigned NX, unsigned y, typename Pixel>
	inline static void blendSlash(
		Pixel* &dp,
		unsigned sa, unsigned sb, unsigned sc, unsigned sd,
		unsigned sf, unsigned sh, unsigned si, unsigned sk
	) { }

	template <unsigned NX, unsigned y, typename Pixel>
	inline static void blend4(
		Pixel* &dp, unsigned sa, unsigned sb, unsigned sc, unsigned sd ) { }
};

template <unsigned i>
class LineRepeater
{
public:
	template <unsigned NX, unsigned NY, typename Pixel>
	inline static void scaleFixedLine(
		Pixel* src0, Pixel* src1, Pixel* src2, Pixel* src3, unsigned srcWidth,
		OutputSurface& dst, unsigned& dstY
	) {
		Pixel* dp = dst.getLinePtr(dstY++, (Pixel*)0);
		// Calculate fixed point coordinate.
		const unsigned y1 = ((NY - i) << 16) / NY;

		unsigned pos1 = 0;
		unsigned pos2 = 0;
		unsigned pos3 = 1;
		Pixel sb = src1[0];
		Pixel sd = src2[0];
		for (unsigned srcX = 0; srcX < srcWidth; srcX++) {
			const unsigned pos0 = pos1;
			pos1 = pos2;
			pos2 = pos3;
			pos3 = min(pos1 + 3, srcWidth) - 1;
			// Get source pixels.
			const Pixel sa = sb; // current pixel
			sb = src1[pos2]; // next pixel
			const Pixel sc = sd;
			sd = src2[pos2];

			// Compute and write colour of destination pixel.
			if (sa == sb && sc == sd && sa == sc) {
				// All the same colour; fill.
				PixelStripRepeater<NX>::template fill<Pixel>(dp, sa);
			} else if (sa == sd && sb != sc) {
				// Pattern in the form of a backslash.
				PixelStripRepeater<NX>::template blendBackslash<NX, y1, Pixel>(
					dp, sa, sb, sc, sd, src0[pos1], src1[pos0], src2[pos3], src3[pos2]
					);
			} else if (sb == sc && sa != sd) {
				// Pattern in the form of a slash.
				PixelStripRepeater<NX>::template blendSlash<NX, y1, Pixel>(
					dp, sa, sb, sc, sd, src0[pos2], src2[pos0], src1[pos3], src3[pos1]
					);
			} else {
				// No pattern; use bilinear interpolatation.
				PixelStripRepeater<NX>::template blend4<NX, y1, Pixel>(
					dp, sa, sb, sc, sd
					);
			}
		}

		LineRepeater<i - 1>::template scaleFixedLine<NX, NY, Pixel>(
			src0, src1, src2, src3, srcWidth, dst, dstY
			);
	}
};
template <>
class LineRepeater<0>
{
public:
	template <unsigned NX, unsigned NY, typename Pixel>
	inline static void scaleFixedLine(
		Pixel* src0, Pixel* src1, Pixel* src2, Pixel* src3, unsigned srcWidth,
		OutputSurface& dst, unsigned& dstY
	) { }
};

template <typename Pixel>
template <unsigned NX, unsigned NY>
void SaI3xScaler<Pixel>::scaleFixed(
	FrameSource& src, unsigned srcStartY, unsigned srcEndY,
	OutputSurface& dst, unsigned dstStartY, unsigned /*dstEndY*/)
{
	const unsigned srcWidth = 320; // TODO: Get width from src.
	assert(dst.getWidth() == srcWidth * NX);
	assert(dst.getHeight() == src.getHeight() * NY);

	unsigned dstY = dstStartY;
	for (unsigned srcY = srcStartY; srcY < srcEndY; srcY++) {
		// Get source line pointers.
		Pixel* const dummy = 0;
		Pixel* src0 = src.getLinePtr(srcY == 0 ? 0 : srcY - 1, dummy);
		Pixel* src1 = src.getLinePtr(srcY, dummy);
		Pixel* src2 = src.getLinePtr(min(srcY + 1, srcEndY - 1), dummy);
		Pixel* src3 = src.getLinePtr(min(srcY + 2, srcEndY - 1), dummy);

		LineRepeater<NY>::template scaleFixedLine<NX, NY, Pixel>(
			src0, src1, src2, src3, srcWidth,
			dst, dstY
			);
	}
}

template <typename Pixel>
void SaI3xScaler<Pixel>::scaleAny(
	FrameSource& src, unsigned srcStartY, unsigned srcEndY,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	// Calculate fixed point end coordinates and deltas.
	const unsigned srcWidth = 320; // TODO: Get width from src.
	const unsigned wfinish = (srcWidth - 1) << 16;
	const unsigned dw = wfinish / (dst.getWidth() - 1);
	const unsigned hfinish = (src.getHeight() - 1) << 16;
	const unsigned dh = hfinish / (dst.getHeight() - 1);

	unsigned h = 0;
	for (unsigned dstY = dstStartY; dstY < dstEndY; dstY++) {
		// Get source line pointers.
		const unsigned line = srcStartY + (h >> 16);
		assert(srcStartY <= line && line < srcEndY);
		Pixel* const dummy = 0;
		Pixel* src0 = src.getLinePtr(line == 0 ? 0 : line - 1, dummy);
		Pixel* src1 = src.getLinePtr(line, dummy);
		Pixel* src2 = src.getLinePtr(min(line + 1, srcEndY - 1), dummy);
		Pixel* src3 = src.getLinePtr(min(line + 2, srcEndY - 1), dummy);

		// Get destination line pointer.
		Pixel* dp = dst.getLinePtr(dstY, dummy);

		// Fractional parts of the fixed point Y coordinates.
		const unsigned y1 = h & 0xffff;
		const unsigned y2 = 0x10000 - y1;
		// Next line.
		h += dh;

		unsigned pos1 = 0;
		unsigned pos2 = 0;
		unsigned pos3 = 1;
		Pixel B = src1[0];
		Pixel D = src2[0];
		for (unsigned w = 0; w < wfinish; ) {
			const unsigned pos0 = pos1;
			pos1 = pos2;
			pos2 = pos3;
			pos3 = min(pos1 + 3, srcWidth) - 1;
			// Get source pixels.
			const Pixel A = B; // current pixel
			B = src1[pos2]; // next pixel
			const Pixel C = D;
			D = src2[pos2];

			// Compute and write colour of destination pixel.
			if (A == B && C == D && A == C) { // 0
				do {
					*dp++ = A;
					w += dw;
				} while ((w >> 16) == pos1);
			} else if (A == D && B != C) { // 1
				do {
					// Fractional parts of the fixed point X coordinates.
					const unsigned x1 = w & 0xffff;
					const unsigned f1 = (x1 >> 1) + (0x10000 >> 2);
					const unsigned f2 = (y1 >> 1) + (0x10000 >> 2);
					Pixel product1;
					if (y1 <= f1 && A == src2[pos3] && A != src0[pos1]) { // close to B
						product1 = bilinear<Pixel>(A, B, f1 - y1);
					} else if (y1 >= f1 && A == src1[pos0] && A != src3[pos2]) { // close to C
						product1 = bilinear<Pixel>(A, C, y1 - f1);
					} else if (x1 >= f2 && A == src0[pos1] && A != src2[pos3]) { // close to B
						product1 = bilinear<Pixel>(A, B, x1 - f2);
					} else if (x1 <= f2 && A == src3[pos2] && A != src1[pos0]) { // close to C
						product1 = bilinear<Pixel>(A, C, f2 - x1);
					} else if (y1 >= x1) { // close to C
						product1 = bilinear<Pixel>(A, C, y1 - x1);
					} else if (y1 <= x1) { // close to B
						product1 = bilinear<Pixel>(A, B, x1 - y1);
					}
					*dp++ = product1;
					w += dw;
				} while ((w >> 16) == pos1);
			} else if (B == C && A != D) { // 2
				do {
					// Fractional parts of the fixed point X coordinates.
					const unsigned x1 = w & 0xffff;
					const unsigned x2 = 0x10000 - x1;
					const unsigned f1 = (x1 >> 1) + (0x10000 >> 2);
					const unsigned f2 = (y1 >> 1) + (0x10000 >> 2);
					Pixel product1;
					if (y2 >= f1 && B == src2[pos0] && B != src0[pos2]) { // close to A
						product1 = bilinear<Pixel>(B, A, y2 - f1);
					} else if (y2 <= f1 && B == src1[pos3] && B != src3[pos1]) { // close to D
						product1 = bilinear<Pixel>(B, D, f1 - y2);
					} else if (x2 >= f2 && B == src0[pos2] && B != src2[pos0]) { // close to A
						product1 = bilinear<Pixel>(B, A, x2 - f2);
					} else if (x2 <= f2 && B == src3[pos1] && B != src1[pos3]) { // close to D
						product1 = bilinear<Pixel>(B, D, f2 - x2);
					} else if (y2 >= x1) { // close to A
						product1 = bilinear<Pixel>(B, A, y2 - x1);
					} else if (y2 <= x1) { // close to D
						product1 = bilinear<Pixel>(B, D, x1 - y2);
					}
					*dp++ = product1;
					w += dw;
				} while ((w >> 16) == pos1);
			} else { // 3
				do {
					// Fractional parts of the fixed point X coordinates.
					const unsigned x1 = w & 0xffff;
					*dp++ = bilinear4<Pixel>(A, B, C, D, x1, y1);
					w += dw;
				} while ((w >> 16) == pos1);
			}
		}
	}
}

template <typename Pixel>
void SaI3xScaler<Pixel>::scale1x1to3x3(
	FrameSource& src, unsigned srcStartY, unsigned srcEndY,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	scaleFixed<3, 3>(src, srcStartY, srcEndY, dst, dstStartY, dstEndY);
}


// Force template instantiation.
template class SaI3xScaler<word>;
template class SaI3xScaler<unsigned int>;

} // namespace openmsx

