// $Id$

#include "BlipBuffer.hh"
#include <algorithm>
#include <cstring>
#include <cassert>
#include <cmath>

namespace openmsx {

static const int BLIP_SAMPLE_BITS = 29;
static const int BLIP_RES = 1 << BlipBuffer::BLIP_PHASE_BITS;
static const int IMPULSE_WIDTH = 16;

static int impulses[BLIP_RES][IMPULSE_WIDTH];

static void initImpulse()
{
	static bool alreadyInit = false;
	if (alreadyInit) return;
	alreadyInit = true;

	static const int HALF_SIZE = BLIP_RES / 2 * (IMPULSE_WIDTH - 1);
	double fimpulse[HALF_SIZE + 2 * BLIP_RES];
	double* out = &fimpulse[BLIP_RES];

	// generate sinc, apply hamming window
	double oversample = ((4.5 / (IMPULSE_WIDTH - 1)) + 0.85);
	double to_angle = M_PI / (2.0 * oversample * BLIP_RES);
	double to_fraction = M_PI / (2 * (HALF_SIZE - 1));
	for (int i = 0; i < HALF_SIZE; ++i) {
		double angle = ((i - HALF_SIZE) * 2 + 1) * to_angle;
		out[i] = sin(angle) / angle;
		out[i] *= 0.54 - 0.46 * cos((2 * i + 1) * to_fraction);
	}

	// need mirror slightly past center for calculation
	for (int i = 0; i < BLIP_RES; ++i) {
		out[HALF_SIZE + i] = out[HALF_SIZE - 1 - i];
	}

	// starts at 0
	for (int i = 0; i < BLIP_RES; ++i) {
		fimpulse[i] = 0.0;
	}

	// find rescale factor
	double total = 0.0;
	for (int i = 0; i < HALF_SIZE; ++i) {
		total += out[i];
	}
	int kernelUnit = 1 << (BLIP_SAMPLE_BITS - 16);
	double rescale = kernelUnit / (2.0 * total);

	// integrate, first difference, rescale, convert to int
	static const int IMPULSES_SIZE = BLIP_RES * (IMPULSE_WIDTH / 2) + 1;
	static int imp[IMPULSES_SIZE];
	double sum = 0.0;
	double next = 0.0;
	for (int i = 0; i < IMPULSES_SIZE; ++i) {
		imp[i] = int(floor((next - sum) * rescale + 0.5));
		sum += fimpulse[i];
		next += fimpulse[i + BLIP_RES];
	}

	// sum pairs for each phase and add error correction to end of first half
	for (int p = BLIP_RES; p-- >= BLIP_RES / 2; /* */) {
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

	// reshuffle to a more cache friendly order
	for (int phase = 0; phase < BLIP_RES; ++phase) {
		const int* imp_fwd = &imp[BLIP_RES - phase];
		const int* imp_rev = &imp[phase];
		for (int i = 0; i < IMPULSE_WIDTH / 2; ++i) {
			impulses[phase][     i] = imp_fwd[BLIP_RES * i];
			impulses[phase][15 - i] = imp_rev[BLIP_RES * i];
		}
	}
}

BlipBuffer::BlipBuffer()
{
	offset = 0;
	accum = 0;
	availSamp = 0;
	memset(buffer, 0, sizeof(buffer));
	initImpulse();
}

void BlipBuffer::addDelta(TimeIndex time, int delta)
{
	unsigned tmp = time.toInt() + IMPULSE_WIDTH;
	assert(tmp < BUFFER_SIZE);
	availSamp = std::max<int>(availSamp, tmp);

	unsigned phase = time.fractAsInt();
	unsigned ofst = time.toInt() + offset;
	if (likely((ofst + IMPULSE_WIDTH) <= BUFFER_SIZE)) {
		for (int i = 0; i < IMPULSE_WIDTH; ++i) {
			buffer[ofst + i] += impulses[phase][i] * delta;
		}
	} else {
		for (int i = 0; i < IMPULSE_WIDTH; ++i) {
			buffer[(ofst + i) & BUFFER_MASK] += impulses[phase][i] * delta;
		}
	}
}

// returns -1, 0, 1 for x negative, zero, positive
static inline int sign(int x)
{
	return (x != 0) | (x >> 31);
}

bool BlipBuffer::readSamples(
	int* __restrict out, unsigned samples, unsigned pitch) __restrict
{
	static const int SAMPLE_SHIFT = BLIP_SAMPLE_BITS - 16;
	static const int BASS_SHIFT = 9;

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
			out[i * pitch] = acc >> SAMPLE_SHIFT;
			acc -= acc / (1 << BASS_SHIFT);
			acc -= sign(acc); // make sure acc eventually goes to zero
		}
		accum = acc;
	} else {
		availSamp -= samples;
		int acc = accum;
		for (unsigned i = 0; i < samples; ++i) {
			out[i * pitch] = acc >> SAMPLE_SHIFT;
			acc -= acc / (1 << BASS_SHIFT);
			acc += buffer[offset];
			buffer[offset] = 0;
			offset = (offset + 1) & BUFFER_MASK;
		}
		accum = acc;
	}
	return true;
}

} // namespace openmsx
