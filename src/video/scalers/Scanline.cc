#include "Scanline.hh"
#include "PixelOperations.hh"
#include "enumerate.hh"
#include "ranges.hh"
#include "unreachable.hh"
#include "xrange.hh"
#include <cassert>
#include <cstddef>
#ifdef __SSE2__
#include <emmintrin.h>
#endif

namespace openmsx {

// class Multiply<uint16_t>

Multiply<uint16_t>::Multiply(const PixelOperations<uint16_t>& pixelOps_)
	: pixelOps(pixelOps_)
	, factor(0)
{
	ranges::fill(tab, 0);
}

void Multiply<uint16_t>::setFactor(unsigned f)
{
	if (f == factor) return;
	factor = f;

	for (auto [p, t] : enumerate(tab)) {
		auto pix = uint32_t(p);
		t = ((((pix & pixelOps.getRmask()) * f) >> 8) & pixelOps.getRmask()) |
		    ((((pix & pixelOps.getGmask()) * f) >> 8) & pixelOps.getGmask()) |
		    ((((pix & pixelOps.getBmask()) * f) >> 8) & pixelOps.getBmask());
	}
}

inline uint16_t Multiply<uint16_t>::multiply(uint16_t p, unsigned f) const
{
	unsigned r = (((p & pixelOps.getRmask()) * f) >> 8) & pixelOps.getRmask();
	unsigned g = (((p & pixelOps.getGmask()) * f) >> 8) & pixelOps.getGmask();
	unsigned b = (((p & pixelOps.getBmask()) * f) >> 8) & pixelOps.getBmask();
	return r | g | b;
}

inline uint16_t Multiply<uint16_t>::multiply(uint16_t p) const
{
	return tab[p];
}


// class Multiply<uint32_t>

Multiply<uint32_t>::Multiply(const PixelOperations<uint32_t>& /*pixelOps*/)
{
}

void Multiply<uint32_t>::setFactor(unsigned f)
{
	factor = f;
}

inline uint32_t Multiply<uint32_t>::multiply(uint32_t p, unsigned f) const
{
	return PixelOperations<uint32_t>::multiply(p, f);
}

inline uint32_t Multiply<uint32_t>::multiply(uint32_t p) const
{
	return multiply(p, factor);
}


#ifdef __SSE2__

// 32bpp
static inline void drawSSE2_1(
	const char* __restrict in1, const char* __restrict in2,
	      char* __restrict out, __m128i f)
{
	__m128i zero = _mm_setzero_si128();
	__m128i a = *reinterpret_cast<const __m128i*>(in1);
	__m128i b = *reinterpret_cast<const __m128i*>(in2);
	__m128i c = _mm_avg_epu8(a, b);
	__m128i l = _mm_unpacklo_epi8(c, zero);
	__m128i h = _mm_unpackhi_epi8(c, zero);
	__m128i m = _mm_mulhi_epu16(l, f);
	__m128i n = _mm_mulhi_epu16(h, f);
	__m128i r = _mm_packus_epi16(m, n);
	*reinterpret_cast<__m128i*>(out) = r;
}
static inline void drawSSE2(
	const uint32_t* __restrict in1_,
	const uint32_t* __restrict in2_,
	      uint32_t* __restrict out_,
	unsigned factor,
	size_t width,
	PixelOperations<uint32_t>& /*dummy*/,
	Multiply<uint32_t>& /*dummy*/)
{
	width *= sizeof(uint32_t); // in bytes
	assert(width >= 64);
	assert((reinterpret_cast<uintptr_t>(in1_) % sizeof(__m128i)) == 0);
	assert((reinterpret_cast<uintptr_t>(in2_) % sizeof(__m128i)) == 0);
	assert((reinterpret_cast<uintptr_t>(out_) % sizeof(__m128i)) == 0);
	const auto* in1 = reinterpret_cast<const char*>(in1_) + width;
	const auto* in2 = reinterpret_cast<const char*>(in2_) + width;
	      auto* out = reinterpret_cast<      char*>(out_) + width;

	__m128i f = _mm_set1_epi16(factor << 8);
	ptrdiff_t x = -ptrdiff_t(width);
	do {
		drawSSE2_1(in1 + x +   0, in2 + x +  0, out + x +  0, f);
		drawSSE2_1(in1 + x +  16, in2 + x + 16, out + x + 16, f);
		drawSSE2_1(in1 + x +  32, in2 + x + 32, out + x + 32, f);
		drawSSE2_1(in1 + x +  48, in2 + x + 48, out + x + 48, f);
		x += 64;
	} while (x < 0);
}

// 16bpp
static inline void drawSSE2(
	const uint16_t* __restrict in1_,
	const uint16_t* __restrict in2_,
	      uint16_t* __restrict out_,
	unsigned factor,
	size_t width,
	PixelOperations<uint16_t>& pixelOps,
	Multiply<uint16_t>& darkener)
{
	width *= sizeof(uint16_t); // in bytes
	assert(width >= 16);
	const auto* in1 = reinterpret_cast<const char*>(in1_) + width;
	const auto* in2 = reinterpret_cast<const char*>(in2_) + width;
	      auto* out = reinterpret_cast<      char*>(out_) + width;

	darkener.setFactor(factor);
	auto table = darkener.getTable();
	__m128i mask = _mm_set1_epi16(pixelOps.getBlendMask());

	ptrdiff_t x = -ptrdiff_t(width);
	do {
		__m128i a = *reinterpret_cast<const __m128i*>(in1 + x);
		__m128i b = *reinterpret_cast<const __m128i*>(in2 + x);
		__m128i c = _mm_add_epi16(
			_mm_and_si128(a, b),
			_mm_srli_epi16(
				_mm_and_si128(mask, _mm_xor_si128(a, b)),
				1));
		*reinterpret_cast<__m128i*>(out + x) = _mm_set_epi16(
			table[_mm_extract_epi16(c, 7)],
			table[_mm_extract_epi16(c, 6)],
			table[_mm_extract_epi16(c, 5)],
			table[_mm_extract_epi16(c, 4)],
			table[_mm_extract_epi16(c, 3)],
			table[_mm_extract_epi16(c, 2)],
			table[_mm_extract_epi16(c, 1)],
			table[_mm_extract_epi16(c, 0)]);
		// An alternative for the above statement is this block (this
		// is close to what we has in our old MMX routine). On gcc this
		// generates significantly shorter (25%) but also significantly
		// slower (30%) code. On clang both alternatives generate
		// identical code, comparable in size to the fast gcc version
		// (but still a bit faster).
		//c = _mm_insert_epi16(c, table[_mm_extract_epi16(c, 0)], 0);
		//c = _mm_insert_epi16(c, table[_mm_extract_epi16(c, 1)], 1);
		//c = _mm_insert_epi16(c, table[_mm_extract_epi16(c, 2)], 2);
		//c = _mm_insert_epi16(c, table[_mm_extract_epi16(c, 3)], 3);
		//c = _mm_insert_epi16(c, table[_mm_extract_epi16(c, 4)], 4);
		//c = _mm_insert_epi16(c, table[_mm_extract_epi16(c, 5)], 5);
		//c = _mm_insert_epi16(c, table[_mm_extract_epi16(c, 6)], 6);
		//c = _mm_insert_epi16(c, table[_mm_extract_epi16(c, 7)], 7);
		//*reinterpret_cast<__m128i*>(out + x) = c;

		x += 16;
	} while (x < 0);
}

#endif


// class Scanline

template<std::unsigned_integral Pixel>
Scanline<Pixel>::Scanline(const PixelOperations<Pixel>& pixelOps_)
	: darkener(pixelOps_)
	, pixelOps(pixelOps_)
{
}

template<std::unsigned_integral Pixel>
void Scanline<Pixel>::draw(
	const Pixel* __restrict src1, const Pixel* __restrict src2,
	Pixel* __restrict dst, unsigned factor, size_t width)
{
#ifdef __SSE2__
	drawSSE2(src1, src2, dst, factor, width, pixelOps, darkener);
#else
	// non-SSE2 routine, both 16bpp and 32bpp
	darkener.setFactor(factor);
	for (auto x : xrange(width)) {
		dst[x] = darkener.multiply(
			pixelOps.template blend<1, 1>(src1[x], src2[x]));
	}
#endif
}

template<std::unsigned_integral Pixel>
Pixel Scanline<Pixel>::darken(Pixel p, unsigned factor) const
{
	return darkener.multiply(p, factor);
}

template<std::unsigned_integral Pixel>
Pixel Scanline<Pixel>::darken(Pixel p1, Pixel p2, unsigned factor) const
{
	return darkener.multiply(pixelOps.template blend<1, 1>(p1, p2), factor);
}

// Force template instantiation.
#if HAVE_16BPP
template class Scanline<uint16_t>;
#endif
#if HAVE_32BPP
template class Scanline<uint32_t>;
#endif

} // namespace openmsx
