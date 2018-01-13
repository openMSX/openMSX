#include "BlipBuffer.hh"
#include "cstd.hh"
#include "likely.hh"
#include <algorithm>
#include <cstring>
#include <cassert>
#include <iostream>

namespace openmsx {

// The input sample stream can only use this many bits out of the available 32
// bits. So 29 bits means the sample values must be in range [-256M, 256M].
static constexpr int BLIP_SAMPLE_BITS = 29;

// Number of samples in a (pre-calculated) impulse-response wave-form.
static constexpr int BLIP_IMPULSE_WIDTH = 16;

// Derived constants
static constexpr int BLIP_RES = 1 << BlipBuffer::BLIP_PHASE_BITS;


// Precalculated impulse table.
struct Impulses {
	int a[BLIP_RES][BLIP_IMPULSE_WIDTH];
};
static CONSTEXPR Impulses calcImpulses()
{
	constexpr int HALF_SIZE = BLIP_RES / 2 * (BLIP_IMPULSE_WIDTH - 1);
	double fimpulse[HALF_SIZE + 2 * BLIP_RES] = {};
	double* out = &fimpulse[BLIP_RES];

	// generate sinc, apply hamming window
	double oversample = ((4.5 / (BLIP_IMPULSE_WIDTH - 1)) + 0.85);
	double to_angle = M_PI / (2.0 * oversample * BLIP_RES);
	double to_fraction = M_PI / (2 * (HALF_SIZE - 1));
	for (int i = 0; i < HALF_SIZE; ++i) {
		double angle = ((i - HALF_SIZE) * 2 + 1) * to_angle;
		out[i] = cstd::sin<2>(angle) / angle;
		out[i] *= 0.54 - 0.46 * cstd::cos<2>((2 * i + 1) * to_fraction);
	}

	// need mirror slightly past center for calculation
	for (int i = 0; i < BLIP_RES; ++i) {
		out[HALF_SIZE + i] = out[HALF_SIZE - 1 - i];
	}

	// find rescale factor
	double total = 0.0;
	for (int i = 0; i < HALF_SIZE; ++i) {
		total += out[i];
	}
	int kernelUnit = 1 << (BLIP_SAMPLE_BITS - 16);
	double rescale = kernelUnit / (2.0 * total);

	// integrate, first difference, rescale, convert to int
	constexpr int IMPULSES_SIZE = BLIP_RES * (BLIP_IMPULSE_WIDTH / 2) + 1;
	int imp[IMPULSES_SIZE] = {};
	double sum = 0.0;
	double next = 0.0;
	for (int i = 0; i < IMPULSES_SIZE; ++i) {
		imp[i] = cstd::round((next - sum) * rescale);
		sum += fimpulse[i];
		next += fimpulse[i + BLIP_RES];
	}

	// sum pairs for each phase and add error correction to end of first half
	for (int p = BLIP_RES; p-- >= BLIP_RES / 2; /**/) {
		int p2 = BLIP_RES - 2 - p;
		int error = kernelUnit;
		for (int i = 1; i < IMPULSES_SIZE; i += BLIP_RES) {
			error -= imp[i + p ];
			error -= imp[i + p2];
		}
		if (p == p2) {
			// phase = 0.5 impulse uses same half for both sides
			error /= 2;
		}
		imp[IMPULSES_SIZE - BLIP_RES + p] += error;
	}

	// reshuffle values to a more cache friendly order
	Impulses impulses = {};
	for (int phase = 0; phase < BLIP_RES; ++phase) {
		const int* imp_fwd = &imp[BLIP_RES - phase];
		const int* imp_rev = &imp[phase];
		int* p = impulses.a[phase];
		for (int i = 0; i < BLIP_IMPULSE_WIDTH / 2; ++i) {
			*p++ = imp_fwd[BLIP_RES * i];
		}
		for (int i = BLIP_IMPULSE_WIDTH / 2 - 1; i >= 0; --i) {
			*p++ = imp_rev[BLIP_RES * i];
		}
	}
	return impulses;
}
static CONSTEXPR Impulses impulses = calcImpulses();


BlipBuffer::BlipBuffer()
{
	if (0) {
		for (int i = 0; i < BLIP_RES; ++i) {
			std::cout << "\t{ " << impulses.a[i][0];
			for (int j = 1; j < BLIP_IMPULSE_WIDTH; ++j) {
				std::cout << ", " << impulses.a[i][j];
			}
			std::cout << " },\n";
		}
	}

	offset = 0;
	accum = 0;
	availSamp = 0;
	memset(buffer, 0, sizeof(buffer));
}

void BlipBuffer::addDelta(TimeIndex time, int delta)
{
	unsigned tmp = time.toInt() + BLIP_IMPULSE_WIDTH;
	assert(tmp < BUFFER_SIZE);
	availSamp = std::max<int>(availSamp, tmp);

	unsigned phase = time.fractAsInt();
	unsigned ofst = time.toInt() + offset;
	if (likely((ofst + BLIP_IMPULSE_WIDTH) <= BUFFER_SIZE)) {
		for (int i = 0; i < BLIP_IMPULSE_WIDTH; ++i) {
			buffer[ofst + i] += impulses.a[phase][i] * delta;
		}
	} else {
		for (int i = 0; i < BLIP_IMPULSE_WIDTH; ++i) {
			buffer[(ofst + i) & BUFFER_MASK] += impulses.a[phase][i] * delta;
		}
	}
}

static const int SAMPLE_SHIFT = BLIP_SAMPLE_BITS - 16;
static const int BASS_SHIFT = 9;

template<unsigned PITCH>
void BlipBuffer::readSamplesHelper(int* __restrict out, unsigned samples) __restrict
{
	assert((offset + samples) <= BUFFER_SIZE);
	int acc = accum;
	unsigned ofst = offset;
	for (unsigned i = 0; i < samples; ++i) {
		out[i * PITCH] = acc >> SAMPLE_SHIFT;
		// Note: the following has different rounding behaviour
		//  for positive and negative numbers! The original
		//  code used 'acc / (1<< BASS_SHIFT)' to avoid this,
		//  but it generates less efficient code.
		acc -= (acc >> BASS_SHIFT);
		acc += buffer[ofst];
		buffer[ofst] = 0;
		++ofst;
	}
	accum = acc;
	offset = ofst & BUFFER_MASK;
}

template <unsigned PITCH>
bool BlipBuffer::readSamples(int* __restrict out, unsigned samples)
{
	if (availSamp <= 0) {
		#ifdef DEBUG
		// buffer contains all zeros (only check this in debug mode)
		for (unsigned i = 0; i < BUFFER_SIZE; ++i) {
			assert(buffer[i] == 0);
		}
		#endif
		if (accum == 0) {
			// muted
			return false;
		}
		int acc = accum;
		for (unsigned i = 0; i < samples; ++i) {
			out[i * PITCH] = acc >> SAMPLE_SHIFT;
			// See note about rounding above.
			acc -= (acc >> BASS_SHIFT);
			acc -= (acc > 0) ? 1 : 0; // make sure acc eventually goes to zero
		}
		accum = acc;
	} else {
		availSamp -= samples;
		unsigned t1 = std::min(samples, BUFFER_SIZE - offset);
		readSamplesHelper<PITCH>(out, t1);
		if (t1 < samples) {
			assert(offset == 0);
			unsigned t2 = samples - t1;
			assert(t2 < BUFFER_SIZE);
			readSamplesHelper<PITCH>(&out[t1 * PITCH], t2);
		}
		assert(offset < BUFFER_SIZE);
	}
	return true;
}

template bool BlipBuffer::readSamples<1>(int*, unsigned);
template bool BlipBuffer::readSamples<2>(int*, unsigned);

} // namespace openmsx
