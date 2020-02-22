#include "BlipBuffer.hh"
#include "cstd.hh"
#include "likely.hh"
#include "Math.hh"
#include <algorithm>
#include <cstring>
#include <cassert>
#include <iostream>

namespace openmsx {

// The input sample stream can only use this many bits out of the available 32
// bits. So 29 bits means the sample values must be in range [-256M, 256M].
constexpr int BLIP_SAMPLE_BITS = 29;

// Number of samples in a (pre-calculated) impulse-response wave-form.
constexpr int BLIP_IMPULSE_WIDTH = 16;

// Derived constants
constexpr int BLIP_RES = 1 << BlipBuffer::BLIP_PHASE_BITS;


// Precalculated impulse table.
struct Impulses {
	float a[BLIP_RES][BLIP_IMPULSE_WIDTH];
};
static constexpr Impulses calcImpulses()
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
	double rescale = 1.0 / (2.0 * total);

	// integrate, first difference, rescale, convert to float
	constexpr int IMPULSES_SIZE = BLIP_RES * (BLIP_IMPULSE_WIDTH / 2) + 1;
	float imp[IMPULSES_SIZE] = {};
	double sum = 0.0;
	double next = 0.0;
	for (int i = 0; i < IMPULSES_SIZE; ++i) {
		imp[i] = float((next - sum) * rescale);
		sum += fimpulse[i];
		next += fimpulse[i + BLIP_RES];
	}
	// Original code would now apply a correction on each kernel so that
	// the (integer) coefficients sum up to 'kernelUnit'. I've measured
	// that after switching to float coefficients this correction is
	// roughly equal to std::numeric_limits<float>::epsilon(). So it's no
	// longer meaningful.

	// reshuffle values to a more cache friendly order
	Impulses impulses = {};
	for (int phase = 0; phase < BLIP_RES; ++phase) {
		const auto* imp_fwd = &imp[BLIP_RES - phase];
		const auto* imp_rev = &imp[phase];
		auto* p = impulses.a[phase];
		for (int i = 0; i < BLIP_IMPULSE_WIDTH / 2; ++i) {
			*p++ = imp_fwd[BLIP_RES * i];
		}
		for (int i = BLIP_IMPULSE_WIDTH / 2 - 1; i >= 0; --i) {
			*p++ = imp_rev[BLIP_RES * i];
		}
	}
	return impulses;
}
constexpr Impulses impulses = calcImpulses();


BlipBuffer::BlipBuffer()
{
	if (false) {
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

void BlipBuffer::addDelta(TimeIndex time, float delta)
{
	unsigned tmp = time.toInt() + BLIP_IMPULSE_WIDTH;
	assert(tmp < BUFFER_SIZE);
	availSamp = std::max<int>(availSamp, tmp);

	unsigned phase = time.fractAsInt();
	unsigned ofst = time.toInt() + offset;
	const float* __restrict impulse = impulses.a[phase];
	if (likely((ofst + BLIP_IMPULSE_WIDTH) <= BUFFER_SIZE)) {
		float* __restrict result = &buffer[ofst];
		for (int i = 0; i < BLIP_IMPULSE_WIDTH; ++i) {
			result[i] += impulse[i] * delta;
		}
	} else {
		for (int i = 0; i < BLIP_IMPULSE_WIDTH; ++i) {
			buffer[(ofst + i) & BUFFER_MASK] += impulse[i] * delta;
		}
	}
}

constexpr float BASS_FACTOR = 511.0f / 512.0f;

template<unsigned PITCH>
void BlipBuffer::readSamplesHelper(float* __restrict out, unsigned samples) __restrict
{
	assert((offset + samples) <= BUFFER_SIZE);
	auto acc = accum;
	unsigned ofst = offset;
	for (unsigned i = 0; i < samples; ++i) {
		out[i * PITCH] = acc;
		acc *= BASS_FACTOR;
		acc += buffer[ofst];
		buffer[ofst] = 0.0f;
		++ofst;
	}
	accum = acc;
	offset = ofst & BUFFER_MASK;
}

static bool isSilent(float x)
{
	// 'accum' falls away slowly (via BASS_FACTOR). Because it's a float it
	// takes a long time before it's really zero. But much sooner we can
	// already say it's practically silent. This check is safe when the
	// input is in range [-1..+1] (does not say silent too soon). When the
	// input is in range [-32768..+32767] it takes longer before we switch
	// to silent mode (but still less than a second).
	constexpr float threshold = 1.0f / 32768;
	return std::abs(x) < threshold;
}

template <unsigned PITCH>
bool BlipBuffer::readSamples(float* __restrict out, unsigned samples)
{
	if (availSamp <= 0) {
		#ifdef DEBUG
		// buffer contains all zeros (only check this in debug mode)
		for (unsigned i = 0; i < BUFFER_SIZE; ++i) {
			assert(buffer[i] == 0.0f);
		}
		#endif
		if (isSilent(accum)) {
			return false; // muted
		}
		auto acc = accum;
		for (unsigned i = 0; i < samples; ++i) {
			out[i * PITCH] = acc;
			acc *= BASS_FACTOR;
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

template bool BlipBuffer::readSamples<1>(float*, unsigned);
template bool BlipBuffer::readSamples<2>(float*, unsigned);

} // namespace openmsx
