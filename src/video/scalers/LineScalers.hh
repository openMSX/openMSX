#ifndef LINESCALERS_HH
#define LINESCALERS_HH

#include "PixelOperations.hh"
#include "ranges.hh"
#include "xrange.hh"
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstring>
#include <span>
#include <type_traits>
#ifdef __SSE2__
#include "emmintrin.h"
#endif
#ifdef __SSSE3__
#include "tmmintrin.h"
#endif

namespace openmsx {

// Tag classes
struct TagCopy {};
template<typename CLASS, typename TAG> struct IsTagged
	: std::is_base_of<TAG, CLASS> {};


// Scalers

/**  Scale_XonY functors
 * Transforms an input line of pixel to an output line (possibly) with
 * a different width. X input pixels are mapped on Y output pixels.
 * @param in Input line
 * @param out Output line
 */
template<std::unsigned_integral Pixel> class Scale_1on3
{
public:
	void operator()(std::span<const Pixel> in, std::span<Pixel> out);
};

template<std::unsigned_integral Pixel> class Scale_1on4
{
public:
	void operator()(std::span<const Pixel> in, std::span<Pixel> out);
};

template<std::unsigned_integral Pixel> class Scale_1on6
{
public:
	void operator()(std::span<const Pixel> in, std::span<Pixel> out);
};

template<std::unsigned_integral Pixel> class Scale_1on2
{
public:
	void operator()(std::span<const Pixel> in, std::span<Pixel> out);
};

template<std::unsigned_integral Pixel> class Scale_1on1 : public TagCopy
{
public:
	void operator()(std::span<const Pixel> in, std::span<Pixel> out);
};

template<std::unsigned_integral Pixel> class Scale_2on1
{
public:
	explicit Scale_2on1(PixelOperations<Pixel> pixelOps);
	void operator()(std::span<const Pixel> in, std::span<Pixel> out);
private:
	PixelOperations<Pixel> pixelOps;
};

template<std::unsigned_integral Pixel> class Scale_6on1
{
public:
	explicit Scale_6on1(PixelOperations<Pixel> pixelOps);
	void operator()(std::span<const Pixel> in, std::span<Pixel> out);
private:
	PixelOperations<Pixel> pixelOps;
};

template<std::unsigned_integral Pixel> class Scale_4on1
{
public:
	explicit Scale_4on1(PixelOperations<Pixel> pixelOps);
	void operator()(std::span<const Pixel> in, std::span<Pixel> out);
private:
	PixelOperations<Pixel> pixelOps;
};

template<std::unsigned_integral Pixel> class Scale_3on1
{
public:
	explicit Scale_3on1(PixelOperations<Pixel> pixelOps);
	void operator()(std::span<const Pixel> in, std::span<Pixel> out);
private:
	PixelOperations<Pixel> pixelOps;
};

template<std::unsigned_integral Pixel> class Scale_3on2
{
public:
	explicit Scale_3on2(PixelOperations<Pixel> pixelOps);
	void operator()(std::span<const Pixel> in, std::span<Pixel> out);
private:
	PixelOperations<Pixel> pixelOps;
};

template<std::unsigned_integral Pixel> class Scale_3on4
{
public:
	explicit Scale_3on4(PixelOperations<Pixel> pixelOps);
	void operator()(std::span<const Pixel> in, std::span<Pixel> out);
private:
	PixelOperations<Pixel> pixelOps;
};

template<std::unsigned_integral Pixel> class Scale_3on8
{
public:
	explicit Scale_3on8(PixelOperations<Pixel> pixelOps);
	void operator()(std::span<const Pixel> in, std::span<Pixel> out);
private:
	PixelOperations<Pixel> pixelOps;
};

template<std::unsigned_integral Pixel> class Scale_2on3
{
public:
	explicit Scale_2on3(PixelOperations<Pixel> pixelOps);
	void operator()(std::span<const Pixel> in, std::span<Pixel> out);
private:
	PixelOperations<Pixel> pixelOps;
};

template<std::unsigned_integral Pixel> class Scale_4on3
{
public:
	explicit Scale_4on3(PixelOperations<Pixel> pixelOps);
	void operator()(std::span<const Pixel> in, std::span<Pixel> out);
private:
	PixelOperations<Pixel> pixelOps;
};

template<std::unsigned_integral Pixel> class Scale_8on3
{
public:
	explicit Scale_8on3(PixelOperations<Pixel> pixelOps);
	void operator()(std::span<const Pixel> in, std::span<Pixel> out);
private:
	PixelOperations<Pixel> pixelOps;
};

template<std::unsigned_integral Pixel> class Scale_2on9
{
public:
	explicit Scale_2on9(PixelOperations<Pixel> pixelOps);
	void operator()(std::span<const Pixel> in, std::span<Pixel> out);
private:
	PixelOperations<Pixel> pixelOps;
};

template<std::unsigned_integral Pixel> class Scale_4on9
{
public:
	explicit Scale_4on9(PixelOperations<Pixel> pixelOps);
	void operator()(std::span<const Pixel> in, std::span<Pixel> out);
private:
	PixelOperations<Pixel> pixelOps;
};

template<std::unsigned_integral Pixel> class Scale_8on9
{
public:
	explicit Scale_8on9(PixelOperations<Pixel> pixelOps);
	void operator()(std::span<const Pixel> in, std::span<Pixel> out);
private:
	PixelOperations<Pixel> pixelOps;
};

template<std::unsigned_integral Pixel> class Scale_4on5
{
public:
	explicit Scale_4on5(PixelOperations<Pixel> pixelOps);
	void operator()(std::span<const Pixel> in, std::span<Pixel> out);
private:
	PixelOperations<Pixel> pixelOps;
};

template<std::unsigned_integral Pixel> class Scale_7on8
{
public:
	explicit Scale_7on8(PixelOperations<Pixel> pixelOps);
	void operator()(std::span<const Pixel> in, std::span<Pixel> out);
private:
	PixelOperations<Pixel> pixelOps;
};

template<std::unsigned_integral Pixel> class Scale_17on20
{
public:
	explicit Scale_17on20(PixelOperations<Pixel> pixelOps);
	void operator()(std::span<const Pixel> in, std::span<Pixel> out);
private:
	PixelOperations<Pixel> pixelOps;
};

template<std::unsigned_integral Pixel> class Scale_9on10
{
public:
	explicit Scale_9on10(PixelOperations<Pixel> pixelOps);
	void operator()(std::span<const Pixel> in, std::span<Pixel> out);
private:
	PixelOperations<Pixel> pixelOps;
};


/**  BlendLines functor
 * Generate an output line that is an interpolation of two input lines.
 * @param in1 First input line
 * @param in2 Second input line
 * @param out Output line
 * @param width Width of the lines in pixels
 */
template<std::unsigned_integral Pixel, unsigned w1 = 1, unsigned w2 = 1> class BlendLines
{
public:
	explicit BlendLines(PixelOperations<Pixel> pixelOps);
	void operator()(std::span<const Pixel> in1, std::span<const Pixel> in2,
	                std::span<Pixel> out);
private:
	PixelOperations<Pixel> pixelOps;
};

/** Stretch (or zoom) a given input line to a wider output line.
 */
template<std::unsigned_integral Pixel>
class ZoomLine
{
public:
	explicit ZoomLine(PixelOperations<Pixel> pixelOps);
	void operator()(std::span<const Pixel> in, std::span<Pixel> out) const;
private:
	PixelOperations<Pixel> pixelOps;
};


/**  AlphaBlendLines functor
 * Generate an output line that is a per-pixel-alpha-blend of the two input
 * lines. The first input line contains the alpha-value per pixel.
 * @param in1 First input line
 * @param in2 Second input line
 * @param out Output line
 */
template<std::unsigned_integral Pixel> class AlphaBlendLines
{
public:
	explicit AlphaBlendLines(PixelOperations<Pixel> pixelOps);
	void operator()(std::span<const Pixel> in1, std::span<const Pixel> in2,
	                std::span<Pixel> out);
	void operator()(Pixel in1, std::span<const Pixel> in2,
	                std::span<Pixel> out);
private:
	PixelOperations<Pixel> pixelOps;
};


/** Polymorphic line scaler.
 * Abstract base class for line scalers. Can be used when one basic algorithm
 * should work in combination with multiple line scalers (e.g. several
 * Scale_XonY variants).
 * A line scaler takes one line of input pixels and outputs a different line
 * of pixels. The input and output don't necessarily have the same number of
 * pixels.
 * An alternative (which we used in the past) is to templatize that algorithm
 * on the LineScaler type. In theory this results in a faster routine, but in
 * practice this performance benefit is often not measurable while it does
 * result in bigger code size.
 */
template<std::unsigned_integral Pixel>
class PolyLineScaler
{
public:
	/** Actually scale a line.
	 * @param in Pointer to buffer containing input line.
	 * @param out Pointer to buffer that should be filled with output.
	 * @param outWidth The number of pixels that should be output.
	 * Note: The number of input pixels is not explicitly specified. This
	 *       depends on the actual scaler, for example Scale_2on1 requires
	 *       twice as many pixels in the input than the output.
	 */
	virtual void operator()(std::span<const Pixel> in, std::span<Pixel> out) = 0;

	/** Is this scale operation actually a copy?
	 * This info can be used to (in a multi-step scale operation) immediately
	 * produce the output of the previous step in this step's output buffer,
	 * so effectively skipping this step.
	 */
	[[nodiscard]] virtual bool isCopy() const = 0;

protected:
	~PolyLineScaler() = default;
};

/** Polymorphic wrapper around another line scaler.
 * This version directly contains (and thus constructs) the wrapped Line Scaler.
 */
template<std::unsigned_integral Pixel, typename Scaler>
class PolyScale final : public PolyLineScaler<Pixel>
{
public:
	PolyScale()
		: scaler()
	{
	}
	explicit PolyScale(PixelOperations<Pixel> pixelOps)
		: scaler(pixelOps)
	{
	}
	void operator()(std::span<const Pixel> in, std::span<Pixel> out) override
	{
		scaler(in, out);
	}
	[[nodiscard]] bool isCopy() const override
	{
		return IsTagged<Scaler, TagCopy>::value;
	}
private:
	Scaler scaler;
};

/** Like PolyScale above, but instead keeps a reference to the actual scaler.
 * Can be used when the actual scaler is expensive to construct (e.g. Blur_1on3).
 */
template<std::unsigned_integral Pixel, typename Scaler>
class PolyScaleRef final : public PolyLineScaler<Pixel>
{
public:
	explicit PolyScaleRef(Scaler& scaler_)
		: scaler(scaler_)
	{
	}
	void operator()(std::span<const Pixel> in, std::span<Pixel> out) override
	{
		scaler(in, out);
	}
	[[nodiscard]] bool isCopy() const override
	{
		return IsTagged<Scaler, TagCopy>::value;
	}
private:
	Scaler& scaler;
};


// implementation

template<std::unsigned_integral Pixel, unsigned N>
static inline void scale_1onN(
	std::span<const Pixel> in, std::span<Pixel> out)
{
	auto outWidth = out.size();
	assert(in.size() == (outWidth / N));

	size_t i = 0, j = 0;
	for (/* */; i < (outWidth - (N - 1)); i += N, j += 1) {
		Pixel pix = in[j];
		for (auto k : xrange(N)) {
			out[i + k] = pix;
		}
	}
	for (auto k : xrange(N - 1)) {
		if ((i + k) < outWidth) out[i + k] = 0;
	}
}

template<std::unsigned_integral Pixel>
void Scale_1on3<Pixel>::operator()(std::span<const Pixel> in, std::span<Pixel> out)
{
	scale_1onN<Pixel, 3>(in, out);
}

template<std::unsigned_integral Pixel>
void Scale_1on4<Pixel>::operator()(std::span<const Pixel> in, std::span<Pixel> out)
{
	scale_1onN<Pixel, 4>(in, out);
}

template<std::unsigned_integral Pixel>
void Scale_1on6<Pixel>::operator()(std::span<const Pixel> in, std::span<Pixel> out)
{
	scale_1onN<Pixel, 6>(in, out);
}

#ifdef __SSE2__
template<std::unsigned_integral Pixel> inline __m128i unpacklo(__m128i x, __m128i y)
{
	if constexpr (sizeof(Pixel) == 4) {
		return _mm_unpacklo_epi32(x, y);
	} else if constexpr (sizeof(Pixel) == 2) {
		return _mm_unpacklo_epi16(x, y);
	} else {
		UNREACHABLE;
	}
}
template<std::unsigned_integral Pixel> inline __m128i unpackhi(__m128i x, __m128i y)
{
	if constexpr (sizeof(Pixel) == 4) {
		return _mm_unpackhi_epi32(x, y);
	} else if constexpr (sizeof(Pixel) == 2) {
		return _mm_unpackhi_epi16(x, y);
	} else {
		UNREACHABLE;
	}
}

template<std::unsigned_integral Pixel>
inline void scale_1on2_SSE(const Pixel* __restrict in_, Pixel* __restrict out_, size_t srcWidth)
{
	size_t bytes = srcWidth * sizeof(Pixel);
	assert((bytes % (4 * sizeof(__m128i))) == 0);
	assert(bytes != 0);

	const auto* in  = reinterpret_cast<const char*>(in_)  +     bytes;
	      auto* out = reinterpret_cast<      char*>(out_) + 2 * bytes;

	auto x = -ptrdiff_t(bytes);
	do {
		__m128i a0 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(in + x +  0));
		__m128i a1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(in + x + 16));
		__m128i a2 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(in + x + 32));
		__m128i a3 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(in + x + 48));
		__m128i l0 = unpacklo<Pixel>(a0, a0);
		__m128i h0 = unpackhi<Pixel>(a0, a0);
		__m128i l1 = unpacklo<Pixel>(a1, a1);
		__m128i h1 = unpackhi<Pixel>(a1, a1);
		__m128i l2 = unpacklo<Pixel>(a2, a2);
		__m128i h2 = unpackhi<Pixel>(a2, a2);
		__m128i l3 = unpacklo<Pixel>(a3, a3);
		__m128i h3 = unpackhi<Pixel>(a3, a3);
		_mm_storeu_si128(reinterpret_cast<__m128i*>(out + 2*x +   0), l0);
		_mm_storeu_si128(reinterpret_cast<__m128i*>(out + 2*x +  16), h0);
		_mm_storeu_si128(reinterpret_cast<__m128i*>(out + 2*x +  32), l1);
		_mm_storeu_si128(reinterpret_cast<__m128i*>(out + 2*x +  48), h1);
		_mm_storeu_si128(reinterpret_cast<__m128i*>(out + 2*x +  64), l2);
		_mm_storeu_si128(reinterpret_cast<__m128i*>(out + 2*x +  80), h2);
		_mm_storeu_si128(reinterpret_cast<__m128i*>(out + 2*x +  96), l3);
		_mm_storeu_si128(reinterpret_cast<__m128i*>(out + 2*x + 112), h3);
		x += 4 * sizeof(__m128i);
	} while (x < 0);
}
#endif

template<std::unsigned_integral Pixel>
void Scale_1on2<Pixel>::operator()(
	std::span<const Pixel> in, std::span<Pixel> out)
{
	// This is a fairly simple algorithm (output each input pixel twice).
	// An ideal compiler should generate optimal (vector) code for it.
	// I checked the 2013-05-29 dev snapshots of gcc-4.9 and clang-3.4:
	// - Clang is not able to vectorize this loop. My best tuned C version
	//   of this routine is a little over 4x slower than the tuned
	//   SSE-intrinsics version.
	// - Gcc can auto-vectorize this routine. Though my best tuned version
	//   (I mean tuned to further improve the auto-vectorization, including
	//   using the new __builtin_assume_aligned() intrinsic) still runs
	//   approx 40% slower than the intrinsics version.
	// Hopefully in some years the compilers have improved further so that
	// the intrinsic version is no longer needed.
	auto srcWidth = in.size();
	assert(out.size() == 2 * srcWidth);

#ifdef __SSE2__
	size_t chunk = 4 * sizeof(__m128i) / sizeof(Pixel);
	size_t srcWidth2 = srcWidth & ~(chunk - 1);
	scale_1on2_SSE(in.data(), out.data(), srcWidth2);
	in  = in .subspan(    srcWidth2);
	out = out.subspan(2 * srcWidth2);
	srcWidth -= srcWidth2;
#endif

	// C++ version. Used both on non-x86 machines and (possibly) on x86 for
	// the last few pixels of the line.
	for (auto x : xrange(srcWidth)) {
		out[x * 2] = out[x * 2 + 1] = in[x];
	}
}

#ifdef __SSE2__
// Memcpy-like routine, it can be faster than a generic memcpy because:
// - It requires that both input and output are 16-bytes aligned.
// - It can only copy (non-zero) integer multiples of 128 bytes.
inline void memcpy_SSE_128(
	const void* __restrict in_, void* __restrict out_, size_t size)
{
	assert((reinterpret_cast<size_t>(in_ ) % 16) == 0);
	assert((reinterpret_cast<size_t>(out_) % 16) == 0);
	assert((size % 128) == 0);
	assert(size != 0);

	const auto* in  = reinterpret_cast<const __m128i*>(in_);
	      auto* out = reinterpret_cast<      __m128i*>(out_);
	const auto* end = in + (size / sizeof(__m128i));
	do {
		out[0] = in[0];
		out[1] = in[1];
		out[2] = in[2];
		out[3] = in[3];
		out[4] = in[4];
		out[5] = in[5];
		out[6] = in[6];
		out[7] = in[7];
		in += 8;
		out += 8;
	} while (in != end);
}
#endif

template<std::unsigned_integral Pixel>
void Scale_1on1<Pixel>::operator()(
	std::span<const Pixel> in, std::span<Pixel> out)
{
	assert(in.size() == out.size());
#ifdef __SSE2__
	// When using a very recent gcc/clang, this routine is only about
	// 10% faster than a simple memcpy(). When using gcc-4.6 (still the
	// default on many systems), it's still about 66% faster.
	const auto* inPtr = in.data();
	auto* outPtr = out.data();
	size_t nBytes = in.size() * sizeof(Pixel);
	size_t n128 = nBytes & ~127;
	memcpy_SSE_128(inPtr, outPtr, n128); // copy 128 byte chunks

	nBytes &= 127; // remaining bytes (if any)
	if (nBytes == 0) [[likely]] return;
	inPtr  += n128 / sizeof(Pixel);
	outPtr += n128 / sizeof(Pixel);
	memcpy(outPtr, inPtr, nBytes);
#else
	ranges::copy(in, out);
#endif
}


template<std::unsigned_integral Pixel>
Scale_2on1<Pixel>::Scale_2on1(PixelOperations<Pixel> pixelOps_)
	: pixelOps(pixelOps_)
{
}

#ifdef __SSE2__
template<int IMM8> static inline __m128i shuffle(__m128i x, __m128i y)
{
	return _mm_castps_si128(_mm_shuffle_ps(
		_mm_castsi128_ps(x), _mm_castsi128_ps(y), IMM8));
}

template<std::unsigned_integral Pixel>
inline __m128i blend(__m128i x, __m128i y, Pixel mask)
{
	if constexpr (sizeof(Pixel) == 4) {
		// 32bpp
		__m128i p = shuffle<0x88>(x, y);
		__m128i q = shuffle<0xDD>(x, y);
		return _mm_avg_epu8(p, q);
	} else {
		// 16bpp, first shuffle odd/even pixels in the right position
#ifdef __SSSE3__
		// This can be done faster using SSSE3
		const __m128i LL = _mm_set_epi8(
			0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
			0x0D, 0x0C, 0x09, 0x08, 0x05, 0x04, 0x01, 0x00);
		const __m128i HL = _mm_set_epi8(
			0x0D, 0x0C, 0x09, 0x08, 0x05, 0x04, 0x01, 0x00,
			0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80);
		const __m128i LH = _mm_set_epi8(
			0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
			0x0F, 0x0E, 0x0B, 0x0A, 0x07, 0x06, 0x03, 0x02);
		const __m128i HH = _mm_set_epi8(
			0x0F, 0x0E, 0x0B, 0x0A, 0x07, 0x06, 0x03, 0x02,
			0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80);
		__m128i ll = _mm_shuffle_epi8(x, LL);
		__m128i hl = _mm_shuffle_epi8(y, HL);
		__m128i lh = _mm_shuffle_epi8(x, LH);
		__m128i hh = _mm_shuffle_epi8(y, HH);
		__m128i p = _mm_or_si128(ll, hl);
		__m128i q = _mm_or_si128(lh, hh);
#else
		// For SSE2 this only generates 1 instruction more, but with
		// longer dependency chains
		__m128i s = _mm_unpacklo_epi16(x, y);
		__m128i t = _mm_unpackhi_epi16(x, y);
		__m128i u = _mm_unpacklo_epi16(s, t);
		__m128i v = _mm_unpackhi_epi16(s, t);
		__m128i p = _mm_unpacklo_epi16(u, v);
		__m128i q = _mm_unpackhi_epi16(u, v);
#endif
		// Actually blend: (p & q) + (((p ^ q) & mask) >> 1)
		__m128i m = _mm_set1_epi16(mask);
		__m128i a = _mm_and_si128(p, q);
		__m128i b = _mm_xor_si128(p, q);
		__m128i c = _mm_and_si128(b, m);
		__m128i d = _mm_srli_epi16(c, 1);
		return _mm_add_epi16(a, d);
	}
}

template<std::unsigned_integral Pixel>
inline void scale_2on1_SSE(
	const Pixel* __restrict in_, Pixel* __restrict out_, size_t dstBytes,
	Pixel mask)
{
	assert((dstBytes % (4 * sizeof(__m128i))) == 0);
	assert(dstBytes != 0);

	const auto* in  = reinterpret_cast<const char*>(in_)  + 2 * dstBytes;
	      auto* out = reinterpret_cast<      char*>(out_) +     dstBytes;

	auto x = -ptrdiff_t(dstBytes);
	do {
		__m128i a0 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(in + 2*x +   0));
		__m128i a1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(in + 2*x +  16));
		__m128i a2 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(in + 2*x +  32));
		__m128i a3 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(in + 2*x +  48));
		__m128i a4 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(in + 2*x +  64));
		__m128i a5 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(in + 2*x +  80));
		__m128i a6 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(in + 2*x +  96));
		__m128i a7 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(in + 2*x + 112));
		__m128i b0 = blend(a0, a1, mask);
		__m128i b1 = blend(a2, a3, mask);
		__m128i b2 = blend(a4, a5, mask);
		__m128i b3 = blend(a6, a7, mask);
		_mm_storeu_si128(reinterpret_cast<__m128i*>(out + x +  0), b0);
		_mm_storeu_si128(reinterpret_cast<__m128i*>(out + x + 16), b1);
		_mm_storeu_si128(reinterpret_cast<__m128i*>(out + x + 32), b2);
		_mm_storeu_si128(reinterpret_cast<__m128i*>(out + x + 48), b3);
		x += 4 * sizeof(__m128i);
	} while (x < 0);
}
#endif

template<std::unsigned_integral Pixel>
void Scale_2on1<Pixel>::operator()(
	std::span<const Pixel> in, std::span<Pixel> out)
{
	assert(in.size() == 2 * out.size());
	auto outWidth = out.size();
#ifdef __SSE2__
	auto n64 = (outWidth * sizeof(Pixel)) & ~63;
	Pixel mask = pixelOps.getBlendMask();
	scale_2on1_SSE(in.data(), out.data(), n64, mask); // process 64 byte chunks
	outWidth &= ((64 / sizeof(Pixel)) - 1); // remaining pixels (if any)
	if (outWidth == 0) [[likely]] return;
	in  = in .subspan(2 * n64 / sizeof(Pixel));
	out = out.subspan(    n64 / sizeof(Pixel));
	// fallthrough to c++ version
#endif
	// pure C++ version
	for (auto i : xrange(outWidth)) {
		out[i] = pixelOps.template blend<1, 1>(
			in[2 * i + 0], in[2 * i + 1]);
	}
}


template<std::unsigned_integral Pixel>
Scale_6on1<Pixel>::Scale_6on1(PixelOperations<Pixel> pixelOps_)
	: pixelOps(pixelOps_)
{
}

template<std::unsigned_integral Pixel>
void Scale_6on1<Pixel>::operator()(
	std::span<const Pixel> in, std::span<Pixel> out)
{
	assert(in.size() == 6 * out.size());
	for (auto i : xrange(out.size())) {
		out[i] = pixelOps.template blend<1, 1, 1, 1, 1, 1>(subspan<6>(in, 6 * i));
	}
}


template<std::unsigned_integral Pixel>
Scale_4on1<Pixel>::Scale_4on1(PixelOperations<Pixel> pixelOps_)
	: pixelOps(pixelOps_)
{
}

template<std::unsigned_integral Pixel>
void Scale_4on1<Pixel>::operator()(
	std::span<const Pixel> in, std::span<Pixel> out)
{
	assert(in.size() == 4 * out.size());
	for (auto i : xrange(out.size())) {
		out[i] = pixelOps.template blend<1, 1, 1, 1>(subspan<4>(in, 4 * i));
	}
}


template<std::unsigned_integral Pixel>
Scale_3on1<Pixel>::Scale_3on1(PixelOperations<Pixel> pixelOps_)
	: pixelOps(pixelOps_)
{
}

template<std::unsigned_integral Pixel>
void Scale_3on1<Pixel>::operator()(
	std::span<const Pixel> in, std::span<Pixel> out)
{
	assert(in.size() == 3 * out.size());
	for (auto i : xrange(out.size())) {
		out[i] = pixelOps.template blend<1, 1, 1>(subspan<3>(in, 3 * i));
	}
}


template<std::unsigned_integral Pixel>
Scale_3on2<Pixel>::Scale_3on2(PixelOperations<Pixel> pixelOps_)
	: pixelOps(pixelOps_)
{
}

template<std::unsigned_integral Pixel>
void Scale_3on2<Pixel>::operator()(
	std::span<const Pixel> in, std::span<Pixel> out)
{
	assert((in.size() / 3) == (out.size() / 2));
	size_t n = out.size();
	size_t i = 0, j = 0;
	for (/* */; i < (n - 1); i += 2, j += 3) {
		out[i + 0] = pixelOps.template blend<2, 1>(subspan<2>(in, j + 0));
		out[i + 1] = pixelOps.template blend<1, 2>(subspan<2>(in, j + 1));
	}
	if (i < n) out[i] = 0;
}


template<std::unsigned_integral Pixel>
Scale_3on4<Pixel>::Scale_3on4(PixelOperations<Pixel> pixelOps_)
	: pixelOps(pixelOps_)
{
}

template<std::unsigned_integral Pixel>
void Scale_3on4<Pixel>::operator()(
	std::span<const Pixel> in, std::span<Pixel> out)
{
	assert((in.size() / 3) == (out.size() / 4));
	size_t n = out.size();
	size_t i = 0, j = 0;
	for (/* */; i < (n - 3); i += 4, j += 3) {
		out[i + 0] = in[j + 0];
		out[i + 1] = pixelOps.template blend<1, 2>(subspan<2>(in, j + 0));
		out[i + 2] = pixelOps.template blend<2, 1>(subspan<2>(in, j + 1));
		out[i + 3] = in[j + 2];
	}
	for (auto k : xrange(4 - 1)) {
		if ((i + k) < n) out[i + k] = 0;
	}
}


template<std::unsigned_integral Pixel>
Scale_3on8<Pixel>::Scale_3on8(PixelOperations<Pixel> pixelOps_)
	: pixelOps(pixelOps_)
{
}

template<std::unsigned_integral Pixel>
void Scale_3on8<Pixel>::operator()(
	std::span<const Pixel> in, std::span<Pixel> out)
{
	assert((in.size() / 3) == (out.size() / 8));
	size_t n = out.size();
	size_t i = 0, j = 0;
	for (/* */; i < (n - 7); i += 8, j += 3) {
		out[i + 0] = in[j + 0];
		out[i + 1] = in[j + 0];
		out[i + 2] = pixelOps.template blend<2, 1>(subspan<2>(in, j + 0));
		out[i + 3] = in[j + 1];
		out[i + 4] = in[j + 1];
		out[i + 5] = pixelOps.template blend<1, 2>(subspan<2>(in, j + 1));
		out[i + 6] = in[j + 2];
		out[i + 7] = in[j + 2];
	}
	for (auto k : xrange(8 - 1)) {
		if ((i + k) < n) out[i + k] = 0;
	}
}


template<std::unsigned_integral Pixel>
Scale_2on3<Pixel>::Scale_2on3(PixelOperations<Pixel> pixelOps_)
	: pixelOps(pixelOps_)
{
}

template<std::unsigned_integral Pixel>
void Scale_2on3<Pixel>::operator()(
	std::span<const Pixel> in, std::span<Pixel> out)
{
	assert((in.size() / 2) == (out.size() / 3));
	size_t n = out.size();
	size_t i = 0, j = 0;
	for (/* */; i < (n - 2); i += 3, j += 2) {
		out[i + 0] = in[j + 0];
		out[i + 1] = pixelOps.template blend<1, 1>(subspan<2>(in, j));
		out[i + 2] = in[j + 1];
	}
	if ((i + 0) < n) out[i + 0] = 0;
	if ((i + 1) < n) out[i + 1] = 0;
}


template<std::unsigned_integral Pixel>
Scale_4on3<Pixel>::Scale_4on3(PixelOperations<Pixel> pixelOps_)
	: pixelOps(pixelOps_)
{
}

template<std::unsigned_integral Pixel>
void Scale_4on3<Pixel>::operator()(
	std::span<const Pixel> in, std::span<Pixel> out)
{
	assert((in.size() / 4) == (out.size() / 3));
	size_t n = out.size();
	size_t i = 0, j = 0;
	for (/* */; i < (n - 2); i += 3, j += 4) {
		out[i + 0] = pixelOps.template blend<3, 1>(subspan<2>(in, j + 0));
		out[i + 1] = pixelOps.template blend<1, 1>(subspan<2>(in, j + 1));
		out[i + 2] = pixelOps.template blend<1, 3>(subspan<2>(in, j + 2));
	}
	if ((i + 0) < n) out[i + 0] = 0;
	if ((i + 1) < n) out[i + 1] = 0;
}


template<std::unsigned_integral Pixel>
Scale_8on3<Pixel>::Scale_8on3(PixelOperations<Pixel> pixelOps_)
	: pixelOps(pixelOps_)
{
}

template<std::unsigned_integral Pixel>
void Scale_8on3<Pixel>::operator()(
	std::span<const Pixel> in, std::span<Pixel> out)
{
	assert((in.size() / 8) == (out.size() / 3));
	size_t n = out.size();
	size_t i = 0, j = 0;
	for (/* */; i < (n - 2); i += 3, j += 8) {
		out[i + 0] = pixelOps.template blend<3, 3, 2>   (subspan<3>(in, j + 0));
		out[i + 1] = pixelOps.template blend<1, 3, 3, 1>(subspan<4>(in, j + 2));
		out[i + 2] = pixelOps.template blend<2, 3, 3>   (subspan<3>(in, j + 5));
	}
	if ((i + 0) < n) out[i + 0] = 0;
	if ((i + 1) < n) out[i + 1] = 0;
}


template<std::unsigned_integral Pixel>
Scale_2on9<Pixel>::Scale_2on9(PixelOperations<Pixel> pixelOps_)
	: pixelOps(pixelOps_)
{
}

template<std::unsigned_integral Pixel>
void Scale_2on9<Pixel>::operator()(
	std::span<const Pixel> in, std::span<Pixel> out)
{
	assert((in.size() / 2) == (out.size() / 9));
	size_t n = out.size();
	size_t i = 0, j = 0;
	for (/* */; i < (n - 8); i += 9, j += 2) {
		out[i + 0] = in[j + 0];
		out[i + 1] = in[j + 0];
		out[i + 2] = in[j + 0];
		out[i + 3] = in[j + 0];
		out[i + 4] = pixelOps.template blend<1, 1>(subspan<2>(in, j));
		out[i + 5] = in[j + 1];
		out[i + 6] = in[j + 1];
		out[i + 7] = in[j + 1];
		out[i + 8] = in[j + 1];
	}
	if ((i + 0) < n) out[i + 0] = 0;
	if ((i + 1) < n) out[i + 1] = 0;
	if ((i + 2) < n) out[i + 2] = 0;
	if ((i + 3) < n) out[i + 3] = 0;
	if ((i + 4) < n) out[i + 4] = 0;
	if ((i + 5) < n) out[i + 5] = 0;
	if ((i + 6) < n) out[i + 6] = 0;
	if ((i + 7) < n) out[i + 7] = 0;
}


template<std::unsigned_integral Pixel>
Scale_4on9<Pixel>::Scale_4on9(PixelOperations<Pixel> pixelOps_)
	: pixelOps(pixelOps_)
{
}

template<std::unsigned_integral Pixel>
void Scale_4on9<Pixel>::operator()(
	std::span<const Pixel> in, std::span<Pixel> out)
{
	assert((in.size() / 4) == (out.size() / 9));
	size_t n = out.size();
	size_t i = 0, j = 0;
	for (/* */; i < (n - 8); i += 9, j += 4) {
		out[i + 0] = in[j + 0];
		out[i + 1] = in[j + 0];
		out[i + 2] = pixelOps.template blend<1, 3>(subspan<2>(in, j + 0));
		out[i + 3] = in[j + 1];
		out[i + 4] = pixelOps.template blend<1, 1>(subspan<2>(in, j + 1));
		out[i + 5] = in[j + 2];
		out[i + 6] = pixelOps.template blend<3, 1>(subspan<2>(in, j + 2));
		out[i + 7] = in[j + 3];
		out[i + 8] = in[j + 3];
	}
	if ((i + 0) < n) out[i + 0] = 0;
	if ((i + 1) < n) out[i + 1] = 0;
	if ((i + 2) < n) out[i + 2] = 0;
	if ((i + 3) < n) out[i + 3] = 0;
	if ((i + 4) < n) out[i + 4] = 0;
	if ((i + 5) < n) out[i + 5] = 0;
	if ((i + 6) < n) out[i + 6] = 0;
	if ((i + 7) < n) out[i + 7] = 0;
}


template<std::unsigned_integral Pixel>
Scale_8on9<Pixel>::Scale_8on9(PixelOperations<Pixel> pixelOps_)
	: pixelOps(pixelOps_)
{
}

template<std::unsigned_integral Pixel>
void Scale_8on9<Pixel>::operator()(
	std::span<const Pixel> in, std::span<Pixel> out)
{
	assert((in.size() / 8) == (out.size() / 9));
	size_t n = out.size();
	size_t i = 0, j = 0;
	for (/* */; i < (n - 8); i += 9, j += 8) {
		out[i + 0] = in[j + 0];
		out[i + 1] = pixelOps.template blend<1, 7>(subspan<2>(in, j + 0));
		out[i + 2] = pixelOps.template blend<1, 3>(subspan<2>(in, j + 1));
		out[i + 3] = pixelOps.template blend<3, 5>(subspan<2>(in, j + 2));
		out[i + 4] = pixelOps.template blend<1, 1>(subspan<2>(in, j + 3));
		out[i + 5] = pixelOps.template blend<5, 3>(subspan<2>(in, j + 4));
		out[i + 6] = pixelOps.template blend<3, 1>(subspan<2>(in, j + 5));
		out[i + 7] = pixelOps.template blend<7, 1>(subspan<2>(in, j + 6));
		out[i + 8] = in[j + 7];
	}
	if ((i + 0) < n) out[i + 0] = 0;
	if ((i + 1) < n) out[i + 1] = 0;
	if ((i + 2) < n) out[i + 2] = 0;
	if ((i + 3) < n) out[i + 3] = 0;
	if ((i + 4) < n) out[i + 4] = 0;
	if ((i + 5) < n) out[i + 5] = 0;
	if ((i + 6) < n) out[i + 6] = 0;
	if ((i + 7) < n) out[i + 7] = 0;
}


template<std::unsigned_integral Pixel>
Scale_4on5<Pixel>::Scale_4on5(PixelOperations<Pixel> pixelOps_)
	: pixelOps(pixelOps_)
{
}

template<std::unsigned_integral Pixel>
void Scale_4on5<Pixel>::operator()(
	std::span<const Pixel> in, std::span<Pixel> out)
{
	assert((in.size() / 4) == (out.size() / 5));
	size_t n = out.size();
	assert((n % 5) == 0);
	for (size_t i = 0, j = 0; i < n; i += 5, j += 4) {
		out[i + 0] = in[j + 0];
		out[i + 1] = pixelOps.template blend<1, 3>(subspan<2>(in, j + 0));
		out[i + 2] = pixelOps.template blend<1, 1>(subspan<2>(in, j + 1));
		out[i + 3] = pixelOps.template blend<3, 1>(subspan<2>(in, j + 2));
		out[i + 4] = in[j + 3];
	}
}


template<std::unsigned_integral Pixel>
Scale_7on8<Pixel>::Scale_7on8(PixelOperations<Pixel> pixelOps_)
	: pixelOps(pixelOps_)
{
}

template<std::unsigned_integral Pixel>
void Scale_7on8<Pixel>::operator()(
	std::span<const Pixel> in, std::span<Pixel> out)
{
	assert((in.size() / 7) == (out.size() / 8));
	size_t n = out.size();
	assert((n % 8) == 0);
	for (size_t i = 0, j = 0; i < n; i += 8, j += 7) {
		out[i + 0] = in[j + 0];
		out[i + 1] = pixelOps.template blend<1, 6>(subspan<2>(in, j + 0));
		out[i + 2] = pixelOps.template blend<2, 5>(subspan<2>(in, j + 1));
		out[i + 3] = pixelOps.template blend<3, 4>(subspan<2>(in, j + 2));
		out[i + 4] = pixelOps.template blend<4, 3>(subspan<2>(in, j + 3));
		out[i + 5] = pixelOps.template blend<5, 2>(subspan<2>(in, j + 4));
		out[i + 6] = pixelOps.template blend<6, 1>(subspan<2>(in, j + 5));
		out[i + 7] = in[j + 6];
	}
}


template<std::unsigned_integral Pixel>
Scale_17on20<Pixel>::Scale_17on20(PixelOperations<Pixel> pixelOps_)
	: pixelOps(pixelOps_)
{
}

template<std::unsigned_integral Pixel>
void Scale_17on20<Pixel>::operator()(
	std::span<const Pixel> in, std::span<Pixel> out)
{
	assert((in.size() / 17) == (out.size() / 20));
	size_t n = out.size();
	assert((n % 20) == 0);
	for (size_t i = 0, j = 0; i < n; i += 20, j += 17) {
		out[i +  0] = in[j +  0];
		out[i +  1] = pixelOps.template blend< 3, 14>(subspan<2>(in, j +  0));
		out[i +  2] = pixelOps.template blend< 6, 11>(subspan<2>(in, j +  1));
		out[i +  3] = pixelOps.template blend< 9,  8>(subspan<2>(in, j +  2));
		out[i +  4] = pixelOps.template blend<12,  5>(subspan<2>(in, j +  3));
		out[i +  5] = pixelOps.template blend<15,  2>(subspan<2>(in, j +  4));
		out[i +  6] = in[j +  5];
		out[i +  7] = pixelOps.template blend< 1, 16>(subspan<2>(in, j +  5));
		out[i +  8] = pixelOps.template blend< 4, 13>(subspan<2>(in, j +  6));
		out[i +  9] = pixelOps.template blend< 7, 10>(subspan<2>(in, j +  7));
		out[i + 10] = pixelOps.template blend<10,  7>(subspan<2>(in, j +  8));
		out[i + 11] = pixelOps.template blend<13,  4>(subspan<2>(in, j +  9));
		out[i + 12] = pixelOps.template blend<16,  1>(subspan<2>(in, j + 10));
		out[i + 13] = in[j + 11];
		out[i + 14] = pixelOps.template blend< 2, 15>(subspan<2>(in, j + 11));
		out[i + 15] = pixelOps.template blend< 5, 12>(subspan<2>(in, j + 12));
		out[i + 16] = pixelOps.template blend< 8,  9>(subspan<2>(in, j + 13));
		out[i + 17] = pixelOps.template blend<11,  6>(subspan<2>(in, j + 14));
		out[i + 18] = pixelOps.template blend<14,  3>(subspan<2>(in, j + 15));
		out[i + 19] = in[j + 16];
	}
}


template<std::unsigned_integral Pixel>
Scale_9on10<Pixel>::Scale_9on10(PixelOperations<Pixel> pixelOps_)
	: pixelOps(pixelOps_)
{
}

template<std::unsigned_integral Pixel>
void Scale_9on10<Pixel>::operator()(
	std::span<const Pixel> in, std::span<Pixel> out)
{
	assert((in.size() / 9) == (out.size() / 10));
	size_t n = out.size();
	assert((n % 10) == 0);
	for (size_t i = 0, j = 0; i < n; i += 10, j += 9) {
		out[i + 0] = in[j + 0];
		out[i + 1] = pixelOps.template blend<1, 8>(subspan<2>(in, j + 0));
		out[i + 2] = pixelOps.template blend<2, 7>(subspan<2>(in, j + 1));
		out[i + 3] = pixelOps.template blend<3, 6>(subspan<2>(in, j + 2));
		out[i + 4] = pixelOps.template blend<4, 5>(subspan<2>(in, j + 3));
		out[i + 5] = pixelOps.template blend<5, 4>(subspan<2>(in, j + 4));
		out[i + 6] = pixelOps.template blend<6, 3>(subspan<2>(in, j + 5));
		out[i + 7] = pixelOps.template blend<7, 2>(subspan<2>(in, j + 6));
		out[i + 8] = pixelOps.template blend<8, 1>(subspan<2>(in, j + 7));
		out[i + 9] = in[j + 8];
	}
}


template<std::unsigned_integral Pixel, unsigned w1, unsigned w2>
BlendLines<Pixel, w1, w2>::BlendLines(PixelOperations<Pixel> pixelOps_)
	: pixelOps(pixelOps_)
{
}

template<std::unsigned_integral Pixel, unsigned w1, unsigned w2>
void BlendLines<Pixel, w1, w2>::operator()(
	std::span<const Pixel> in1, std::span<const Pixel> in2, std::span<Pixel> out)
{
	// It _IS_ allowed that the output is the same as one of the inputs.
	// TODO SSE optimizations
	// pure C++ version
	assert(in1.size() == in2.size());
	assert(in1.size() == out.size());
	for (auto i : xrange(in1.size())) { // TODO zip_equal()
		out[i] = pixelOps.template blend<w1, w2>(in1[i], in2[i]);
	}
}


template<std::unsigned_integral Pixel>
ZoomLine<Pixel>::ZoomLine(PixelOperations<Pixel> pixelOps_)
	: pixelOps(pixelOps_)
{
}

template<std::unsigned_integral Pixel>
void ZoomLine<Pixel>::operator()(
	std::span<const Pixel> in, std::span<Pixel> out) const
{
	constexpr unsigned FACTOR = 256;

	unsigned step = FACTOR * in.size() / out.size();
	unsigned i = 0 * FACTOR;
	for (auto o : xrange(out.size())) {
		Pixel p0 = in[(i / FACTOR) + 0];
		Pixel p1 = in[(i / FACTOR) + 1];
		out[o] = pixelOps.lerp(p0, p1, i % FACTOR);
		i += step;
	}
}


template<std::unsigned_integral Pixel>
AlphaBlendLines<Pixel>::AlphaBlendLines(PixelOperations<Pixel> pixelOps_)
	: pixelOps(pixelOps_)
{
}

template<std::unsigned_integral Pixel>
void AlphaBlendLines<Pixel>::operator()(
	std::span<const Pixel> in1, std::span<const Pixel> in2, std::span<Pixel> out)
{
	// It _IS_ allowed that the output is the same as one of the inputs.
	assert(in1.size() == in2.size());
	assert(in1.size() == out.size());
	for (auto i : xrange(in1.size())) { // TODO zip_equal()
		out[i] = pixelOps.alphaBlend(in1[i], in2[i]);
	}
}

template<std::unsigned_integral Pixel>
void AlphaBlendLines<Pixel>::operator()(
	Pixel in1, std::span<const Pixel> in2, std::span<Pixel> out)
{
	// It _IS_ allowed that the output is the same as the input.

	// ATM this routine is only called when 'in1' is not fully opaque nor
	// fully transparent. This cannot happen in 16bpp modes.
	assert(sizeof(Pixel) == 4);
	assert(in2.size() == out.size());

	unsigned alpha = pixelOps.alpha(in1);

	// When one of the two colors is loop-invariant, using the
	// pre-multiplied-alpha-blending equation is a tiny bit more efficient
	// than using alphaBlend() or even lerp().
	//    for (auto i : xrange(width)) {
	//        out[i] = pixelOps.lerp(in1, in2[i], alpha);
	//    }
	Pixel in1M = pixelOps.multiply(in1, alpha);
	unsigned alpha2 = 256 - alpha;
	for (auto i : xrange(in2.size())) { // TODO zip_equal()
		out[i] = in1M + pixelOps.multiply(in2[i], alpha2);
	}
}

} // namespace openmsx

#endif
