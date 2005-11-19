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

template <class Pixel>
SaI3xScaler<Pixel>::SaI3xScaler(SDL_PixelFormat* format)
	: Scaler3<Pixel>(format)
	, pixelOps(format)
{
}

const unsigned WIDTH256 = 320; // TODO: Specify this in a clean way.
const unsigned HEIGHT = 480;

template <class Pixel>
inline Pixel SaI3xScaler<Pixel>::blend(Pixel p1, Pixel p2)
{
	return pixelOps.template blend<1, 1>(p1, p2);
}

static unsigned redblueMask = 0xF81F;
static unsigned greenMask = 0x7E0;

template <class Pixel>
static Pixel bilinear(unsigned a, unsigned b, unsigned x)
{
	if (a == b) return a;

	const unsigned areaB = (x >> 11) & 0x1f; //reduce 16 bit fraction to 5 bits
	const unsigned areaA = 0x20 - areaB;

	if (sizeof(Pixel) == 2) {
		a = (a & redblueMask) | ((a & greenMask) << 16);
		b = (b & redblueMask) | ((b & greenMask) << 16);
		const unsigned result = ((areaA * a) + (areaB * b)) >> 5;
		return (result & redblueMask) | ((result >> 16) & greenMask);
	} else {
		const unsigned result0 =
			((a & 0x00FF00FF) * areaA + (b & 0x00FF00FF) * areaB) >> 5;
		const unsigned result1 =
			((a & 0xFF00FF00) >> 5) * areaA + ((b & 0xFF00FF00) >> 5) * areaB;
		return (result0 & 0x00FF00FF) | (result1 & 0xFF00FF00);
	}
}

/*
static word bilinear(unsigned a, unsigned b, unsigned x)
{
	if (a == b) return a;

	const unsigned areaB = (x >> 11) & 0x1f; //reduce 16 bit fraction to 5 bits
	const unsigned areaA = 0x20 - areaB;

	a = (a & redblueMask) | ((a & greenMask) << 16);
	b = (b & redblueMask) | ((b & greenMask) << 16);

	const unsigned result = ((areaA * a) + (areaB * b)) >> 5;

	return (result & redblueMask) | ((result >> 16) & greenMask);
}
*/

template <class Pixel>
static Pixel bilinear4(
	unsigned a, unsigned b, unsigned c, unsigned d, unsigned x, unsigned y
) {
	x = (x >> 11) & 0x1f;
	y = (y >> 11) & 0x1f;
	const unsigned xy = (x*y) >> 5;

	const unsigned areaA = 0x20 + xy - x - y;
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
		const unsigned result0 = (
			(a & 0x00FF00FF) * areaA + (b & 0x00FF00FF) * areaB +
			(c & 0x00FF00FF) * areaC + (d & 0x00FF00FF) * areaD
			) >> 5;
		const unsigned result1 =
			((a & 0xFF00FF00) >> 5) * areaA + ((b & 0xFF00FF00) >> 5) * areaB +
			((c & 0xFF00FF00) >> 5) * areaC + ((d & 0xFF00FF00) >> 5) * areaD;
		return (result0 & 0x00FF00FF) | (result1 & 0xFF00FF00);
	}
}

/*
static word bilinear4(
	unsigned a, unsigned b, unsigned c, unsigned d, unsigned x, unsigned y
) {
	x = (x >> 11) & 0x1f;
	y = (y >> 11) & 0x1f;
	const unsigned xy = (x*y) >> 5;

	a = (a & redblueMask) | ((a & greenMask) << 16);
	b = (b & redblueMask) | ((b & greenMask) << 16);
	c = (c & redblueMask) | ((c & greenMask) << 16);
	d = (d & redblueMask) | ((d & greenMask) << 16);

	const unsigned areaA = 0x20 + xy - x - y;
	const unsigned areaB = x - xy;
	const unsigned areaC = y - xy;
	const unsigned areaD = xy;

	unsigned result = (
		(areaA * a) + (areaB * b) + (areaC * c) + (areaD * d)
		) >> 5;

	return (result & redblueMask) | ((result >> 16) & greenMask);
}
*/

template <class Pixel>
void SaI3xScaler<Pixel>::scale1x1to3x3(
	FrameSource& src, unsigned srcStartY, unsigned srcEndY,
	OutputSurface& dst, unsigned dstStartY, unsigned dstEndY)
{
	// Calculate fixed point end coordinates and deltas.
	const unsigned wfinish = (320 - 1) << 16;
	const unsigned dw = wfinish / (dst.getWidth() - 1);
	const unsigned hfinish = (src.getHeight() - 1) << 16;
	const unsigned dh = hfinish / (dst.getHeight() - 1);

	unsigned h = 0;
	for (unsigned dstY = dstStartY; dstY < dstEndY; dstY++) {
		// Get source line pointers.
		const unsigned line = srcStartY + (h >> 16);
		assert(srcStartY <= line && line < srcEndY);
		Pixel* const dummy = 0;
		Pixel* src0 = src.getLinePtr(max(line - 1, srcStartY), dummy);
		Pixel* src1 = src.getLinePtr(line, dummy);
		Pixel* src2 = src.getLinePtr(min(line + 1, srcEndY - 1), dummy);
		Pixel* src3 = src.getLinePtr(min(line + 1, srcEndY - 1), dummy);

		// Get destination line pointer.
		assert(dstY < dstEndY);
		Pixel* dp = dst.getLinePtr(dstY, dummy);

		// Fractional parts of the fixed point Y coordinates.
		const unsigned y1 = h & 0xffff;
		const unsigned y2 = 0x10000 - y1;
		// Next line.
		h += dh;

		for (unsigned w = 0; w < wfinish; w += dw) {
			// Clip source X.
			const unsigned pos1 = w >> 16;
			assert(pos1 < WIDTH256);
			const unsigned pos0 = pos1 == 0 ? 0 : pos1 - 1;
			const unsigned pos2 = min(pos1 + 1, WIDTH256 - 1);
			const unsigned pos3 = min(pos1 + 2, WIDTH256 - 1);
			// Get source pixels.
			const Pixel A = src1[pos1]; // current pixel
			const Pixel B = src1[pos2]; // next pixel
			const Pixel C = src2[pos1];
			const Pixel D = src2[pos2];
			const Pixel E = src0[pos1];
			const Pixel F = src0[pos2];
			const Pixel G = src1[pos0];
			const Pixel H = src2[pos0];
			const Pixel I = src1[pos3];
			const Pixel J = src2[pos3];
			const Pixel K = src3[pos1];
			const Pixel L = src3[pos2];

			// Fractional parts of the fixed point X coordinates.
			const unsigned x1 = w & 0xffff;
			const unsigned x2 = 0x10000 - x1;

			// Compute colour of destination pixel.
			Pixel product1;
			if (A == B && C == D && A == C) { // 0
				product1 = A;
			} else if (A == D && B != C) { // 1
				const unsigned f1 = (x1 >> 1) + (0x10000 >> 2);
				const unsigned f2 = (y1 >> 1) + (0x10000 >> 2);
				if (y1 <= f1 && A == J && A != E) { // close to B
					product1 = bilinear<Pixel>(A, B, f1 - y1);
				} else if (y1 >= f1 && A == G && A != L) { // close to C
					product1 = bilinear<Pixel>(A, C, y1 - f1);
				} else if (x1 >= f2 && A == E && A != J) { // close to B
					product1 = bilinear<Pixel>(A, B, x1 - f2);
				} else if (x1 <= f2 && A == L && A != G) { // close to C
					product1 = bilinear<Pixel>(A, C, f2 - x1);
				} else if (y1 >= x1) { // close to C
					product1 = bilinear<Pixel>(A, C, y1 - x1);
				} else if (y1 <= x1) { // close to B
					product1 = bilinear<Pixel>(A, B, x1 - y1);
				}
			} else if (B == C && A != D) { // 2
				const unsigned f1 = (x1 >> 1) + (0x10000 >> 2);
				const unsigned f2 = (y1 >> 1) + (0x10000 >> 2);
				if (y2 >= f1 && B == H && B != F) { // close to A
					product1 = bilinear<Pixel>(B, A, y2 - f1);
				} else if (y2 <= f1 && B == I && B != K) { // close to D
					product1 = bilinear<Pixel>(B, D, f1 - y2);
				} else if (x2 >= f2 && B == F && B != H) { // close to A
					product1 = bilinear<Pixel>(B, A, x2 - f2);
				} else if (x2 <= f2 && B == K && B != I) { // close to D
					product1 = bilinear<Pixel>(B, D, f2 - x2);
				} else if (y2 >= x1) { // close to A
					product1 = bilinear<Pixel>(B, A, y2 - x1);
				} else if (y2 <= x1) { // close to D
					product1 = bilinear<Pixel>(B, D, x1 - y2);
				}
			} else { // 3
				product1 = bilinear4<Pixel>(A, B, C, D, x1, y1);
			}
			// Write destination pixel.
			*dp++ = product1;
		}
	}
}


// Force template instantiation.
template class SaI3xScaler<word>;
template class SaI3xScaler<unsigned int>;

} // namespace openmsx

