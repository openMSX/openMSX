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
#include "MemBuffer.hh"
#include "aligned.hh"
#include "likely.hh"
#include "ranges.hh"
#include "stl.hh"
#include "vla.hh"
#include "xrange.hh"
#include "build-info.hh"
#include <vector>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <cassert>
#include <iterator>
#ifdef __SSE2__
#include <emmintrin.h>
#endif

namespace openmsx {

// Note: without appending 'f' to the values in ResampleCoeffs.ii,
// this will generate thousands of C4305 warnings in VC++
// E.g. warning C4305: 'initializing' : truncation from 'double' to 'const float'
constexpr float coeffs[] = {
	#include "ResampleCoeffs.ii"
};

using FilterIndex = FixedPoint<16>;

constexpr int INDEX_INC = 128;
constexpr int COEFF_LEN = int(std::size(coeffs));
constexpr int COEFF_HALF_LEN = COEFF_LEN - 1;
constexpr unsigned TAB_LEN = 4096;
constexpr unsigned HALF_TAB_LEN = TAB_LEN / 2;

class ResampleCoeffs
{
public:
	ResampleCoeffs(const ResampleCoeffs&) = delete;
	ResampleCoeffs& operator=(const ResampleCoeffs&) = delete;

	static ResampleCoeffs& instance();
	void getCoeffs(double ratio, int16_t*& permute, float*& table, unsigned& filterLen);
	void releaseCoeffs(double ratio);

private:
	using Table = MemBuffer<float, SSE_ALIGNMENT>;
	using PermuteTable = MemBuffer<int16_t>;

	ResampleCoeffs() = default;
	~ResampleCoeffs();

	static Table calcTable(double ratio, int16_t* permute, unsigned& filterLen);

	struct Element {
		double ratio;
		PermuteTable permute;
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

void ResampleCoeffs::getCoeffs(
	double ratio, int16_t*& permute, float*& table, unsigned& filterLen)
{
	if (auto it = ranges::find(cache, ratio, &Element::ratio);
	    it != end(cache)) {
		permute   = it->permute.data();
		table     = it->table.data();
		filterLen = it->filterLen;
		it->count++;
		return;
	}
	Element elem;
	elem.ratio = ratio;
	elem.count = 1;
	elem.permute = PermuteTable(HALF_TAB_LEN);
	elem.table = calcTable(ratio, elem.permute.data(), elem.filterLen);
	permute   = elem.permute.data();
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

// -- Permutation stuff --
//
// The rows in the resample coefficient table are not visited sequentially.
// Instead, depending on the resample-ratio, we take fixed non-integer jumps
// from one row to the next.
//
// In reality the table has 4096 rows (of which only 2048 are actually stored).
// But for simplicity I'll here work out examples for a table with only 16 rows
// (of which 8 are stored).
//
// Let's first assume a jump of '5.2'. This means that after we've used row
// 'r', the next row we need is 'r + 5.2'. Of course row numbers must be
// integers, so a jump of 5.2 actually means that 80% of the time we advance 5
// rows and 20% of the time we advance 6 rows.
//
// The rows in the (full) table are circular. This means that once we're past
// row 15 (in this example) we restart at row 0. So rows 'wrap' past the end
// (modulo arithmetic). We also only store the 1st half of the table, the
// entries for the 2nd half are 'folded' back to the 1st half according to the
// formula: y = 15 - x.
//
// Let's now calculate the possible transitions. If we're currently on row '0',
// the next row will be either '5' (80% chance) or row '6' (20% chance). When
// we're on row '5' the next most likely row will be '10', but after folding
// '10' becomes '15-10 = 5' (so 5 goes to itself (80% chance)). Row '10' most
// likely goes to '15', after folding we get that '5' goes to '0'. Row '15'
// most likely goes to '20', and after wrapping and folding that becomes '0'
// goes to '4'. Calculating this for all rows gives:
//   0 -> 5 or 4 (80%)   0 -> 6 or 5 (20%)
//   1 -> 6 or 3         1 -> 7 or 4
//   2 -> 7 or 2         2 -> 7 or 3
//   3 -> 7 or 1         3 -> 6 or 2
//   4 -> 6 or 0         4 -> 5 or 1
//   5 -> 5 or 0         5 -> 4 or 0
//   6 -> 4 or 1         6 -> 3 or 0
//   7 -> 3 or 2         7 -> 2 or 1
// So every row has 4 possible successors (2 more and 2 less likely). Possibly
// some of these 4 are the same, or even the same as the starting row. Note
// that if row x goes to row y (x->y) then also y->x, this turns out to be true
// in general.
//
// For cache efficiency it's best if rows that are needed after each other in
// time are also stored sequentially in memory (both before or after is fine).
// Clearly storing the rows in numeric order will not read the memory
// sequentially. For this specific example we could stores the rows in the
// order:
//    2, 7, 3, 1, 6, 4, 0, 5
// With this order all likely transitions are sequential. The less likely
// transitions are not. But I don't believe there exists an order that's good
// for both the likely and the unlikely transitions. Do let me know if I'm
// wrong.
//
// In this example the transitions form a single chain (it turns out this is
// often the case). But for example for a step-size of 4.3 we get
//   0 -> 4 or 3 (70%)   0 -> 5 or 4 (30%)
//   1 -> 5 or 2         1 -> 6 or 3
//   2 -> 6 or 1         2 -> 7 or 2
//   3 -> 7 or 0         3 -> 7 or 1
//   4 -> 7 or 0         4 -> 6 or 0
//   5 -> 6 or 1         5 -> 5 or 0
//   6 -> 5 or 2         6 -> 4 or 1
//   7 -> 4 or 3         7 -> 3 or 2
// Only looking at the more likely transitions, we get 2 cycles of length 4:
//   0, 4, 7, 3
//   1, 5, 6, 2
//
// So the previous example gave a single chain with 2 clear end-points. Now we
// have 2 separate cycles. It turns out that for any possible step-size we
// either get a single chain or k cycles of size N/k. (So e.g. a chain of
// length 5 plus a cycle of length 3 is impossible. Also 1 cycle of length 4
// plus 2 cycles of length 2 is impossible). To be honest I've only partially
// mathematically proven this, but at least I've verified it for N=16 and
// N=4096 for all possible step-sizes.
//
// To linearise a chain in memory there are only 2 (good) possibilities: start
// at either end-point. But to store a cycle any point is as good as any other.
// Also the order in which to store the cycles themselves can still be chosen.
//
// Let's come back to the example with step-size 4.3. If we linearise this as
//   | 0, 4, 7, 3 | 1, 5, 6, 2 |
// then most of the more likely transitions are sequential. The exceptions are
//     0 <-> 3   and   1 <-> 2
// but those are unavoidable with cycles. In return 2 of the less likely
// transitions '3 <-> 1' are now sequential. I believe this is the best
// possible linearization (better said: there are other linearizations that are
// equally good, but none is better). But do let me know if you find a better
// one!
//
// For step-size '8.4' an optimal(?) linearization seems to be
//   | 0, 7 | 1, 6 | 2, 5 | 3, 4 |
// For step-size '7.9' the order is:
//   | 7, 0 | 6, 1 | 5, 2 | 4, 3 |
// And for step-size '3.8':
//   | 7, 4, 0, 3 | 6, 5, 1, 2 |
//
// I've again not (fully) mathematically proven it, but it seems we can
// optimally(?) linearise cycles by:
// * if likely step < unlikely step:
//    pick unassigned rows from 0 to N/2-1, and complete each cycle
// * if likely step > unlikely step:
//    pick unassigned rows from N/2-1 to 0, and complete each cycle
//
// The routine calcPermute() below calculates these optimal(?) linearizations.
// More in detail it calculates a permutation table: the i-th element in this
// table tells where in memory the i-th logical row of the original (half)
// resample coefficient table is physically stored.

constexpr unsigned N = TAB_LEN;
constexpr unsigned N1 = N - 1;
constexpr unsigned N2 = N / 2;

static constexpr unsigned mapIdx(unsigned x)
{
	unsigned t = x & N1; // first wrap
	return (t < N2) ? t : N1 - t; // then fold
}

static constexpr std::pair<unsigned, unsigned> next(unsigned x, unsigned step)
{
	return {mapIdx(x + step), mapIdx(N1 - x + step)};
}

static void calcPermute(double ratio, int16_t* permute)
{
	double r2 = ratio * N;
	double fract = r2 - floor(r2);
	unsigned step = floor(r2);
	bool incr = [&] {
		if (fract > 0.5) {
			// mostly (> 50%) take steps of 'floor(r2) + 1'
			step += 1;
			return false; // assign from high to low
		} else {
			// mostly take steps of 'floor(r2)'
			return true; // assign from low to high
		}
	}();

	// initially set all as unassigned
	std::fill_n(permute, N2, -1);

	unsigned nxt1, nxt2;
	unsigned restart = incr ? 0 : N2 - 1;
	unsigned curr = restart;
	// check for chain (instead of cycles)
	if (incr) {
		for (auto i : xrange(N2)) {
			std::tie(nxt1, nxt2) = next(i, step);
			if ((nxt1 == i) || (nxt2 == i)) { curr = i; break; }
		}
	} else {
		for (unsigned i = N2 - 1; int(i) >= 0; --i) {
			std::tie(nxt1, nxt2) = next(i, step);
			if ((nxt1 == i) || (nxt2 == i)) { curr = i; break; }
		}
	}

	// assign all rows (in chain of cycle(s))
	unsigned cnt = 0;
	while (true) {
		assert(permute[curr] == -1);
		assert(cnt < N2);
		permute[curr] = cnt++;

		std::tie(nxt1, nxt2) = next(curr, step);
		if (permute[nxt1] == -1) {
			curr = nxt1;
			continue;
		} else if (permute[nxt2] == -1) {
			curr = nxt2;
			continue;
		}

		// finished chain or cycle
		if (cnt == N2) break; // done

		// continue with next cycle
		while (permute[restart] != -1) {
			if (incr) {
				++restart;
				assert(restart != N2);
			} else {
				assert(restart != 0);
				--restart;
			}
		}
		curr = restart;
	}

#ifdef DEBUG
	int16_t testPerm[N2];
	ranges::iota(testPerm, 0);
	assert(std::is_permutation(permute, permute + N2, testPerm));
#endif
}

static constexpr double getCoeff(FilterIndex index)
{
	double fraction = index.fractionAsDouble();
	int indx = index.toInt();
	return double(coeffs[indx]) +
	       fraction * (double(coeffs[indx + 1]) - double(coeffs[indx]));
}

ResampleCoeffs::Table ResampleCoeffs::calcTable(
	double ratio, int16_t* permute, unsigned& filterLen)
{
	calcPermute(ratio, permute);

	double floatIncr = (ratio > 1.0) ? INDEX_INC / ratio : INDEX_INC;
	double normFactor = floatIncr / INDEX_INC;
	auto increment = FilterIndex(floatIncr);
	FilterIndex maxFilterIndex(COEFF_HALF_LEN);

	int min_idx = -maxFilterIndex.divAsInt(increment);
	int max_idx = 1 + (maxFilterIndex - (increment - FilterIndex(floatIncr))).divAsInt(increment);
	int idx_cnt = max_idx - min_idx + 1;
	filterLen = (idx_cnt + 3) & ~3; // round up to multiple of 4
	min_idx -= (filterLen - idx_cnt) / 2;
	Table table(HALF_TAB_LEN * filterLen);
	memset(table.data(), 0, HALF_TAB_LEN * filterLen * sizeof(float));

	for (auto t : xrange(HALF_TAB_LEN)) {
		float* tab = &table[permute[t] * filterLen];
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
	return table;
}


template<unsigned CHANNELS>
ResampleHQ<CHANNELS>::ResampleHQ(
		ResampledSoundDevice& input_, const DynamicClock& hostClock_)
	: ResampleAlgo(input_)
	, hostClock(hostClock_)
	, ratio(float(hostClock.getPeriod().toDouble() / getEmuClock().getPeriod().toDouble()))
{
	ResampleCoeffs::instance().getCoeffs(double(ratio), permute, table, filterLen);

	// fill buffer with 'enough' zero's
	unsigned extra = int(filterLen + 1 + ratio + 1);
	bufStart = 0;
	bufEnd   = extra;
	nonzeroSamples = 0;
	unsigned initialSize = 4000; // buffer grows dynamically if this is too small
	buffer.resize((initialSize + extra) * CHANNELS); // zero-initialized
}

template<unsigned CHANNELS>
ResampleHQ<CHANNELS>::~ResampleHQ()
{
	ResampleCoeffs::instance().releaseCoeffs(double(ratio));
}

#ifdef __SSE2__
template<bool REVERSE>
static inline void calcSseMono(const float* buf_, const float* tab_, size_t len, float* out)
{
	assert((len % 4) == 0);
	assert((uintptr_t(tab_) % 16) == 0);

	ptrdiff_t x = (len & ~7) * sizeof(float);
	assert((x % 32) == 0);
	const char* buf = reinterpret_cast<const char*>(buf_) + x;
	const char* tab = reinterpret_cast<const char*>(tab_) + (REVERSE ? -x : x);
	x = -x;

	__m128 a0 = _mm_setzero_ps();
	__m128 a1 = _mm_setzero_ps();
	do {
		__m128 b0 = _mm_loadu_ps(reinterpret_cast<const float*>(buf + x +  0));
		__m128 b1 = _mm_loadu_ps(reinterpret_cast<const float*>(buf + x + 16));
		__m128 t0, t1;
		if constexpr (REVERSE) {
			t0 = _mm_loadr_ps(reinterpret_cast<const float*>(tab - x - 16));
			t1 = _mm_loadr_ps(reinterpret_cast<const float*>(tab - x - 32));
		} else {
			t0 = _mm_load_ps (reinterpret_cast<const float*>(tab + x +  0));
			t1 = _mm_load_ps (reinterpret_cast<const float*>(tab + x + 16));
		}
		__m128 m0 = _mm_mul_ps(b0, t0);
		__m128 m1 = _mm_mul_ps(b1, t1);
		a0 = _mm_add_ps(a0, m0);
		a1 = _mm_add_ps(a1, m1);
		x += 2 * sizeof(__m128);
	} while (x < 0);
	if (len & 4) {
		__m128 b0 = _mm_loadu_ps(reinterpret_cast<const float*>(buf));
		__m128 t0;
		if constexpr (REVERSE) {
			t0 = _mm_loadr_ps(reinterpret_cast<const float*>(tab - 16));
		} else {
			t0 = _mm_load_ps (reinterpret_cast<const float*>(tab));
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

	ptrdiff_t x = 2 * (len & ~7) * sizeof(float);
	const char* buf = reinterpret_cast<const char*>(buf_) + x;
	const char* tab = reinterpret_cast<const char*>(tab_);
	x = -x;

	__m128 a0 = _mm_setzero_ps();
	__m128 a1 = _mm_setzero_ps();
	__m128 a2 = _mm_setzero_ps();
	__m128 a3 = _mm_setzero_ps();
	do {
		__m128 b0 = _mm_loadu_ps(reinterpret_cast<const float*>(buf + x +  0));
		__m128 b1 = _mm_loadu_ps(reinterpret_cast<const float*>(buf + x + 16));
		__m128 b2 = _mm_loadu_ps(reinterpret_cast<const float*>(buf + x + 32));
		__m128 b3 = _mm_loadu_ps(reinterpret_cast<const float*>(buf + x + 48));
		__m128 ta, tb;
		if constexpr (REVERSE) {
			ta = _mm_loadr_ps(reinterpret_cast<const float*>(tab - 16));
			tb = _mm_loadr_ps(reinterpret_cast<const float*>(tab - 32));
			tab -= 2 * sizeof(__m128);
		} else {
			ta = _mm_load_ps (reinterpret_cast<const float*>(tab +  0));
			tb = _mm_load_ps (reinterpret_cast<const float*>(tab + 16));
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
		__m128 b0 = _mm_loadu_ps(reinterpret_cast<const float*>(buf +  0));
		__m128 b1 = _mm_loadu_ps(reinterpret_cast<const float*>(buf + 16));
		__m128 ta;
		if constexpr (REVERSE) {
			ta = _mm_loadr_ps(reinterpret_cast<const float*>(tab - 16));
		} else {
			ta = _mm_load_ps (reinterpret_cast<const float*>(tab +  0));
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

#endif

template<unsigned CHANNELS>
void ResampleHQ<CHANNELS>::calcOutput(
	float pos, float* __restrict output)
{
	assert((filterLen & 3) == 0);

	int bufIdx = int(pos) + bufStart;
	assert((bufIdx + filterLen) <= bufEnd);
	bufIdx *= CHANNELS;
	const float* buf = &buffer[bufIdx];

	int t = unsigned(lrintf(pos * TAB_LEN)) % TAB_LEN;
	if (!(t & HALF_TAB_LEN)) {
		// first half, begin of row 't'
		t = permute[t];
		const float* tab = &table[t * filterLen];

#ifdef __SSE2__
		if constexpr (CHANNELS == 1) {
			calcSseMono  <false>(buf, tab, filterLen, output);
		} else {
			calcSseStereo<false>(buf, tab, filterLen, output);
		}
		return;
#endif

		// c++ version, both mono and stereo
		for (auto ch : xrange(CHANNELS)) {
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
			output[ch] = r0 + r1 + r2 + r3;
			++buf;
		}
	} else {
		// 2nd half, end of row 'TAB_LEN - 1 - t'
		t = permute[TAB_LEN - 1 - t];
		const float* tab = &table[(t + 1) * filterLen];

#ifdef __SSE2__
		if constexpr (CHANNELS == 1) {
			calcSseMono  <true>(buf, tab, filterLen, output);
		} else {
			calcSseStereo<true>(buf, tab, filterLen, output);
		}
		return;
#endif

		// c++ version, both mono and stereo
		for (auto ch : xrange(CHANNELS)) {
			float r0 = 0.0f;
			float r1 = 0.0f;
			float r2 = 0.0f;
			float r3 = 0.0f;
			for (int i = 0; i < int(filterLen); i += 4) {
				r0 += tab[-i - 1] * buf[CHANNELS * (i + 0)];
				r1 += tab[-i - 2] * buf[CHANNELS * (i + 1)];
				r2 += tab[-i - 3] * buf[CHANNELS * (i + 2)];
				r3 += tab[-i - 4] * buf[CHANNELS * (i + 3)];
			}
			output[ch] = r0 + r1 + r2 + r3;
			++buf;
		}
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
	VLA_SSE_ALIGNED(float, tmpBuf, emuNum * CHANNELS + 3);
	if (input.generateInput(tmpBuf, emuNum)) {
		memcpy(&buffer[bufEnd * CHANNELS], tmpBuf,
		       emuNum * CHANNELS * sizeof(float));
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

template<unsigned CHANNELS>
bool ResampleHQ<CHANNELS>::generateOutputImpl(
	float* __restrict dataOut, unsigned hostNum, EmuTime::param time)
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
		float pos = emuClk.getTicksTillDouble(host1);
		assert(pos <= (ratio + 2));
		for (auto i : xrange(hostNum)) {
			calcOutput(pos, &dataOut[i * CHANNELS]);
			pos += ratio;
		}
	}
	emuClk += emuNum;
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
