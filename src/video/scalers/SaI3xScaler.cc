// 2xSaI is Copyright (c) 1999-2001 by Derek Liauw Kie Fa.
//   http://elektron.its.tudelft.nl/~dalikifa/
// 2xSaI is free under GPL.
//
// Modified for use in openMSX by Maarten ter Huurne.

#include "SaI3xScaler.hh"
#include "FrameSource.hh"
#include "ScalerOutput.hh"
#include "narrow.hh"
#include "vla.hh"
#include "build-info.hh"
#include <cassert>
#include <cstdint>

namespace openmsx {

template<std::unsigned_integral Pixel>
SaI3xScaler<Pixel>::SaI3xScaler(const PixelOperations<Pixel>& pixelOps_)
	: Scaler3<Pixel>(pixelOps_)
	, pixelOps(pixelOps_)
{
}

template<std::unsigned_integral Pixel>
void SaI3xScaler<Pixel>::scaleBlank1to3(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	unsigned dstHeight = dst.getHeight();
	unsigned stopDstY = (dstEndY == dstHeight)
	                  ? dstEndY : dstEndY - 3;
	unsigned srcY = srcStartY, dstY = dstStartY;
	for (/* */; dstY < stopDstY; srcY += 1, dstY += 3) {
		auto color = src.getLineColor<Pixel>(srcY);
		for (auto i : xrange(3)) {
			dst.fillLine(dstY + i, color);
		}
	}
	if (dstY != dstHeight) {
		unsigned nextLineWidth = src.getLineWidth(srcY + 1);
		assert(src.getLineWidth(srcY) == 1);
		assert(nextLineWidth != 1);
		this->dispatchScale(src, srcY, srcEndY, nextLineWidth,
		                    dst, dstY, dstEndY);
	}
}

template<std::unsigned_integral Pixel>
inline Pixel SaI3xScaler<Pixel>::blend(Pixel p1, Pixel p2) const
{
	return pixelOps.template blend<1, 1>(p1, p2);
}

static constexpr unsigned redBlueMask = 0xF81F;
static constexpr unsigned greenMask = 0x7E0;

// TODO use PixelOperations::lerp()
template<std::unsigned_integral Pixel>
[[nodiscard]] static Pixel bilinear(unsigned a, unsigned b, unsigned x);

template<> [[nodiscard]] uint16_t bilinear<uint16_t>(unsigned a, unsigned b, unsigned x)
{
	if (a == b) return narrow_cast<uint16_t>(a);

	const unsigned areaB = x >> 11; // reduce 16 bit fraction to 5 bits
	const unsigned areaA = 0x20 - areaB;

	a = (a & redBlueMask) | ((a & greenMask) << 16);
	b = (b & redBlueMask) | ((b & greenMask) << 16);
	const unsigned result = ((areaA * a) + (areaB * b)) >> 5;
	return (result & redBlueMask) | ((result >> 16) & greenMask);
}

template<> [[nodiscard]] uint32_t bilinear<uint32_t>(unsigned a, unsigned b, unsigned x)
{
	if (a == b) return a;

	const unsigned areaB = x >> 8; // reduce 16 bit fraction to 8 bits
	const unsigned areaA = 0x100 - areaB;

	const unsigned result0 =
		((a & 0x00FF00FF) * areaA + (b & 0x00FF00FF) * areaB) >> 8;
	const unsigned result1 =
		((a & 0xFF00FF00) >> 8) * areaA + ((b & 0xFF00FF00) >> 8) * areaB;
	return (result0 & 0x00FF00FF) | (result1 & 0xFF00FF00);
}

// TODO move to PixelOperations
template<std::unsigned_integral Pixel> [[nodiscard]] static Pixel bilinear4(
	unsigned a, unsigned b, unsigned c, unsigned d, unsigned x, unsigned y);

template<> uint16_t bilinear4<uint16_t>(
	unsigned a, unsigned b, unsigned c, unsigned d, unsigned x, unsigned y)
{
	x >>= 11;
	y >>= 11;
	const unsigned xy = (x * y) >> 5;

	const unsigned areaA = 0x20 + xy - x - y;
	const unsigned areaB = x - xy;
	const unsigned areaC = y - xy;
	const unsigned areaD = xy;

	a = (a & redBlueMask) | ((a & greenMask) << 16);
	b = (b & redBlueMask) | ((b & greenMask) << 16);
	c = (c & redBlueMask) | ((c & greenMask) << 16);
	d = (d & redBlueMask) | ((d & greenMask) << 16);
	unsigned result = (
		(areaA * a) + (areaB * b) + (areaC * c) + (areaD * d)
		) >> 5;
	return (result & redBlueMask) | ((result >> 16) & greenMask);
}

template<> [[nodiscard]] uint32_t bilinear4<uint32_t>(
	unsigned a, unsigned b, unsigned c, unsigned d, unsigned x, unsigned y)
{
	x >>= 8;
	y >>= 8;
	const unsigned xy = (x * y) >> 8;

	const unsigned areaA = (1 << 8) + xy - x - y;
	const unsigned areaB = x - xy;
	const unsigned areaC = y - xy;
	const unsigned areaD = xy;

	const unsigned result0 =
		((a & 0x00FF00FF) * areaA + (b & 0x00FF00FF) * areaB +
		 (c & 0x00FF00FF) * areaC + (d & 0x00FF00FF) * areaD) >> 8;
	const unsigned result1 =
		((a & 0xFF00FF00) >> 8) * areaA + ((b & 0xFF00FF00) >> 8) * areaB +
		((c & 0xFF00FF00) >> 8) * areaC + ((d & 0xFF00FF00) >> 8) * areaD;
	return (result0 & 0x00FF00FF) | (result1 & 0xFF00FF00);
}

template<std::unsigned_integral Pixel>
class Blender
{
public:
	template<unsigned x>
	[[nodiscard]] inline static Pixel blend(unsigned a, unsigned b);

	template<unsigned x, unsigned y>
	[[nodiscard]] inline static Pixel blend(unsigned a, unsigned b, unsigned c, unsigned d);
};

template<unsigned X, unsigned OLD, unsigned NEW>
struct Round {
	static_assert(OLD > NEW);
	static constexpr unsigned result =
		(X >> (OLD - NEW)) + ((X >> (OLD - NEW - 1)) & 1);
};

template<std::unsigned_integral Pixel>
template<unsigned x>
inline Pixel Blender<Pixel>::blend(unsigned a, unsigned b)
{
	if (a == b) return narrow_cast<Pixel>(a);

	const unsigned bits = (sizeof(Pixel) == 2) ? 5 : 8;
	const unsigned areaB = Round<x, 16, bits>::result;
	const unsigned areaA = (1 << bits) - areaB;

	if constexpr (sizeof(Pixel) == 2) {
		a = (a & redBlueMask) | ((a & greenMask) << 16);
		b = (b & redBlueMask) | ((b & greenMask) << 16);
		const unsigned result = ((areaA * a) + (areaB * b)) >> bits;
		return (result & redBlueMask) | ((result >> 16) & greenMask);
	} else {
		const unsigned result0 =
			((a & 0x00FF00FF) * areaA +
			 (b & 0x00FF00FF) * areaB) >> bits;
		const unsigned result1 =
			((a & 0xFF00FF00) >> bits) * areaA +
			((b & 0xFF00FF00) >> bits) * areaB;
		return (result0 & 0x00FF00FF) | (result1 & 0xFF00FF00);
	}
}

template<std::unsigned_integral Pixel>
template<unsigned wx, unsigned wy>
inline Pixel Blender<Pixel>::blend(
	unsigned a, unsigned b, unsigned c, unsigned d)
{
	const unsigned bits = (sizeof(Pixel) == 2) ? 5 : 8;
	const unsigned xy = (wx * wy) >> 16;
	const unsigned areaB = Round<wx - xy, 16, bits>::result;
	const unsigned areaC = Round<wy - xy, 16, bits>::result;
	const unsigned areaD = Round<xy,      16, bits>::result;
	const unsigned areaA = (1 << bits) - areaB - areaC - areaD;

	if constexpr (sizeof(Pixel) == 2) {
		a = (a & redBlueMask) | ((a & greenMask) << 16);
		b = (b & redBlueMask) | ((b & greenMask) << 16);
		c = (c & redBlueMask) | ((c & greenMask) << 16);
		d = (d & redBlueMask) | ((d & greenMask) << 16);
		unsigned result = (
			(areaA * a) + (areaB * b) + (areaC * c) + (areaD * d)
			) >> bits;
		return (result & redBlueMask) | ((result >> 16) & greenMask);
	} else {
		const unsigned result0 =
			((a & 0x00FF00FF) * areaA +
			 (b & 0x00FF00FF) * areaB +
			 (c & 0x00FF00FF) * areaC +
			 (d & 0x00FF00FF) * areaD) >> bits;
		const unsigned result1 =
			((a & 0xFF00FF00) >> bits) * areaA +
			((b & 0xFF00FF00) >> bits) * areaB +
			((c & 0xFF00FF00) >> bits) * areaC +
			((d & 0xFF00FF00) >> bits) * areaD;
		return (result0 & 0x00FF00FF) | (result1 & 0xFF00FF00);
	}
}

template<unsigned i>
class PixelStripRepeater
{
public:
	template<std::unsigned_integral Pixel>
	inline static void fill(Pixel*& dp, unsigned sa) {
		*dp++ = Pixel(sa);
		PixelStripRepeater<i - 1>::template fill<Pixel>(dp, sa);
	}

	template<unsigned NX, unsigned y, std::unsigned_integral Pixel>
	inline static void blendBackslash(
		Pixel*& dp,
		unsigned sa, unsigned sb, unsigned sc, unsigned sd,
		unsigned se, unsigned sg, unsigned sj, unsigned sl)
	{
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
			dp, sa, sb, sc, sd, se, sg, sj, sl);
	}

	template<unsigned NX, unsigned y, std::unsigned_integral Pixel>
	inline static void blendSlash(
		Pixel*& dp,
		unsigned sa, unsigned sb, unsigned sc, unsigned sd,
		unsigned sf, unsigned sh, unsigned si, unsigned sk)
	{
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
			dp, sa, sb, sc, sd, sf, sh, si, sk);
	}

	template<unsigned NX, unsigned y, std::unsigned_integral Pixel>
	inline static void blend4(
		Pixel*& dp, unsigned sa, unsigned sb, unsigned sc, unsigned sd)
	{
		const unsigned x = ((NX - i) << 16) / NX;
		*dp++ = Blender<Pixel>::template blend<x, y>(sa, sb, sc, sd);
		PixelStripRepeater<i - 1>::template blend4<NX, y, Pixel>(dp, sa, sb, sc, sd);
	}
};
template<>
class PixelStripRepeater<0>
{
public:
	template<std::unsigned_integral Pixel>
	inline static void fill(Pixel*& /*dp*/, unsigned /*sa*/) { }

	template<unsigned NX, unsigned y, std::unsigned_integral Pixel>
	inline static void blendBackslash(
		Pixel*& /*dp*/, unsigned /*sa*/, unsigned /*sb*/,
		unsigned /*sc*/, unsigned /*sd*/, unsigned /*se*/,
		unsigned /*sg*/, unsigned /*sj*/, unsigned /*sl*/) { }

	template<unsigned NX, unsigned y, std::unsigned_integral Pixel>
	inline static void blendSlash(
		Pixel*& /*dp*/, unsigned /*sa*/, unsigned /*sb*/,
		unsigned /*sc*/, unsigned /*sd*/, unsigned /*sf*/,
		unsigned /*sh*/, unsigned /*si*/, unsigned /*sk*/) { }

	template<unsigned NX, unsigned y, std::unsigned_integral Pixel>
	inline static void blend4(Pixel*& /*dp*/, unsigned /*sa*/,
		unsigned /*sb*/, unsigned /*sc*/, unsigned /*sd*/) { }
};

template<unsigned i>
class LineRepeater
{
public:
	template<unsigned NX, unsigned NY, std::unsigned_integral Pixel>
	inline static void scaleFixedLine(
		std::span<const Pixel> src0, std::span<const Pixel> src1,
		std::span<const Pixel> src2, std::span<const Pixel> src3,
		ScalerOutput<Pixel>& dst, unsigned& dstY)
	{
		auto srcWidth = src0.size();
		assert(src0.size() == srcWidth);
		assert(src1.size() == srcWidth);
		assert(src2.size() == srcWidth);
		assert(src3.size() == srcWidth);

		auto dstLine = dst.acquireLine(dstY);
		auto* dp = dstLine.data();
		// Calculate fixed point coordinate.
		static constexpr unsigned y1 = ((NY - i) << 16) / NY;

		size_t pos1 = 0;
		size_t pos2 = 0;
		size_t pos3 = 1;
		Pixel sb = src1[0];
		Pixel sd = src2[0];
		repeat(srcWidth, [&] {
			const auto pos0 = pos1;
			pos1 = pos2;
			pos2 = pos3;
			pos3 = std::min(pos1 + 3, srcWidth) - 1;
			// Get source pixels.
			const Pixel sa = sb; // current pixel
			sb = src1[pos2]; // next pixel
			const Pixel sc = sd;
			sd = src2[pos2];

			// Compute and write color of destination pixel.
			if (sa == sb && sc == sd && sa == sc) {
				// All the same color; fill.
				PixelStripRepeater<NX>::template fill<Pixel>(dp, sa);
			} else if (sa == sd && sb != sc) {
				// Pattern in the form of a backslash.
				PixelStripRepeater<NX>::template blendBackslash<NX, y1, Pixel>(
					dp, sa, sb, sc, sd, src0[pos1], src1[pos0], src2[pos3], src3[pos2]);
			} else if (sb == sc && sa != sd) {
				// Pattern in the form of a slash.
				PixelStripRepeater<NX>::template blendSlash<NX, y1, Pixel>(
					dp, sa, sb, sc, sd, src0[pos2], src2[pos0], src1[pos3], src3[pos1]);
			} else {
				// No pattern; use bilinear interpolation.
				PixelStripRepeater<NX>::template blend4<NX, y1, Pixel>(
					dp, sa, sb, sc, sd);
			}
		});
		dst.releaseLine(dstY, dstLine);
		++dstY;

		LineRepeater<i - 1>::template scaleFixedLine<NX, NY, Pixel>(
			src0, src1, src2, src3, dst, dstY);
	}
};
template<>
class LineRepeater<0>
{
public:
	template<unsigned NX, unsigned NY, std::unsigned_integral Pixel>
	inline static void scaleFixedLine(
		std::span<const Pixel> /*src0*/, std::span<const Pixel> /*src1*/,
		std::span<const Pixel> /*src2*/, std::span<const Pixel> /*src3*/,
		ScalerOutput<Pixel>& /*dst*/, unsigned& /*dstY*/)
	{ }
};

template<std::unsigned_integral Pixel>
template<unsigned NX, unsigned NY>
void SaI3xScaler<Pixel>::scaleFixed(FrameSource& src,
	unsigned srcStartY, unsigned /*srcEndY*/, unsigned srcWidth,
	ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	assert(dst.getWidth() == srcWidth * NX);
	assert(dst.getHeight() == src.getHeight() * NY);

	VLA_SSE_ALIGNED(Pixel, buf0, srcWidth);
	VLA_SSE_ALIGNED(Pixel, buf1, srcWidth);
	VLA_SSE_ALIGNED(Pixel, buf2, srcWidth);
	VLA_SSE_ALIGNED(Pixel, buf3, srcWidth);

	auto srcY = narrow<int>(srcStartY);
	auto src0 = src.getLine(srcY - 1, buf0);
	auto src1 = src.getLine(srcY + 0, buf1);
	auto src2 = src.getLine(srcY + 1, buf2);

	auto dstY = dstStartY;
	while (dstY < dstEndY) {
		auto src3 = src.getLine(srcY + 2, buf3);
		LineRepeater<NY>::template scaleFixedLine<NX, NY, Pixel>(
			src0, src1, src2, src3, dst, dstY);
		src0 = src1;
		src1 = src2;
		src2 = src3;
		std::swap(buf0, buf1);
		std::swap(buf1, buf2);
		std::swap(buf2, buf3);
		++srcY;
	}
}

template<std::unsigned_integral Pixel>
void SaI3xScaler<Pixel>::scaleAny(FrameSource& src,
	unsigned srcStartY, unsigned /*srcEndY*/, unsigned srcWidth,
	ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	// Calculate fixed point end coordinates and deltas.
	const unsigned wFinish = (srcWidth - 1) << 16;
	const unsigned dw = wFinish / (dst.getWidth() - 1);
	const unsigned hFinish = (src.getHeight() - 1) << 16;
	const unsigned dh = hFinish / (dst.getHeight() - 1);

	VLA_SSE_ALIGNED(Pixel, buf0, srcWidth);
	VLA_SSE_ALIGNED(Pixel, buf1, srcWidth);
	VLA_SSE_ALIGNED(Pixel, buf2, srcWidth);
	VLA_SSE_ALIGNED(Pixel, buf3, srcWidth);

	unsigned h = 0;
	for (auto dstY : xrange(dstStartY, dstEndY)) {
		// Get source line pointers.
		auto line = narrow<int>(srcStartY + (h >> 16));
		// TODO possible optimization: reuse srcN from previous step
		auto src0 = src.getLine(line - 1, buf0);
		auto src1 = src.getLine(line + 0, buf1);
		auto src2 = src.getLine(line + 1, buf2);
		auto src3 = src.getLine(line + 2, buf3);

		// Get destination line pointer.
		auto dstLine = dst.acquireLine(dstY);
		auto* dp = dstLine.data();

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
		for (unsigned w = 0; w < wFinish; /**/) {
			const unsigned pos0 = pos1;
			pos1 = pos2;
			pos2 = pos3;
			pos3 = std::min(pos1 + 3, srcWidth) - 1;
			// Get source pixels.
			const Pixel A = B; // current pixel
			B = src1[pos2]; // next pixel
			const Pixel C = D;
			D = src2[pos2];

			// Compute and write color of destination pixel.
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
					} else {
						assert(y1 < x1); // close to B
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
					} else {
						assert(y2 < x1); // close to D
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
		dst.releaseLine(dstY, dstLine);
	}
}

template<std::unsigned_integral Pixel>
void SaI3xScaler<Pixel>::scale1x1to3x3(FrameSource& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	ScalerOutput<Pixel>& dst, unsigned dstStartY, unsigned dstEndY)
{
	scaleFixed<3, 3>(src, srcStartY, srcEndY, srcWidth, dst, dstStartY, dstEndY);
}


// Force template instantiation.
#if HAVE_16BPP
template class SaI3xScaler<uint16_t>;
#endif
#if HAVE_32BPP
template class SaI3xScaler<uint32_t>;
#endif

} // namespace openmsx
