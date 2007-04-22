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

#define SHIFT_BITS          16
#define FP_ONE              ((double)(1 << SHIFT_BITS))
#define DOUBLE_TO_FP(x)     (lrint((x) * FP_ONE))
#define INT_TO_FP(x)        ((x) << SHIFT_BITS)
#define FP_FRACTION_PART(x) ((x) & ((1 << SHIFT_BITS) - 1))
#define FP_TO_INT(x)        (((x) >> SHIFT_BITS))
#define FP_TO_DOUBLE(x)     (FP_FRACTION_PART(x) / FP_ONE)

template <unsigned CHANNELS>
Resample<CHANNELS>::Resample()
{
	ratio = 1.0;
	lastPos = 0.0;
	bufCurrent = BUF_LEN / 2;
	bufEnd     = BUF_LEN / 2;
	memset(buffer, 0, sizeof(buffer));
}

template <unsigned CHANNELS>
Resample<CHANNELS>::~Resample()
{
}

template <unsigned CHANNELS>
void Resample<CHANNELS>::setResampleRatio(double inFreq, double outFreq)
{
	ratio = inFreq / outFreq;
	bufCurrent = BUF_LEN / 2;
	bufEnd     = BUF_LEN / 2;
	memset(buffer, 0, sizeof(buffer));
}

template <unsigned CHANNELS>
void Resample<CHANNELS>::calcOutput(
	int increment, int startFilterIndex, double normFactor, float* output)
{
	int maxFilterIndex = INT_TO_FP(COEFF_HALF_LEN);

	// apply the left half of the filter
	int filterIndex = startFilterIndex;
	int coeffCount = (maxFilterIndex - filterIndex) / increment;
	filterIndex = filterIndex + coeffCount * increment;
	int bufIndex = (bufCurrent - coeffCount) * CHANNELS;

	double left[CHANNELS];
	for (unsigned i = 0; i < CHANNELS; ++i) {
		left[i] = 0.0;
	}
	do {
		double fraction = FP_TO_DOUBLE(filterIndex);
		int indx = FP_TO_INT(filterIndex);
		double icoeff = coeffs[indx] +
		                fraction * (coeffs[indx + 1] - coeffs[indx]);
		for (unsigned i = 0; i < CHANNELS; ++i) {
			left[i] += icoeff * buffer[bufIndex + i];
		}
		filterIndex -= increment;
		bufIndex += CHANNELS;
	} while (filterIndex >= INT_TO_FP(0));

	// apply the right half of the filter
	filterIndex = increment - startFilterIndex;
	coeffCount = (maxFilterIndex - filterIndex) / increment;
	filterIndex = filterIndex + coeffCount * increment;
	bufIndex = (bufCurrent + (1 + coeffCount)) * CHANNELS;

	double right[CHANNELS];
	for (unsigned i = 0; i < CHANNELS; ++i) {
		right[i] = 0.0;
	}
	do {
		double fraction = FP_TO_DOUBLE(filterIndex);
		int indx = FP_TO_INT(filterIndex);
		double icoeff = coeffs[indx] + 
		                fraction * (coeffs[indx + 1] - coeffs[indx]);
		for (unsigned i = 0; i < CHANNELS; ++i) {
			right[i] += icoeff * buffer[bufIndex + i];
		}
		filterIndex -= increment;
		bufIndex -= CHANNELS;
	} while (filterIndex > INT_TO_FP(0));

	for (unsigned i = 0; i < CHANNELS; ++i) {
		output[i] = (left[i] + right[i]) * normFactor;
	}
}

template <unsigned CHANNELS>
void Resample<CHANNELS>::prepareData(unsigned halfFilterLen, unsigned extra)
{
	assert(bufCurrent <= bufEnd);
	assert(bufEnd <= BUF_LEN);
	assert(halfFilterLen <= bufCurrent);

	unsigned available = bufEnd - bufCurrent;
	unsigned request = halfFilterLen + extra;
	int missing = request - available;
	if (missing > 0) {
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
		generateInput(buffer + bufEnd * CHANNELS, missing);
		bufEnd += missing;
	}

	assert(bufCurrent + halfFilterLen <= bufEnd);
	assert(bufEnd <= BUF_LEN);
}

template <unsigned CHANNELS>
void Resample<CHANNELS>::generateOutput(float* dataOut, unsigned num)
{
	// check the sample rate ratio wrt the buffer len
	double count = (COEFF_HALF_LEN + 2.0) / INDEX_INC;
	if (ratio > 1.0) {
		count *= ratio;
	}

	// maximum coefficients on either side of center point
	int halfFilterLen = lrint(count) + 1;

	double inputIndex = lastPos;
	double floatIncr = (ratio > 1.0)
	                 ? INDEX_INC / ratio
	                 : INDEX_INC;
	double normFactor = floatIncr / INDEX_INC;
	int increment = DOUBLE_TO_FP(floatIncr);

	double rem = fmod(inputIndex, 1.0);
	bufCurrent = (bufCurrent + lrint(inputIndex - rem)) % BUF_LEN;
	inputIndex = rem;

	// main processing loop
	for (unsigned i = 0; i < num; ++i) {
		// need to reload buffer?
		int samplesInHand = (bufEnd - bufCurrent + BUF_LEN) % BUF_LEN;
		if (samplesInHand <= halfFilterLen) {
			int extra = (ratio > 1.0)
			          ? lrint((num - i) * ratio) + 1
			          :       (num - i);
			prepareData(halfFilterLen, extra);
		}

		int startFilterIndex = DOUBLE_TO_FP(inputIndex * floatIncr);
		calcOutput(increment, startFilterIndex, normFactor,
		           &dataOut[i * CHANNELS]);

		// figure out the next index
		inputIndex += ratio;
		rem = fmod(inputIndex, 1.0);
		bufCurrent = (bufCurrent + lrint(inputIndex - rem)) % BUF_LEN;
		inputIndex = rem;
	}
	lastPos = inputIndex;
}

// Force template instantiation.
template class Resample<1>;
template class Resample<2>;

} // namespace openmsx
