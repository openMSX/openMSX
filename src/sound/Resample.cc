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


#include "Resample.hh"
#include "MemoryOps.hh"
#include "HostCPU.hh"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <cassert>

namespace openmsx {

static const float coeffs[] = {
	#include "ResampleCoeffs.ii"
};
static const int INDEX_INC = 128;
static const int COEFF_LEN = sizeof(coeffs) / sizeof(float);
static const int COEFF_HALF_LEN = COEFF_LEN - 1;
static const unsigned TAB_LEN = 4096;

template <unsigned CHANNELS>
Resample<CHANNELS>::Resample()
	: increment(0), table(0)
{
	ratio = 1.0f;
	lastPos = 0.0f;
	bufCurrent = BUF_LEN / 2;
	bufEnd     = BUF_LEN / 2;
	memset(buffer, 0, sizeof(buffer));
	nonzeroSamples = 0;
}

template <unsigned CHANNELS>
Resample<CHANNELS>::~Resample()
{
	if (table) {
		MemoryOps::freeAligned(table);
	}
}

template <unsigned CHANNELS>
void Resample<CHANNELS>::setResampleRatio(double inFreq, double outFreq)
{
	ratio = inFreq / outFreq;
	bufCurrent = BUF_LEN / 2;
	bufEnd     = BUF_LEN / 2;
	memset(buffer, 0, sizeof(buffer));

	// check the sample rate ratio wrt the buffer len
	double count = (COEFF_HALF_LEN + 2.0) / INDEX_INC;
	if (ratio > 1.0f) {
		count *= ratio;
	}
	// maximum coefficients on either side of center point
	halfFilterLen = lrint(count) + 1;

	floatIncr = (ratio > 1.0f) ? INDEX_INC / ratio : INDEX_INC;
	normFactor = floatIncr / INDEX_INC;
	increment = FilterIndex(floatIncr);

	calculateCoeffs();
}

template <unsigned CHANNELS>
double Resample<CHANNELS>::getCoeff(FilterIndex index)
{
	double fraction = index.fractionAsDouble();
	int indx = index.toInt();
	return coeffs[indx] + fraction * (coeffs[indx + 1] - coeffs[indx]);
}

template <unsigned CHANNELS>
void Resample<CHANNELS>::calculateCoeffs()
{
	FilterIndex maxFilterIndex(COEFF_HALF_LEN);
	int min_idx = -maxFilterIndex.divAsInt(increment);
	int max_idx = 1 + (maxFilterIndex - (increment - FilterIndex(floatIncr))).divAsInt(increment);
	int idx_cnt = max_idx - min_idx + 1;
	filterLen = (idx_cnt + 3) & ~3; // round up to multiple of 4
	min_idx -= (filterLen - idx_cnt);
	if (table) {
		MemoryOps::freeAligned(table);
	}
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
				getCoeff(filterIndex) * normFactor;
			filterIndex -= increment;
			bufIndex += 1;
		} while (filterIndex >= FilterIndex(0));

		filterIndex = increment - startFilterIndex;
		coeffCount = (maxFilterIndex - filterIndex).divAsInt(increment);
		filterIndex += increment * coeffCount;
		bufIndex = 1 + coeffCount;
		do {
			table[t * filterLen + bufIndex - min_idx] =
				getCoeff(filterIndex) * normFactor;
			filterIndex -= increment;
			bufIndex -= 1;
		} while (filterIndex > FilterIndex(0));
	}
}

template <unsigned CHANNELS>
void Resample<CHANNELS>::calcOutput2(float lastPos, float* output)
{
	assert((filterLen & 3) == 0);
	int t = static_cast<int>(lastPos * TAB_LEN + 0.5f) % TAB_LEN;

	int tabIdx = t * filterLen;
	int bufIdx = (bufCurrent - halfFilterLen) * CHANNELS;

	#ifdef ASM_X86
	const HostCPU& cpu = HostCPU::getInstance();
	if ((CHANNELS == 1) && cpu.hasSSE()) {
		// SSE version, mono
		long filterLen16 = filterLen & ~15;
		unsigned filterLenRest = filterLen - filterLen16;
		asm (
			"xorps	%%xmm0,%%xmm0;"
			"xorps	%%xmm1,%%xmm1;"
			"xorps	%%xmm2,%%xmm2;"
			"xorps	%%xmm3,%%xmm3;"
		"1:"
			"movups	  (%0,%3),%%xmm4;"
			"mulps	  (%1,%3),%%xmm4;"
			"movups	16(%0,%3),%%xmm5;"
			"mulps	16(%1,%3),%%xmm5;"
			"movups	32(%0,%3),%%xmm6;"
			"mulps	32(%1,%3),%%xmm6;"
			"movups	48(%0,%3),%%xmm7;"
			"mulps	48(%1,%3),%%xmm7;"
			"addps	%%xmm4,%%xmm0;"
			"addps	%%xmm5,%%xmm1;"
			"addps	%%xmm6,%%xmm2;"
			"addps	%%xmm7,%%xmm3;"
			"add	$64,%3;"
			"jnz	1b;"

			"test	$8,%4;"
			"jz	2f;"
			"movups	  (%0,%3), %%xmm4;"
			"mulps	  (%1,%3), %%xmm4;"
			"movups	16(%0,%3), %%xmm5;"
			"mulps	16(%1,%3), %%xmm5;"
			"addps	%%xmm4,%%xmm0;"
			"addps	%%xmm5,%%xmm1;"
			"add	$32,%3;"
		"2:"
			"test	$4,%4;"
			"jz	3f;"
			"movups	  (%0,%3), %%xmm6;"
			"mulps	  (%1,%3), %%xmm6;"
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
			"movss	%%xmm0,(%2);"

			: // no output
			: "r" (&buffer[bufIdx + filterLen16]) // 0
			, "r" (&table[tabIdx + filterLen16]) // 1
			, "r" (output) // 2
			, "r" (-4 * filterLen16) // 3
			, "r" (filterLenRest) // 4
			#ifdef __SSE__
			: "xmm0", "xmm1", "xmm2", "xmm3"
			, "xmm4", "xmm5", "xmm6", "xmm7"
			#endif
		);
		return;
	}
	
	if ((CHANNELS == 2) && cpu.hasSSE()) {
		// SSE version, stereo
		long filterLen8 = filterLen & ~7;
		unsigned filterLenRest = filterLen - filterLen8;
		asm (
			"xorps	%%xmm0,%%xmm0;"
			"xorps	%%xmm1,%%xmm1;"
			"xorps	%%xmm2,%%xmm2;"
			"xorps	%%xmm3,%%xmm3;"
		"1:"
			"movups	  (%0,%3,2),%%xmm4;"
			"movups	16(%0,%3,2),%%xmm5;"
			"movaps	  (%1,%3),%%xmm6;"
			"movaps	%%xmm6,%%xmm7;"
			"shufps	 $80,%%xmm6,%%xmm6;"
			"shufps	$250,%%xmm7,%%xmm7;"
			"mulps	%%xmm4,%%xmm6;"
			"mulps	%%xmm5,%%xmm7;"
			"addps	%%xmm6,%%xmm0;"
			"addps	%%xmm7,%%xmm1;"

			"movups	32(%0,%3,2),%%xmm4;"
			"movups	48(%0,%3,2),%%xmm5;"
			"movaps	16(%1,%3),%%xmm6;"
			"movaps	%%xmm6,%%xmm7;"
			"shufps	 $80,%%xmm6,%%xmm6;"
			"shufps	$250,%%xmm7,%%xmm7;"
			"mulps	%%xmm4,%%xmm6;"
			"mulps	%%xmm5,%%xmm7;"
			"addps	%%xmm6,%%xmm2;"
			"addps	%%xmm7,%%xmm3;"

			"add	$32,%3;"
			"jnz	1b;"

			"test	$4,%4;"
			"jz	2f;"
			"movups	  (%0,%3,2),%%xmm4;"
			"movups	16(%0,%3,2),%%xmm5;"
			"movaps	  (%1,%3),%%xmm6;"
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
			"movq	%%xmm0,(%2);"

			: // no output
			: "r" (&buffer[bufIdx + 2 * filterLen8]) // 0
			, "r" (&table[tabIdx + filterLen8]) // 1
			, "r" (output) // 2
			, "r" (-4 * filterLen8) // 3
			, "r" (filterLenRest) // 4
			#ifdef __SSE__
			: "xmm0", "xmm1", "xmm2", "xmm3"
			, "xmm4", "xmm5", "xmm6", "xmm7"
			#endif
		);
		return;
	}
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
		output[ch] = r0 + r1 + r2 + r3;
		++bufIdx;
	}
}

template <unsigned CHANNELS>
void Resample<CHANNELS>::calcOutput(FilterIndex startFilterIndex, float* output)
{
	FilterIndex maxFilterIndex(COEFF_HALF_LEN);

	// apply the left half of the filter
	FilterIndex filterIndex(startFilterIndex);
	int coeffCount = (maxFilterIndex - filterIndex).divAsInt(increment);
	filterIndex += increment * coeffCount;
	int bufIndex = (bufCurrent - coeffCount) * CHANNELS;

	float left[CHANNELS];
	for (unsigned i = 0; i < CHANNELS; ++i) {
		left[i] = 0.0f;
	}
	do {
		float fraction = filterIndex.fractionAsFloat();
		int indx = filterIndex.toInt();
		float icoeff = coeffs[indx] +
		                fraction * (coeffs[indx + 1] - coeffs[indx]);
		for (unsigned i = 0; i < CHANNELS; ++i) {
			left[i] += icoeff * buffer[bufIndex + i];
		}
		filterIndex -= increment;
		bufIndex += CHANNELS;
	} while (filterIndex >= FilterIndex(0));

	// apply the right half of the filter
	filterIndex = increment - startFilterIndex;
	coeffCount = (maxFilterIndex - filterIndex).divAsInt(increment);
	filterIndex += increment * coeffCount;
	bufIndex = (bufCurrent + (1 + coeffCount)) * CHANNELS;

	float right[CHANNELS];
	for (unsigned i = 0; i < CHANNELS; ++i) {
		right[i] = 0.0f;
	}
	do {
		float fraction = filterIndex.fractionAsFloat();
		int indx = filterIndex.toInt();
		float icoeff = coeffs[indx] +
		                fraction * (coeffs[indx + 1] - coeffs[indx]);
		for (unsigned i = 0; i < CHANNELS; ++i) {
			right[i] += icoeff * buffer[bufIndex + i];
		}
		filterIndex -= increment;
		bufIndex -= CHANNELS;
	} while (filterIndex > FilterIndex(0));

	for (unsigned i = 0; i < CHANNELS; ++i) {
		output[i] = (left[i] + right[i]) * normFactor;
	}
}

template <unsigned CHANNELS>
void Resample<CHANNELS>::prepareData(unsigned extra)
{
	assert(bufCurrent <= bufEnd);
	assert(bufEnd <= BUF_LEN);
	assert(halfFilterLen <= bufCurrent);

	unsigned available = bufEnd - bufCurrent;
	unsigned request = halfFilterLen + extra;
	int missing = request - available;
	assert(missing > 0);

	unsigned free = BUF_LEN - bufEnd;
	int overflow = missing - free;
	if (overflow > 0) {
		// close to end, restart at begin
		memmove(buffer,
			buffer + (bufCurrent - halfFilterLen) * CHANNELS,
			(halfFilterLen + available) * sizeof(float) * CHANNELS);
		bufCurrent = halfFilterLen;
		bufEnd = halfFilterLen + available;
		missing = std::min<unsigned>(missing, BUF_LEN - bufEnd);
	}
	if (generateInput(buffer + bufEnd * CHANNELS, missing)) {
		bufEnd += missing;
		nonzeroSamples = bufEnd - bufCurrent + halfFilterLen;
	} else {
		memset(buffer + bufEnd * CHANNELS, 0,
		       missing * sizeof(float));
		bufEnd += missing;
	}

	assert(bufCurrent + halfFilterLen <= bufEnd);
	assert(bufEnd <= BUF_LEN);
}

template <unsigned CHANNELS>
bool Resample<CHANNELS>::generateOutput(float* dataOut, unsigned num)
{
	bool anyNonZero = false;

	// main processing loop
	for (unsigned i = 0; i < num; ++i) {
		// need to reload buffer?
		assert(bufCurrent <= bufEnd);
		int available = bufEnd - bufCurrent;
		if (available <= (int)halfFilterLen) {
			int extra = (ratio > 1.0f)
			          ? lrint((num - i) * ratio) + 1
			          :       (num - i);
			prepareData(extra);
		}
		if (nonzeroSamples) {
			// -- old implementation
			//FilterIndex startFilterIndex(lastPos * floatIncr);
			//calcOutput(startFilterIndex, &dataOut[i * CHANNELS]);
			// -- new implementation
			calcOutput2(lastPos, &dataOut[i * CHANNELS]);
			// --
			anyNonZero = true;
		} else {
			for (unsigned j = 0; j < CHANNELS; ++j) {
				dataOut[i * CHANNELS + j] = 0.0f;
			}
		}

		// figure out the next index
		lastPos += ratio;
		assert(lastPos >= 0.0f);
		float intPos = truncf(lastPos);
		lastPos -= intPos;
		int consumed = lrint(intPos);
		bufCurrent += consumed;
		nonzeroSamples = std::max<int>(0, nonzeroSamples - consumed);
		assert(bufCurrent <= bufEnd);
	}
	return anyNonZero;
}

// Force template instantiation.
template class Resample<1>;
template class Resample<2>;

} // namespace openmsx
