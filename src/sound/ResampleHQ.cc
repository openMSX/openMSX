// Based on libsamplerate-0.1.2 (aka Secret Rabbit Code)
//
//  simplified code in several ways:
//   - resample algorithm is no longer switchable, we took this variant:
//        Band limited sinc interpolation, fastest, 97dB SNR, 80% BW
//   - don't allow to change sample rate on-the-fly
//   - assume input (and thus also output) signals have infinite length, so
//     there is no special code to handle the ending of the signal
//   - changed/simplified API to better match openmsx use model
//     (e.g. remove all error checking)

#include "ResampleHQ.hh"

#include "ResampledSoundDevice.hh"

#include "FixedPoint.hh"
#include "Math.hh"
#include "MemBuffer.hh"
#include "aligned.hh"
#include "narrow.hh"
#include "ranges.hh"
#include "small_buffer.hh"
#include "stl.hh"
#include "xrange.hh"

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <vector>
#ifdef __SSE2__
#include <emmintrin.h>
#endif

namespace openmsx {

// Letting the compiler deduce the (type and) size of the std::array works fine
// with gcc, msvc and clang-11 but is broken from clang-12 onwards. More
// specifically it only works for sizes upto 256. For more details see:
//   https://www.mail-archive.com/llvm-bugs@lists.llvm.org/msg50598.html
//   https://reviews.llvm.org/D86936
// As a workaround we do hardcode the (type and) size here:
//static constexpr std::array coeffs = {
static constexpr std::array<float, 2463 + 1> coeffs = {
	#include "ResampleCoeffs.ii"
};

using FilterIndex = FixedPoint<16>;

static constexpr int INDEX_INC = 128;
static constexpr int COEFF_LEN = int(std::size(coeffs) - 1);
static constexpr int COEFF_HALF_LEN = COEFF_LEN - 1;
static constexpr size_t TAB_LEN      = ResampleHQ<1>::TAB_LEN;
static constexpr size_t HALF_TAB_LEN = ResampleHQ<1>::HALF_TAB_LEN;

class ResampleCoeffs
{
public:
	ResampleCoeffs(const ResampleCoeffs&) = delete;
	ResampleCoeffs(ResampleCoeffs&&) = delete;
	ResampleCoeffs& operator=(const ResampleCoeffs&) = delete;
	ResampleCoeffs& operator=(ResampleCoeffs&&) = delete;

	static ResampleCoeffs& instance();
	void getCoeffs(double ratio, float*& table, unsigned& filterLen);
	void releaseCoeffs(double ratio);

private:
	using Table = MemBuffer<float, SSE_ALIGNMENT>;

	ResampleCoeffs() = default;
	~ResampleCoeffs();

	static Table calcTable(double ratio, unsigned& filterLen);

	struct Element {
		double ratio;
		Table table;
		unsigned filterLen;
		unsigned count;
	};
	std::vector<Element> cache; // typically 1-4 entries -> unsorted vector
};

ResampleCoeffs::~ResampleCoeffs()
{
	assert(cache.empty());
}

ResampleCoeffs& ResampleCoeffs::instance()
{
	static ResampleCoeffs resampleCoeffs;
	return resampleCoeffs;
}

void ResampleCoeffs::getCoeffs(double ratio, float*& table, unsigned& filterLen)
{
	if (auto it = std::ranges::find(cache, ratio, &Element::ratio);
	    it != end(cache)) {
		table     = it->table.data();
		filterLen = it->filterLen;
		it->count++;
		return;
	}
	Element elem;
	elem.ratio = ratio;
	elem.count = 1;
	elem.table = calcTable(ratio, elem.filterLen);
	table     = elem.table.data();
	filterLen = elem.filterLen;
	cache.push_back(std::move(elem));
}

void ResampleCoeffs::releaseCoeffs(double ratio)
{
	auto it = rfind_unguarded(cache, ratio, &Element::ratio);
	it->count--;
	if (it->count == 0) {
		move_pop_back(cache, it);
	}
}

static constexpr double getCoeff(FilterIndex index)
{
	double fraction = index.fractionAsDouble();
	int indx = index.toInt();
	return double(coeffs[indx]) +
	       fraction * (double(coeffs[indx + 1]) - double(coeffs[indx]));
}

ResampleCoeffs::Table ResampleCoeffs::calcTable(double ratio, unsigned& filterLen)
{
	double floatIncr = (ratio > 1.0) ? INDEX_INC / ratio : INDEX_INC;
	double normFactor = floatIncr / INDEX_INC;
	auto increment = FilterIndex(floatIncr);
	FilterIndex maxFilterIndex(COEFF_HALF_LEN);

	int min_idx = -maxFilterIndex.divAsInt(increment);
	int max_idx = 1 + (maxFilterIndex - (increment - FilterIndex(floatIncr))).divAsInt(increment);
	int idx_cnt = max_idx - min_idx + 1;
	filterLen = (idx_cnt + 3) & ~3; // round up to multiple of 4
	min_idx -= (narrow<int>(filterLen) - idx_cnt) / 2;
	// Row 't' holds the filter for (sub-sample) phase (t + 0.5) / TAB_LEN.
	// Rows are stored in order, so the two rows that enclose some phase
	// are adjacent in memory. Only the rows of the first half are
	// calculated (row TAB_LEN-1-t is row t mirrored), plus one extra row
	// on both sides (rows -1 and HALF_TAB_LEN) so that the two rows are
	// _always_ adjacent (see calcOutput()). Element 0 of the table is the
	// start of row -1.
	Table table((HALF_TAB_LEN + 2) * filterLen);
	std::ranges::fill(std::span{table}, 0.0f);

	for (auto t : xrange(HALF_TAB_LEN)) {
		float* tab = &table[(t + 1) * filterLen];
		double lastPos = (double(t) + 0.5) / TAB_LEN;
		FilterIndex startFilterIndex(lastPos * floatIncr);

		FilterIndex filterIndex(startFilterIndex);
		int coeffCount = (maxFilterIndex - filterIndex).divAsInt(increment);
		filterIndex += increment * coeffCount;
		int bufIndex = -coeffCount;
		do {
			tab[bufIndex - min_idx] =
				float(getCoeff(filterIndex) * normFactor);
			filterIndex -= increment;
			bufIndex += 1;
		} while (filterIndex >= FilterIndex(0));

		filterIndex = increment - startFilterIndex;
		coeffCount = (maxFilterIndex - filterIndex).divAsInt(increment);
		filterIndex += increment * coeffCount;
		bufIndex = 1 + coeffCount;
		do {
			tab[bufIndex - min_idx] =
				float(getCoeff(filterIndex) * normFactor);
			filterIndex -= increment;
			bufIndex -= 1;
		} while (filterIndex > FilterIndex(0));
	}

	// Row HALF_TAB_LEN is row HALF_TAB_LEN-1 mirrored.
	{
		const float* src = &table[HALF_TAB_LEN * filterLen];
		float* dst = &table[(HALF_TAB_LEN + 1) * filterLen];
		for (auto i : xrange(filterLen)) {
			dst[i] = src[filterLen - 1 - i];
		}
	}
	// Row -1 is the mirror of row TAB_LEN, which is row 0 applied one
	// sample later: row 0 reversed and shifted by one tap. This drops the
	// outermost tap of the window, but that coefficient is ~1e-7 (-137dB).
	{
		const float* src = &table[1 * filterLen];
		float* dst = &table[0];
		for (auto i : xrange(filterLen - 1)) {
			dst[i] = src[filterLen - 2 - i];
		}
		dst[filterLen - 1] = 0.0f;
	}
	return table;
}

template<unsigned CHANNELS>
ResampleHQ<CHANNELS>::ResampleHQ(
		ResampledSoundDevice& input_, const DynamicClock& hostClock_)
	: ResampleAlgo(input_)
	, hostClock(hostClock_)
	, ratio(float(hostClock.getPeriod().toDouble() / getEmuClock().getPeriod().toDouble()))
	, step(uint64_t((hostClock.getPeriod().toDouble() / getEmuClock().getPeriod().toDouble()) * 4294967296.0 + 0.5))
{
	ResampleCoeffs::instance().getCoeffs(double(ratio), table, filterLen);

	// fill buffer with 'enough' zero's
	unsigned extra = filterLen + 1 + narrow_cast<int>(ratio) + 1;
	bufStart = 0;
	bufEnd   = extra;
	size_t initialSize = 4000; // buffer grows dynamically if this is too small
	buffer.resize((initialSize + extra) * CHANNELS); // zero-initialized
}

template<unsigned CHANNELS>
ResampleHQ<CHANNELS>::~ResampleHQ()
{
	ResampleCoeffs::instance().releaseCoeffs(double(ratio));
}

// -- The inner loop: a dot-product between a row of the coefficient table and
//    the (mono or interleaved-stereo) sample buffer.
//
// The table row is read in two ways:
//  * 'forward':  out[ch] = sum(i=0..n-1) tab[i]     * buf[CHANNELS*i + ch]
//  * 'reverse':  out[ch] = sum(i=0..n-1) tab[-1-i]  * buf[CHANNELS*i + ch]
// In both cases we walk the table row with ascending addresses. Each call
// typically needs a different row (the rows are 0.5-3KB and the full table is
// up to 1.5MB, so it mostly streams from L2/L3), and the hardware prefetcher
// handles ascending streams a lot better than descending ones. The sample
// buffer is small and L1-hot, so for the 'reverse' case we reverse that one
// instead.
//
// On gcc/clang we use vector extensions: 4-wide (SSE2, NEON) or 8-wide (AVX).
// This generates code that is as good as hand written intrinsics, but it's
// portable across x86 and ARM. Other compilers (MSVC) get SSE2 intrinsics, or
// as a last resort a plain C++ loop.

#if (defined(__GNUC__) || defined(__clang__)) && !defined(RESAMPLE_HQ_NO_VECTOR_EXT)
#define RESAMPLE_HQ_VECTOR_EXT 1
#endif

#ifdef RESAMPLE_HQ_VECTOR_EXT

using Vec4 = float __attribute__((vector_size(16)));
#ifdef __AVX__
using Vec = float __attribute__((vector_size(32)));
#else
using Vec = Vec4;
#endif
static constexpr size_t VEC_LEN = sizeof(Vec) / sizeof(float);

// reverse the order of the elements
[[nodiscard]] static inline Vec4 vreverse(Vec4 v)
{
	return __builtin_shufflevector(v, v, 3, 2, 1, 0);
}
// duplicate each element: [a b c d] -> [a a b b] and [c c d d]
[[nodiscard]] static inline Vec4 vdupLo(Vec4 v)
{
	return __builtin_shufflevector(v, v, 0, 0, 1, 1);
}
[[nodiscard]] static inline Vec4 vdupHi(Vec4 v)
{
	return __builtin_shufflevector(v, v, 2, 2, 3, 3);
}
// reversed duplicate: [a b c d] -> [d d c c] and [b b a a]
[[nodiscard]] static inline Vec4 vdupRevLo(Vec4 v)
{
	return __builtin_shufflevector(v, v, 3, 3, 2, 2);
}
[[nodiscard]] static inline Vec4 vdupRevHi(Vec4 v)
{
	return __builtin_shufflevector(v, v, 1, 1, 0, 0);
}
// reduce to 4 elements (for Vec4: nothing to do)
[[nodiscard, maybe_unused]] static inline Vec4 vfold(Vec4 v) { return v; }
#ifdef __AVX__
[[nodiscard]] static inline Vec vreverse(Vec v)
{
	return __builtin_shufflevector(v, v, 7, 6, 5, 4, 3, 2, 1, 0);
}
[[nodiscard]] static inline Vec vdupLo(Vec v)
{
	return __builtin_shufflevector(v, v, 0, 0, 1, 1, 2, 2, 3, 3);
}
[[nodiscard]] static inline Vec vdupHi(Vec v)
{
	return __builtin_shufflevector(v, v, 4, 4, 5, 5, 6, 6, 7, 7);
}
[[nodiscard]] static inline Vec vdupRevLo(Vec v)
{
	return __builtin_shufflevector(v, v, 7, 7, 6, 6, 5, 5, 4, 4);
}
[[nodiscard]] static inline Vec vdupRevHi(Vec v)
{
	return __builtin_shufflevector(v, v, 3, 3, 2, 2, 1, 1, 0, 0);
}
// 8 -> 4 elements: add the two halves
[[nodiscard]] static inline Vec4 vfold(Vec v)
{
	return __builtin_shufflevector(v, v, 0, 1, 2, 3) +
	       __builtin_shufflevector(v, v, 4, 5, 6, 7);
}
#endif

// Process one vector of coefficients 'c' (= 'VEC_LEN' coefficients), starting
// at coefficient-index 'i', accumulate into 'acc'.
template<unsigned CHANNELS, bool REVERSE, typename V>
static inline void vstep(V& acc0, V& acc1, V c, const float* buf, size_t n, size_t i)
{
	constexpr size_t L = sizeof(V) / sizeof(float);
	if constexpr (CHANNELS == 1) {
		if constexpr (!REVERSE) {
			acc0 += c * unalignedLoad<V>(buf + i);
		} else {
			acc0 += vreverse(c) * unalignedLoad<V>(buf + (n - L - i));
		}
	} else {
		// stereo: each coefficient is used for both the left and the
		// right sample, so one vector of coefficients covers two
		// vectors of (interleaved) samples.
		if constexpr (!REVERSE) {
			const float* b = buf + 2 * i;
			acc0 += vdupLo(c) * unalignedLoad<V>(b);
			acc1 += vdupHi(c) * unalignedLoad<V>(b + L);
		} else {
			const float* b = buf + 2 * (n - L - i);
			acc0 += vdupRevLo(c) * unalignedLoad<V>(b);
			acc1 += vdupRevHi(c) * unalignedLoad<V>(b + L);
		}
	}
}

// Dot-product of the sample buffer with two consecutive table rows: the row
// at 'tab' and the row directly following it in memory. The sample loads are
// shared between the two rows, so this costs ~1.5x a single dot-product.
// In the 'reverse' case 'tab' points to one-past-the-end of the first row and
// both rows are read backwards (see the comment above).
template<unsigned CHANNELS, bool REVERSE>
static inline void calcKernel2(
	const float* __restrict buf, const float* __restrict tab, size_t n,
	float* __restrict out0, float* __restrict out1)
{
	assert((n % 4) == 0);
	// In the 'reverse' case 'tab' points one-past-the-end of the first row.
	const float* t0 = REVERSE ? (tab - n) : tab;
	const float* t1 = t0 + n;

	Vec a0 = {}, a1 = {}, b0 = {}, b1 = {};
	size_t i = 0;
	for (/**/; (i + VEC_LEN) <= n; i += VEC_LEN) {
		vstep<CHANNELS, REVERSE>(a0, a1, unalignedLoad<Vec>(t0 + i), buf, n, i);
		vstep<CHANNELS, REVERSE>(b0, b1, unalignedLoad<Vec>(t1 + i), buf, n, i);
	}
	Vec4 ra0 = vfold(a0), ra1 = vfold(a1);
	Vec4 rb0 = vfold(b0), rb1 = vfold(b1);
	if constexpr (VEC_LEN > 4) {
		if (i < n) { // remaining 4 coefficients
			vstep<CHANNELS, REVERSE>(ra0, ra1, unalignedLoad<Vec4>(t0 + i), buf, n, i);
			vstep<CHANNELS, REVERSE>(rb0, rb1, unalignedLoad<Vec4>(t1 + i), buf, n, i);
		}
	}
	if constexpr (CHANNELS == 1) {
		out0[0] = (ra0[0] + ra0[1]) + (ra0[2] + ra0[3]);
		out1[0] = (rb0[0] + rb0[1]) + (rb0[2] + rb0[3]);
	} else {
		Vec4 ra = ra0 + ra1; // [L R L R]
		Vec4 rb = rb0 + rb1;
		out0[0] = ra[0] + ra[2];
		out0[1] = ra[1] + ra[3];
		out1[0] = rb[0] + rb[2];
		out1[1] = rb[1] + rb[3];
	}
}

#elif defined(__SSE2__)

// SSE2 intrinsics. Only used by compilers without vector extensions (MSVC).

static inline __m128 reverse(__m128 x)
{
	return _mm_shuffle_ps(x, x, _MM_SHUFFLE(0, 1, 2, 3));
}

template<bool REVERSE>
static inline void calcSseMono(const float* buf_, const float* tab_, size_t len, float* out)
{
	assert((len % 4) == 0);
	assert((uintptr_t(tab_) % 16) == 0);

	auto x = narrow<ptrdiff_t>((len & ~7) * sizeof(float));
	assert((x % 32) == 0);
	const char* buf = std::bit_cast<const char*>(buf_) + x;
	const char* tab = std::bit_cast<const char*>(tab_) + (REVERSE ? -x : x);
	x = -x;

	__m128 a0 = _mm_setzero_ps();
	__m128 a1 = _mm_setzero_ps();
	do {
		__m128 b0 = _mm_loadu_ps(std::bit_cast<const float*>(buf + x +  0));
		__m128 b1 = _mm_loadu_ps(std::bit_cast<const float*>(buf + x + 16));
		__m128 t0, t1;
		if constexpr (REVERSE) {
			t0 = reverse(_mm_loadu_ps(std::bit_cast<const float*>(tab - x - 16)));
			t1 = reverse(_mm_loadu_ps(std::bit_cast<const float*>(tab - x - 32)));
		} else {
			t0 = _mm_loadu_ps (std::bit_cast<const float*>(tab + x +  0));
			t1 = _mm_loadu_ps (std::bit_cast<const float*>(tab + x + 16));
		}
		__m128 m0 = _mm_mul_ps(b0, t0);
		__m128 m1 = _mm_mul_ps(b1, t1);
		a0 = _mm_add_ps(a0, m0);
		a1 = _mm_add_ps(a1, m1);
		x += 2 * sizeof(__m128);
	} while (x < 0);
	if (len & 4) {
		__m128 b0 = _mm_loadu_ps(std::bit_cast<const float*>(buf));
		__m128 t0;
		if constexpr (REVERSE) {
			t0 = reverse(_mm_loadu_ps(std::bit_cast<const float*>(tab - 16)));
		} else {
			t0 = _mm_loadu_ps (std::bit_cast<const float*>(tab));
		}
		__m128 m0 = _mm_mul_ps(b0, t0);
		a0 = _mm_add_ps(a0, m0);
	}

	__m128 a = _mm_add_ps(a0, a1);
	// The following can be _slightly_ faster by using the SSE3 _mm_hadd_ps()
	// intrinsic, but not worth the trouble.
	__m128 t = _mm_add_ps(a, _mm_movehl_ps(a, a));
	__m128 s = _mm_add_ss(t, _mm_shuffle_ps(t, t, 1));

	_mm_store_ss(out, s);
}

template<int N> static inline __m128 shuffle(__m128 x)
{
	return _mm_castsi128_ps(_mm_shuffle_epi32(_mm_castps_si128(x), N));
}
template<bool REVERSE>
static inline void calcSseStereo(const float* buf_, const float* tab_, size_t len, float* out)
{
	assert((len % 4) == 0);
	assert((uintptr_t(tab_) % 16) == 0);

	auto x = narrow<ptrdiff_t>(2 * (len & ~7) * sizeof(float));
	const auto* buf = std::bit_cast<const char*>(buf_) + x;
	const auto* tab = std::bit_cast<const char*>(tab_);
	x = -x;

	__m128 a0 = _mm_setzero_ps();
	__m128 a1 = _mm_setzero_ps();
	__m128 a2 = _mm_setzero_ps();
	__m128 a3 = _mm_setzero_ps();
	do {
		__m128 b0 = _mm_loadu_ps(std::bit_cast<const float*>(buf + x +  0));
		__m128 b1 = _mm_loadu_ps(std::bit_cast<const float*>(buf + x + 16));
		__m128 b2 = _mm_loadu_ps(std::bit_cast<const float*>(buf + x + 32));
		__m128 b3 = _mm_loadu_ps(std::bit_cast<const float*>(buf + x + 48));
		__m128 ta, tb;
		if constexpr (REVERSE) {
			ta = reverse(_mm_loadu_ps(std::bit_cast<const float*>(tab - 16)));
			tb = reverse(_mm_loadu_ps(std::bit_cast<const float*>(tab - 32)));
			tab -= 2 * sizeof(__m128);
		} else {
			ta = _mm_loadu_ps (std::bit_cast<const float*>(tab +  0));
			tb = _mm_loadu_ps (std::bit_cast<const float*>(tab + 16));
			tab += 2 * sizeof(__m128);
		}
		__m128 t0 = shuffle<0x50>(ta);
		__m128 t1 = shuffle<0xFA>(ta);
		__m128 t2 = shuffle<0x50>(tb);
		__m128 t3 = shuffle<0xFA>(tb);
		__m128 m0 = _mm_mul_ps(b0, t0);
		__m128 m1 = _mm_mul_ps(b1, t1);
		__m128 m2 = _mm_mul_ps(b2, t2);
		__m128 m3 = _mm_mul_ps(b3, t3);
		a0 = _mm_add_ps(a0, m0);
		a1 = _mm_add_ps(a1, m1);
		a2 = _mm_add_ps(a2, m2);
		a3 = _mm_add_ps(a3, m3);
		x += 4 * sizeof(__m128);
	} while (x < 0);
	if (len & 4) {
		__m128 b0 = _mm_loadu_ps(std::bit_cast<const float*>(buf +  0));
		__m128 b1 = _mm_loadu_ps(std::bit_cast<const float*>(buf + 16));
		__m128 ta;
		if constexpr (REVERSE) {
			ta = reverse(_mm_loadu_ps(std::bit_cast<const float*>(tab - 16)));
		} else {
			ta = _mm_loadu_ps (std::bit_cast<const float*>(tab +  0));
		}
		__m128 t0 = shuffle<0x50>(ta);
		__m128 t1 = shuffle<0xFA>(ta);
		__m128 m0 = _mm_mul_ps(b0, t0);
		__m128 m1 = _mm_mul_ps(b1, t1);
		a0 = _mm_add_ps(a0, m0);
		a1 = _mm_add_ps(a1, m1);
	}

	__m128 a01 = _mm_add_ps(a0, a1);
	__m128 a23 = _mm_add_ps(a2, a3);
	__m128 a   = _mm_add_ps(a01, a23);
	// Can faster with SSE3, but (like above) not worth the trouble.
	__m128 s = _mm_add_ps(a, _mm_movehl_ps(a, a));
	_mm_store_ss(&out[0], s);
	_mm_store_ss(&out[1], shuffle<0x55>(s));
}

template<unsigned CHANNELS, bool REVERSE>
static inline void calcKernel(
	const float* __restrict buf, const float* __restrict tab, size_t n,
	float* __restrict out)
{
	if constexpr (CHANNELS == 1) {
		calcSseMono  <REVERSE>(buf, tab, n, out);
	} else {
		calcSseStereo<REVERSE>(buf, tab, n, out);
	}
}

#else // no vector extensions, no SSE2

// Plain C++ version, both mono and stereo. Four independent accumulators so
// that the compiler can (partially) auto-vectorize.
template<unsigned CHANNELS, bool REVERSE>
static inline void calcKernel(
	const float* __restrict buf, const float* __restrict tab, size_t n,
	float* __restrict out)
{
	assert((n % 4) == 0);
	for (auto ch : xrange(CHANNELS)) {
		float r0 = 0.0f;
		float r1 = 0.0f;
		float r2 = 0.0f;
		float r3 = 0.0f;
		if constexpr (!REVERSE) {
			for (size_t i = 0; i < n; i += 4) {
				r0 += tab[i + 0] * buf[CHANNELS * (i + 0)];
				r1 += tab[i + 1] * buf[CHANNELS * (i + 1)];
				r2 += tab[i + 2] * buf[CHANNELS * (i + 2)];
				r3 += tab[i + 3] * buf[CHANNELS * (i + 3)];
			}
		} else {
			for (ptrdiff_t i = 0; i < ptrdiff_t(n); i += 4) {
				r0 += tab[-i - 1] * buf[CHANNELS * (i + 0)];
				r1 += tab[-i - 2] * buf[CHANNELS * (i + 1)];
				r2 += tab[-i - 3] * buf[CHANNELS * (i + 2)];
				r3 += tab[-i - 4] * buf[CHANNELS * (i + 3)];
			}
		}
		out[ch] = r0 + r1 + r2 + r3;
		++buf;
	}
}


#endif

#ifndef RESAMPLE_HQ_VECTOR_EXT
// Simple (not shared-load) version for the non-vector-extension paths.
template<unsigned CHANNELS, bool REVERSE>
static inline void calcKernel2(
	const float* __restrict buf, const float* __restrict tab, size_t n,
	float* __restrict out0, float* __restrict out1)
{
	calcKernel<CHANNELS, REVERSE>(buf, tab, n, out0);
	calcKernel<CHANNELS, REVERSE>(buf, tab + n, n, out1); // 2nd row (or its end)
}
#endif

template<unsigned CHANNELS>
void ResampleHQ<CHANNELS>::calcOutput(
	unsigned intPos, uint32_t fracPos, float* __restrict output)
{
	assert((filterLen & 3) == 0);

	unsigned bufIdx = intPos + bufStart;
	assert((bufIdx + filterLen) <= bufEnd);
	const float* buf = &buffer[bufIdx * CHANNELS];

	// Row 't' of the table holds the filter for phase (t + 0.5) / TAB_LEN.
	// Linearly interpolate between the two rows 'r' and 'r+1' whose phases
	// enclose the requested phase: r = floor(phase * TAB_LEN - 0.5), which
	// is in the range [-1, TAB_LEN-1]. The top TAB_BITS bits of the
	// (shifted) fraction give 'r', the remaining bits the weight 'w'.
	constexpr uint32_t HALF_ROW = uint32_t(1) << (31 - TAB_BITS);
	constexpr uint32_t W_MASK = (uint32_t(1) << (32 - TAB_BITS)) - 1;
	constexpr float W_SCALE = 1.0f / float(uint64_t(1) << (32 - TAB_BITS));
	uint32_t u = fracPos - HALF_ROW; // wraps for fracPos < HALF_ROW -> r = -1
	int r = (fracPos < HALF_ROW) ? -1 : int(u >> (32 - TAB_BITS));
	float w = float(u & W_MASK) * W_SCALE;

	// Only the first half of the rows is stored (table[0] is row -1):
	// row TAB_LEN-1-t is stored row t read backwards (the 'reverse'
	// kernel). Thanks to the two extra rows -1 and HALF_TAB_LEN the two
	// rows are adjacent in memory in both halves.
	float a[CHANNELS], b[CHANNELS];
	if (r < int(HALF_TAB_LEN)) {
		// rows r, r+1, both stored
		calcKernel2<CHANNELS, false>(buf, &table[(r + 1) * filterLen], filterLen, a, b);
	} else {
		// rows r, r+1 are stored rows m, m-1 (mirrored), with m = TAB_LEN-1-r
		int m = int(TAB_LEN) - 1 - r;
		calcKernel2<CHANNELS, true>(buf, &table[(m + 1) * filterLen], filterLen, b, a);
	}
	for (auto ch : xrange(CHANNELS)) {
		output[ch] = a[ch] + w * (b[ch] - a[ch]);
	}
}

template<unsigned CHANNELS>
void ResampleHQ<CHANNELS>::prepareData(unsigned emuNum)
{
	// Still enough free space at end of buffer?
	unsigned free = unsigned(buffer.size() / CHANNELS) - bufEnd;
	if (free < emuNum) {
		// No, then move everything to the start
		// (data needs to be in a contiguous memory block)
		unsigned available = bufEnd - bufStart;
		memmove(&buffer[0], &buffer[bufStart * size_t(CHANNELS)],
			available * size_t(CHANNELS) * sizeof(float));
		bufStart = 0;
		bufEnd = available;

		free = unsigned(buffer.size() / CHANNELS) - bufEnd;
		auto missing = narrow_cast<int>(emuNum - free);
		if (missing > 0) [[unlikely]] {
			// Still not enough room: grow the buffer.
			// TODO an alternative is to instead of using a large
			// buffer, chop the work in multiple smaller pieces.
			// That may have the advantage that the data fits in
			// the CPU's data cache. OTOH too small chunks have
			// more overhead. (Not yet implemented because it's
			// more complex).
			buffer.resize(buffer.size() + missing * size_t(CHANNELS));
		}
	}
	small_buffer<float, 8192> tmpBufExtra(uninitialized_tag{}, emuNum * CHANNELS + 3); // typical ~5194 (PSG, samples=1024) but could be larger
	auto tmpBuf = subspan(tmpBufExtra, 0, emuNum * CHANNELS);
	if (input.generateInput(tmpBufExtra.data(), emuNum)) {
		copy_to_range(tmpBuf,
		              subspan(buffer, bufEnd * CHANNELS));
		bufEnd += emuNum;
		nonzeroSamples = bufEnd - bufStart;
	} else {
		std::ranges::fill(subspan(buffer, bufEnd * CHANNELS, emuNum * CHANNELS), 0);
		bufEnd += emuNum;
	}

	assert(bufStart <= bufEnd);
	assert(bufEnd <= (buffer.size() / CHANNELS));
}

template<unsigned CHANNELS>
bool ResampleHQ<CHANNELS>::generateOutputImpl(
	float* __restrict dataOut, size_t hostNum, EmuTime time)
{
	auto& emuClk = getEmuClock();
	unsigned emuNum = emuClk.getTicksTill(time);
	if (emuNum > 0) {
		prepareData(emuNum);
	}

	bool notMuted = nonzeroSamples > 0;
	if (notMuted) {
		// main processing loop
		EmuTime host1 = hostClock.getFastAdd(1);
		assert(host1 > emuClk.getTime());
		// Position (in input samples, relative to 'bufStart') of the
		// first output sample, as 32.32 fixed point. Advancing this with
		// a fixed point 'step' keeps the position exact over the whole
		// fragment. (A float 'pos += ratio' accumulated an error of up
		// to ~1e-4 input samples over a 1024-sample fragment, that
		// showed up as sidebands only 30-70dB below the signal.)
		double pos0 = emuClk.getTicksTillDouble(host1);
		assert(pos0 <= (ratio + 2));
		auto pos = uint64_t(pos0 * 4294967296.0 + 0.5);
		for (auto i : xrange(hostNum)) {
			calcOutput(unsigned(pos >> 32), uint32_t(pos),
			           &dataOut[i * CHANNELS]);
			pos += step;
		}
	}
	emuClk += emuNum;
	bufStart += emuNum;
	nonzeroSamples = std::max<int>(0, nonzeroSamples - emuNum);

	assert(bufStart <= bufEnd);
	unsigned available = bufEnd - bufStart;
	unsigned extra = filterLen + 1 + narrow_cast<int>(ratio) + 1;
	assert(available == extra); (void)available; (void)extra;

	return notMuted;
}

// Force template instantiation.
template class ResampleHQ<1>;
template class ResampleHQ<2>;

} // namespace openmsx
