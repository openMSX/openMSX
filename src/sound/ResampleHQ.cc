// Based on libsamplerate-0.1.2 (aka Secret Rabit Code)
//
//  simplified code in several ways:
//   - resample algorithm is no longer switchable, we took this variant:
//        Band limited sinc interpolation, fastest, 97dB SNR, 80% BW
//   - don't allow to change sample rate on-the-fly
//   - assume input (and thus also output) signals have infinte length, so
//     there is no special code to handle the ending of the signal
//   - changed/simplified API to better match openmsx use model
//     (e.g. remove all error checking)

#include "ResampleHQ.hh"
#include "ResampledSoundDevice.hh"
#include "FixedPoint.hh"
#include "MemBuffer.hh"
#include "countof.hh"
#include "likely.hh"
#include "stl.hh"
#include "vla.hh"
#include "build-info.hh"
#include <algorithm>
#include <vector>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <cassert>
#ifdef __SSE2__
#include <emmintrin.h>
#endif

namespace openmsx {

// Note: without appending 'f' to the values in ResampleCoeffs.ii,
// this will generate thousands of C4305 warnings in VC++
// E.g. warning C4305: 'initializing' : truncation from 'double' to 'const float'
static const float coeffs[] = {
	#include "ResampleCoeffs.ii"
};

static const int INDEX_INC = 128;
static const int COEFF_LEN = countof(coeffs);
static const int COEFF_HALF_LEN = COEFF_LEN - 1;
static const unsigned TAB_LEN = 4096;

class ResampleCoeffs
{
public:
	static ResampleCoeffs& instance();
	void getCoeffs(double ratio, float*& table, unsigned& filterLen);
	void releaseCoeffs(double ratio);

private:
	using FilterIndex = FixedPoint<16>;
	using Table = MemBuffer<float, SSE2_ALIGNMENT>;

	ResampleCoeffs();
	~ResampleCoeffs();

	double getCoeff(FilterIndex index);
	Table calcTable(double ratio, unsigned& filterLen);

	struct Element {
		double ratio;
		Table table;
		unsigned filterLen;
		unsigned count;
	};
	std::vector<Element> cache; // typically 1-4 entries -> unsorted vector
};

ResampleCoeffs::ResampleCoeffs()
{
}

ResampleCoeffs::~ResampleCoeffs()
{
	assert(cache.empty());
}

ResampleCoeffs& ResampleCoeffs::instance()
{
	static ResampleCoeffs resampleCoeffs;
	return resampleCoeffs;
}

void ResampleCoeffs::getCoeffs(
	double ratio, float*& table, unsigned& filterLen)
{
	auto it = find_if(begin(cache), end(cache),
		[=](const Element& e) { return e.ratio == ratio; });
	if (it != end(cache)) {
		table     = it->table.data();
		filterLen = it->filterLen;
		it->count++;
		return;
	}
	Table tab = calcTable(ratio, filterLen);
	table = tab.data();
	cache.push_back({ratio, std::move(tab), filterLen, 1});
}

void ResampleCoeffs::releaseCoeffs(double ratio)
{
	auto it = rfind_if_unguarded(cache,
		[=](const Element& e) { return e.ratio == ratio; });
	it->count--;
	if (it->count == 0) {
		move_pop_back(cache, it);
	}
}

double ResampleCoeffs::getCoeff(FilterIndex index)
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
	FilterIndex increment = FilterIndex(floatIncr);
	FilterIndex maxFilterIndex(COEFF_HALF_LEN);

	int min_idx = -maxFilterIndex.divAsInt(increment);
	int max_idx = 1 + (maxFilterIndex - (increment - FilterIndex(floatIncr))).divAsInt(increment);
	int idx_cnt = max_idx - min_idx + 1;
	filterLen = (idx_cnt + 3) & ~3; // round up to multiple of 4
	min_idx -= (filterLen - idx_cnt);
	Table table(TAB_LEN * filterLen);
	memset(table.data(), 0, TAB_LEN * filterLen * sizeof(float));

	for (unsigned t = 0; t < TAB_LEN; ++t) {
		double lastPos = double(t) / TAB_LEN;
		FilterIndex startFilterIndex(lastPos * floatIncr);

		FilterIndex filterIndex(startFilterIndex);
		int coeffCount = (maxFilterIndex - filterIndex).divAsInt(increment);
		filterIndex += increment * coeffCount;
		int bufIndex = -coeffCount;
		do {
			table[t * filterLen + bufIndex - min_idx] =
				float(getCoeff(filterIndex) * normFactor);
			filterIndex -= increment;
			bufIndex += 1;
		} while (filterIndex >= FilterIndex(0));

		filterIndex = increment - startFilterIndex;
		coeffCount = (maxFilterIndex - filterIndex).divAsInt(increment);
		filterIndex += increment * coeffCount;
		bufIndex = 1 + coeffCount;
		do {
			table[t * filterLen + bufIndex - min_idx] =
				float(getCoeff(filterIndex) * normFactor);
			filterIndex -= increment;
			bufIndex -= 1;
		} while (filterIndex > FilterIndex(0));
	}
	return table;
}


template <unsigned CHANNELS>
ResampleHQ<CHANNELS>::ResampleHQ(
		ResampledSoundDevice& input_,
		const DynamicClock& hostClock_, unsigned emuSampleRate)
	: input(input_)
	, hostClock(hostClock_)
	, emuClock(hostClock.getTime(), emuSampleRate)
	, ratio(float(emuSampleRate) / hostClock.getFreq())
{
	ResampleCoeffs::instance().getCoeffs(ratio, table, filterLen);

	// fill buffer with 'enough' zero's
	unsigned extra = int(filterLen + 1 + ratio + 1);
	bufStart = 0;
	bufEnd   = extra;
	nonzeroSamples = 0;
	unsigned initialSize = 4000; // buffer grows dynamically if this is too small
	buffer.resize((initialSize + extra) * CHANNELS); // zero-initialized
}

template <unsigned CHANNELS>
ResampleHQ<CHANNELS>::~ResampleHQ()
{
	ResampleCoeffs::instance().releaseCoeffs(ratio);
}

#ifdef __SSE2__
static inline void calcSseMono(const float* buf_, const float* tab_, size_t len, int* out)
{
	assert((len % 4) == 0);
	assert((uintptr_t(tab_) % 16) == 0);

	ptrdiff_t x = (len & ~7) * sizeof(float);
	assert((x % 32) == 0);
	const char* buf = reinterpret_cast<const char*>(buf_) + x;
	const char* tab = reinterpret_cast<const char*>(tab_) + x;
	x = -x;

	__m128 a0 = _mm_setzero_ps();
	__m128 a1 = _mm_setzero_ps();
	do {
		__m128 b0 = _mm_loadu_ps(reinterpret_cast<const float*>(buf + x +  0));
		__m128 b1 = _mm_loadu_ps(reinterpret_cast<const float*>(buf + x + 16));
		__m128 t0 = _mm_load_ps (reinterpret_cast<const float*>(tab + x +  0));
		__m128 t1 = _mm_load_ps (reinterpret_cast<const float*>(tab + x + 16));
		__m128 m0 = _mm_mul_ps(b0, t0);
		__m128 m1 = _mm_mul_ps(b1, t1);
		a0 = _mm_add_ps(a0, m0);
		a1 = _mm_add_ps(a1, m1);
		x += 2 * sizeof(__m128);
	} while (x < 0);
	if (len & 4) {
		__m128 b0 = _mm_loadu_ps(reinterpret_cast<const float*>(buf));
		__m128 t0 = _mm_load_ps (reinterpret_cast<const float*>(tab));
		__m128 m0 = _mm_mul_ps(b0, t0);
		a0 = _mm_add_ps(a0, m0);
	}

	__m128 a = _mm_add_ps(a0, a1);
	// The following can be _slighly_ faster by using the SSE3 _mm_hadd_ps()
	// intrinsic, but not worth the trouble.
	__m128 t = _mm_add_ps(a, _mm_movehl_ps(a, a));
	__m128 s = _mm_add_ss(t, _mm_shuffle_ps(t, t, 1));

	*out = _mm_cvtss_si32(s);
}

template<int N> static inline __m128 shuffle(__m128 x)
{
	return _mm_castsi128_ps(_mm_shuffle_epi32(_mm_castps_si128(x), N));
}
static inline void calcSseStereo(const float* buf_, const float* tab_, size_t len, int* out)
{
	assert((len % 4) == 0);
	assert((uintptr_t(tab_) % 16) == 0);

	ptrdiff_t x = (len & ~7) * sizeof(float);
	const char* buf = reinterpret_cast<const char*>(buf_) + 2*x;
	const char* tab = reinterpret_cast<const char*>(tab_) +   x;
	x = -x;

	__m128 a0 = _mm_setzero_ps();
	__m128 a1 = _mm_setzero_ps();
	__m128 a2 = _mm_setzero_ps();
	__m128 a3 = _mm_setzero_ps();
	do {
		__m128 b0 = _mm_loadu_ps(reinterpret_cast<const float*>(buf + 2*x +  0));
		__m128 b1 = _mm_loadu_ps(reinterpret_cast<const float*>(buf + 2*x + 16));
		__m128 b2 = _mm_loadu_ps(reinterpret_cast<const float*>(buf + 2*x + 32));
		__m128 b3 = _mm_loadu_ps(reinterpret_cast<const float*>(buf + 2*x + 48));
		__m128 ta = _mm_load_ps (reinterpret_cast<const float*>(tab +   x +  0));
		__m128 tb = _mm_load_ps (reinterpret_cast<const float*>(tab +   x + 16));
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
		x += 2 * sizeof(__m128);
	} while (x < 0);
	if (len & 4) {
		__m128 b0 = _mm_loadu_ps(reinterpret_cast<const float*>(buf +  0));
		__m128 b1 = _mm_loadu_ps(reinterpret_cast<const float*>(buf + 16));
		__m128 ta = _mm_load_ps (reinterpret_cast<const float*>(tab));
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
	__m128i si = _mm_cvtps_epi32(s);
#if ASM_X86_64
	*reinterpret_cast<int64_t*>(out) = _mm_cvtsi128_si64(si);
#else
	out[0] = _mm_cvtsi128_si32(si);
	out[1] = _mm_cvtsi128_si32(_mm_shuffle_epi32(si, 0x55));
#endif
}
#endif

template <unsigned CHANNELS>
void ResampleHQ<CHANNELS>::calcOutput(
	float pos, int* __restrict output)
{
	assert((filterLen & 3) == 0);

	int t = int(pos * TAB_LEN + 0.5f) % TAB_LEN;
	const float* tab = &table[t * filterLen];

	int bufIdx = int(pos) + bufStart;
	assert((bufIdx + filterLen) <= bufEnd);
	bufIdx *= CHANNELS;
	const float* buf = &buffer[bufIdx];

#ifdef __SSE2__
	if (CHANNELS == 1) {
		calcSseMono  (buf, tab, filterLen, output);
	} else {
		calcSseStereo(buf, tab, filterLen, output);
	}
	return;
#endif

	// c++ version, both mono and stereo
	for (unsigned ch = 0; ch < CHANNELS; ++ch) {
		float r0 = 0.0f;
		float r1 = 0.0f;
		float r2 = 0.0f;
		float r3 = 0.0f;
		for (unsigned i = 0; i < filterLen; i += 4) {
			r0 += tab[i + 0] * buf[CHANNELS * (i + 0)];
			r1 += tab[i + 1] * buf[CHANNELS * (i + 1)];
			r2 += tab[i + 2] * buf[CHANNELS * (i + 2)];
			r3 += tab[i + 3] * buf[CHANNELS * (i + 3)];
		}
		output[ch] = lrint(r0 + r1 + r2 + r3);
		++buf;
	}
}

template <unsigned CHANNELS>
void ResampleHQ<CHANNELS>::prepareData(unsigned emuNum)
{
	// Still enough free space at end of buffer?
	unsigned free = unsigned(buffer.size() / CHANNELS) - bufEnd;
	if (free < emuNum) {
		// No, then move everything to the start
		// (data needs to be in a contiguous memory block)
		unsigned available = bufEnd - bufStart;
		memmove(&buffer[0], &buffer[bufStart * CHANNELS],
			available * CHANNELS * sizeof(float));
		bufStart = 0;
		bufEnd = available;

		free = unsigned(buffer.size() / CHANNELS) - bufEnd;
		int missing = emuNum - free;
		if (unlikely(missing > 0)) {
			// Still not enough room: grow the buffer.
			// TODO an alternative is to instead of using a large
			// buffer, chop the work in multiple smaller pieces.
			// That may have the advantage that the data fits in
			// the CPU's data cache. OTOH too small chunks have
			// more overhead. (Not yet implemented because it's
			// more complex).
			buffer.resize(buffer.size() + missing * CHANNELS);
		}
	}
	VLA_SSE_ALIGNED(int, tmpBuf, emuNum * CHANNELS + 3);
	if (input.generateInput(tmpBuf, emuNum)) {
		for (unsigned i = 0; i < emuNum * CHANNELS; ++i) {
			buffer[bufEnd * CHANNELS + i] = float(tmpBuf[i]);
		}
		bufEnd += emuNum;
		nonzeroSamples = bufEnd - bufStart;
	} else {
		memset(&buffer[bufEnd * CHANNELS], 0,
		       emuNum * CHANNELS * sizeof(float));
		bufEnd += emuNum;
	}

	assert(bufStart <= bufEnd);
	assert(bufEnd <= (buffer.size() / CHANNELS));
}

template <unsigned CHANNELS>
bool ResampleHQ<CHANNELS>::generateOutput(
	int* __restrict dataOut, unsigned hostNum, EmuTime::param time)
{
	unsigned emuNum = emuClock.getTicksTill(time);
	if (emuNum > 0) {
		prepareData(emuNum);
	}

	bool notMuted = nonzeroSamples > 0;
	if (notMuted) {
		// main processing loop
		EmuTime host1 = hostClock.getFastAdd(1);
		assert(host1 > emuClock.getTime());
		float pos = emuClock.getTicksTillDouble(host1);
		assert(pos <= (ratio + 2));
		for (unsigned i = 0; i < hostNum; ++i) {
			calcOutput(pos, &dataOut[i * CHANNELS]);
			pos += ratio;
		}
	}
	emuClock += emuNum;
	bufStart += emuNum;
	nonzeroSamples = std::max<int>(0, nonzeroSamples - emuNum);

	assert(bufStart <= bufEnd);
	unsigned available = bufEnd - bufStart;
	unsigned extra = int(filterLen + 1 + ratio + 1);
	assert(available == extra); (void)available; (void)extra;

	return notMuted;
}

// Force template instantiation.
template class ResampleHQ<1>;
template class ResampleHQ<2>;

} // namespace openmsx
