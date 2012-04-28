// $Id$

// Based on libsamplerate-0.1.2 (aka Secret Rabit Code)
//
//  simplified code in several ways:
//   - resample algorithm is no longer switchable, we took this variant:
//        Band limited sinc interpolation, fastest, 97dB SNR, 80% BW
//   - only handle a single channel (mono)
//   - don't allow to change sample rate on-the-fly
//   - assume input (and thus also output) signals have infinte length, so
//     there is no special code to handle the ending of the signal
//   - changed/simplified API to better match openmsx use model
//     (e.g. remove all error checking)

#include "ResampleHQ.hh"
#include "ResampledSoundDevice.hh"
#include "FixedPoint.hh"
#include "MemoryOps.hh"
#include "HostCPU.hh"
#include "noncopyable.hh"
#include "vla.hh"
#include "countof.hh"
#include "likely.hh"
#include "build-info.hh"
#include <algorithm>
#include <map>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <cassert>

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

// Assembly functions
#ifdef _MSC_VER
extern "C"
{
	void __cdecl ResampleHQ_calcOutput_1_SSE(
		const void* bufferOffset, const void* tableOffset,
		void* output, long filterLen16Product, unsigned filterLenRest);
}
#endif

class ResampleCoeffs : private noncopyable
{
public:
	static ResampleCoeffs& instance();
	void getCoeffs(double ratio, float*& table, unsigned& filterLen);
	void releaseCoeffs(double ratio);

private:
	typedef FixedPoint<16> FilterIndex;

	ResampleCoeffs();
	~ResampleCoeffs();

	double getCoeff(FilterIndex index);
	void calcTable(double ratio, float*& table, unsigned& filterLen);

	struct Element {
		float* table;
		unsigned count;
		unsigned filterLen;
	};
	typedef std::map<double, Element> Cache;
	Cache cache;
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
	Cache::iterator it = cache.find(ratio);
	if (it != cache.end()) {
		it->second.count++;
		table     = it->second.table;
		filterLen = it->second.filterLen;
		return;
	}
	calcTable(ratio, table, filterLen);
	Element elem;
	elem.count = 1;
	elem.table = table;
	elem.filterLen = filterLen;
	cache[ratio] = elem;
}

void ResampleCoeffs::releaseCoeffs(double ratio)
{
	Cache::iterator it = cache.find(ratio);
	assert(it != cache.end());
	it->second.count--;
	if (it->second.count == 0) {
		MemoryOps::freeAligned(it->second.table);
		cache.erase(it);
	}
}

double ResampleCoeffs::getCoeff(FilterIndex index)
{
	double fraction = index.fractionAsDouble();
	int indx = index.toInt();
	return coeffs[indx] + fraction * (coeffs[indx + 1] - coeffs[indx]);
}

void ResampleCoeffs::calcTable(
	double ratio, float*& table, unsigned& filterLen)
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
	table = static_cast<float*>(MemoryOps::mallocAligned(
		16, TAB_LEN * filterLen * sizeof(float)));
	memset(table, 0, TAB_LEN * filterLen * sizeof(float));

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

template <unsigned CHANNELS>
void ResampleHQ<CHANNELS>::calcOutput(
	float pos, int* __restrict output) __restrict
{
	assert((filterLen & 3) == 0);
	int t = int(pos * TAB_LEN + 0.5f) % TAB_LEN;

	int tabIdx = t * filterLen;
	int bufIdx = int(pos) + bufStart;
	assert((bufIdx + filterLen) <= bufEnd);
	bufIdx *= CHANNELS;

	#if ASM_X86 && !defined(__APPLE__)
	// On Mac OS X, we are one register short, because EBX is not available.
	// We disable this piece of assembly and fall back to the C++ code.
	const HostCPU& cpu = HostCPU::getInstance();
	if ((CHANNELS == 1) && cpu.hasSSE()) {
		// SSE version, mono
		long filterLen16 = filterLen & ~15;
		unsigned filterLenRest = filterLen - filterLen16;
	#ifdef _MSC_VER
		// It's quite inelegant to execute these computations outside
		// the ASM function, but it does reduce the number of parameters
		// passed to the function from 8 to 5
		ResampleHQ_calcOutput_1_SSE(
			&buffer[bufIdx + filterLen16],
			&table[tabIdx + filterLen16],
			output,
			-4 * filterLen16,
			filterLenRest);
		return;
	}
	#else
		long dummy1;
		unsigned dummy2;
		asm volatile (
			"xorps	%%xmm0,%%xmm0;"
			"xorps	%%xmm1,%%xmm1;"
			"xorps	%%xmm2,%%xmm2;"
			"xorps	%%xmm3,%%xmm3;"
		"1:"
			"movups	  (%[BUF],%[FL16]),%%xmm4;"
			"mulps	  (%[TAB],%[FL16]),%%xmm4;"
			"movups	16(%[BUF],%[FL16]),%%xmm5;"
			"mulps	16(%[TAB],%[FL16]),%%xmm5;"
			"movups	32(%[BUF],%[FL16]),%%xmm6;"
			"mulps	32(%[TAB],%[FL16]),%%xmm6;"
			"movups	48(%[BUF],%[FL16]),%%xmm7;"
			"mulps	48(%[TAB],%[FL16]),%%xmm7;"
			"addps	%%xmm4,%%xmm0;"
			"addps	%%xmm5,%%xmm1;"
			"addps	%%xmm6,%%xmm2;"
			"addps	%%xmm7,%%xmm3;"
			"add	$64,%[FL16];"
			"jnz	1b;"

			"test	$8,%[FLR];"
			"jz	2f;"
			"movups	  (%[BUF],%[FL16]), %%xmm4;"
			"mulps	  (%[TAB],%[FL16]), %%xmm4;"
			"movups	16(%[BUF],%[FL16]), %%xmm5;"
			"mulps	16(%[TAB],%[FL16]), %%xmm5;"
			"addps	%%xmm4,%%xmm0;"
			"addps	%%xmm5,%%xmm1;"
			"add	$32,%[FL16];"
		"2:"
			"test	$4,%[FLR];"
			"jz	3f;"
			"movups	  (%[BUF],%[FL16]), %%xmm6;"
			"mulps	  (%[TAB],%[FL16]), %%xmm6;"
			"addps	%%xmm6,%%xmm2;"
		"3:"
			"addps	%%xmm1,%%xmm0;"
			"addps	%%xmm3,%%xmm2;"
			"addps	%%xmm2,%%xmm0;"
			"movaps	%%xmm0,%%xmm7;"
			"shufps	$78,%%xmm0,%%xmm7;"
			"addps	%%xmm0,%%xmm7;"
			"movaps	%%xmm7,%%xmm0;"
			"shufps	$177,%%xmm7,%%xmm0;"
			"addss	%%xmm7,%%xmm0;"
			"cvtss2si %%xmm0,%[TMP];"
			"mov	%[TMP],(%[OUT]);"

			: [FL16] "=r"     (dummy1)
			, [TMP]  "=&r"    (dummy2)
			: [BUF]  "r"      (&buffer[bufIdx + filterLen16])
			, [TAB]  "r"      (&table[tabIdx + filterLen16])
			, [OUT]  "r"      (output)
			,        "[FL16]" (-4 * filterLen16)
			, [FLR]  "r"      (filterLenRest)
			: "memory"
			#ifdef __SSE__
			, "xmm0", "xmm1", "xmm2", "xmm3"
			, "xmm4", "xmm5", "xmm6", "xmm7"
			#endif
		);
		return;
	}

	if ((CHANNELS == 2) && cpu.hasSSE()) {
		// SSE version, stereo
		long filterLen8 = filterLen & ~7;
		unsigned filterLenRest = filterLen - filterLen8;
		long dummy;
		asm volatile (
			"xorps	%%xmm0,%%xmm0;"
			"xorps	%%xmm1,%%xmm1;"
			"xorps	%%xmm2,%%xmm2;"
			"xorps	%%xmm3,%%xmm3;"
		"1:"
			"movups	  (%[BUF],%[FL8],2),%%xmm4;"
			"movups	16(%[BUF],%[FL8],2),%%xmm5;"
			"movaps	  (%[TAB],%[FL8]),%%xmm6;"
			"movaps	%%xmm6,%%xmm7;"
			"shufps	 $80,%%xmm6,%%xmm6;"
			"shufps	$250,%%xmm7,%%xmm7;"
			"mulps	%%xmm4,%%xmm6;"
			"mulps	%%xmm5,%%xmm7;"
			"addps	%%xmm6,%%xmm0;"
			"addps	%%xmm7,%%xmm1;"

			"movups	32(%[BUF],%[FL8],2),%%xmm4;"
			"movups	48(%[BUF],%[FL8],2),%%xmm5;"
			"movaps	16(%[TAB],%[FL8]),%%xmm6;"
			"movaps	%%xmm6,%%xmm7;"
			"shufps	 $80,%%xmm6,%%xmm6;"
			"shufps	$250,%%xmm7,%%xmm7;"
			"mulps	%%xmm4,%%xmm6;"
			"mulps	%%xmm5,%%xmm7;"
			"addps	%%xmm6,%%xmm2;"
			"addps	%%xmm7,%%xmm3;"

			"add	$32,%[FL8];"
			"jnz	1b;"

			"test	$4,%[FLR];"
			"jz	2f;"
			"movups	  (%[BUF],%[FL8],2),%%xmm4;"
			"movups	16(%[BUF],%[FL8],2),%%xmm5;"
			"movaps	  (%[TAB],%[FL8]),%%xmm6;"
			"movaps	%%xmm6,%%xmm7;"
			"shufps	 $80,%%xmm6,%%xmm6;"
			"shufps	$250,%%xmm7,%%xmm7;"
			"mulps	%%xmm4,%%xmm6;"
			"mulps	%%xmm5,%%xmm7;"
			"addps	%%xmm6,%%xmm0;"
			"addps	%%xmm7,%%xmm1;"
		"2:"
			"addps	%%xmm3,%%xmm2;"
			"addps	%%xmm1,%%xmm0;"
			"addps	%%xmm2,%%xmm0;"
			"movaps	%%xmm0,%%xmm4;"
			"shufps	$78,%%xmm0,%%xmm0;"
			"addps	%%xmm4,%%xmm0;"
			"cvtps2pi %%xmm0,%%mm0;"
			"movq	%%mm0,(%[OUT]);"
			"emms;"

			: [FL8] "=r"    (dummy)
			: [BUF] "r"     (&buffer[bufIdx + 2 * filterLen8])
			, [TAB] "r"     (&table[tabIdx + filterLen8])
			, [OUT] "r"     (output)
			,       "[FL8]" (-4 * filterLen8)
			, [FLR] "r"     (filterLenRest) // 4
			: "memory"
			#ifdef __SSE__
			, "mm0"
			, "xmm0", "xmm1", "xmm2", "xmm3"
			, "xmm4", "xmm5", "xmm6", "xmm7"
			#endif
		);
		return;
	}
	#endif
	#endif

	// c++ version, both mono and stereo
	for (unsigned ch = 0; ch < CHANNELS; ++ch) {
		float r0 = 0.0f;
		float r1 = 0.0f;
		float r2 = 0.0f;
		float r3 = 0.0f;
		for (unsigned i = 0; i < filterLen; i += 4) {
			r0 += table[tabIdx + i + 0] *
			      buffer[bufIdx + CHANNELS * (i + 0)];
			r1 += table[tabIdx + i + 1] *
			      buffer[bufIdx + CHANNELS * (i + 1)];
			r2 += table[tabIdx + i + 2] *
			      buffer[bufIdx + CHANNELS * (i + 2)];
			r3 += table[tabIdx + i + 3] *
			      buffer[bufIdx + CHANNELS * (i + 3)];
		}
		output[ch] = lrint(r0 + r1 + r2 + r3);
		++bufIdx;
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
#if ASM_X86
	VLA_ALIGNED(int, tmpBuf, emuNum * CHANNELS + 3, 16);
#else
	VLA(int, tmpBuf, emuNum * CHANNELS + 3);
#endif
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
	int* __restrict dataOut, unsigned hostNum, EmuTime::param time) __restrict
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
