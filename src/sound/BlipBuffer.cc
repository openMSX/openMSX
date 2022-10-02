#include "BlipBuffer.hh"
#include "cstd.hh"
#include "Math.hh"
#include "ranges.hh"
#include "xrange.hh"
#include <algorithm>
#include <array>
#include <cassert>
#include <iostream>

namespace openmsx {

// Number of samples in a (pre-calculated) impulse-response wave-form.
static constexpr int BLIP_IMPULSE_WIDTH = 16;

// Derived constants
static constexpr int BLIP_RES = 1 << BlipBuffer::BLIP_PHASE_BITS;


// Precalculated impulse table.
static constexpr auto impulses = [] {
	constexpr int HALF_SIZE = BLIP_RES / 2 * (BLIP_IMPULSE_WIDTH - 1);
	std::array<double, BLIP_RES + HALF_SIZE + BLIP_RES> fImpulse = {};
	std::span<double, HALF_SIZE> out = subspan<HALF_SIZE>(fImpulse, BLIP_RES);
	std::span<double, BLIP_RES>  end = subspan<BLIP_RES >(fImpulse, BLIP_RES + HALF_SIZE);

	// generate sinc, apply hamming window
	double overSample = ((4.5 / (BLIP_IMPULSE_WIDTH - 1)) + 0.85);
	double to_angle = Math::pi / (2.0 * overSample * BLIP_RES);
	double to_fraction = Math::pi / (2 * (HALF_SIZE - 1));
	for (auto i : xrange(HALF_SIZE)) {
		double angle = ((i - HALF_SIZE) * 2 + 1) * to_angle;
		out[i] = cstd::sin<2>(angle) / angle;
		out[i] *= 0.54 - 0.46 * cstd::cos<2>((2 * i + 1) * to_fraction);
	}

	// need mirror slightly past center for calculation
	for (auto i : xrange(BLIP_RES)) {
		end[i] = out[HALF_SIZE - 1 - i];
	}

	// find rescale factor
	double total = 0.0;
	for (auto i : xrange(HALF_SIZE)) {
		total += out[i];
	}
	double rescale = 1.0 / (2.0 * total);

	// integrate, first difference, rescale, convert to float
	constexpr int IMPULSES_SIZE = BLIP_RES * (BLIP_IMPULSE_WIDTH / 2) + 1;
	std::array<float, IMPULSES_SIZE> imp = {};
	double sum = 0.0;
	double next = 0.0;
	for (auto i : xrange(IMPULSES_SIZE)) {
		imp[i] = float((next - sum) * rescale);
		sum += fImpulse[i];
		next += fImpulse[i + BLIP_RES];
	}
	// Original code would now apply a correction on each kernel so that
	// the (integer) coefficients sum up to 'kernelUnit'. I've measured
	// that after switching to float coefficients this correction is
	// roughly equal to std::numeric_limits<float>::epsilon(). So it's no
	// longer meaningful.

	// reshuffle values to a more cache friendly order
	std::array<std::array<float, BLIP_IMPULSE_WIDTH>, BLIP_RES> result = {};
	for (auto phase : xrange(BLIP_RES)) {
		const auto* imp_fwd = &imp[BLIP_RES - phase];
		const auto* imp_rev = &imp[phase];
		auto* p = result[phase].data();
		for (auto i : xrange(BLIP_IMPULSE_WIDTH / 2)) {
			*p++ = imp_fwd[BLIP_RES * i];
		}
		for (int i = BLIP_IMPULSE_WIDTH / 2 - 1; i >= 0; --i) {
			*p++ = imp_rev[BLIP_RES * i];
		}
	}
	return result;
}();


BlipBuffer::BlipBuffer()
{
	if (false) {
		for (auto i : xrange(BLIP_RES)) {
			std::cout << "\t{ " << impulses[i][0];
			for (auto j : xrange(1, BLIP_IMPULSE_WIDTH)) {
				std::cout << ", " << impulses[i][j];
			}
			std::cout << " },\n";
		}
	}

	offset = 0;
	accum = 0;
	availSamp = 0;
	ranges::fill(buffer, 0);
}

void BlipBuffer::addDelta(TimeIndex time, float delta)
{
	unsigned tmp = time.toInt() + BLIP_IMPULSE_WIDTH;
	assert(tmp < BUFFER_SIZE);
	availSamp = std::max<int>(availSamp, tmp);

	unsigned phase = time.fractAsInt();
	unsigned ofst = time.toInt() + offset;
	const float* __restrict impulse = impulses[phase].data();
	if ((ofst + BLIP_IMPULSE_WIDTH) <= BUFFER_SIZE) [[likely]] {
		float* __restrict result = &buffer[ofst];
		for (auto i : xrange(BLIP_IMPULSE_WIDTH)) {
			result[i] += impulse[i] * delta;
		}
	} else {
		for (auto i : xrange(BLIP_IMPULSE_WIDTH)) {
			buffer[(ofst + i) & BUFFER_MASK] += impulse[i] * delta;
		}
	}
}

static constexpr float BASS_FACTOR = 511.0f / 512.0f;

template<unsigned PITCH>
void BlipBuffer::readSamplesHelper(float* __restrict out, unsigned samples)
{
	assert((offset + samples) <= BUFFER_SIZE);
	auto acc = accum;
	unsigned ofst = offset;
	for (auto i : xrange(samples)) {
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

template<unsigned PITCH>
bool BlipBuffer::readSamples(float* __restrict out, unsigned samples)
{
	if (availSamp <= 0) {
		#ifdef DEBUG
		// buffer contains all zeros (only check this in debug mode)
		assert(ranges::all_of(buffer, [](const auto& b) { return b == 0.0f; }));
		#endif
		if (isSilent(accum)) {
			return false; // muted
		}
		auto acc = accum;
		for (auto i : xrange(samples)) {
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
