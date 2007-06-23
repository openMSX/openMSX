// $Id:$

#include "BlipBuffer.hh"
#include <algorithm>
#include <cstring>
#include <cassert>
#include <cmath>

namespace openmsx {

static const int BLIP_SAMPLE_BITS = 30;
static const int BLIP_RES = 1 << BlipBuffer::BLIP_PHASE_BITS;
static const int IMPULSE_WIDTH = 16;
static const int IMPULSES_SIZE = BLIP_RES * (IMPULSE_WIDTH / 2) + 1;

static int impulses[IMPULSES_SIZE];

static void initImpulse()
{
	static bool alreadyInit = false;
	if (alreadyInit) return;
	alreadyInit = true;

	double fimpulse[BLIP_RES / 2 * (IMPULSE_WIDTH - 1) + BLIP_RES * 2];

	// generate sinc
	int halfSize = BLIP_RES / 2 * (IMPULSE_WIDTH - 1);
	double oversample = 1.15;
	double to_angle = M_PI / (2.0 * oversample * BLIP_RES);
	for (int i = 0; i < halfSize; ++i) {
		double angle = ((i - halfSize) * 2 + 1) * to_angle;
		fimpulse[BLIP_RES + i] = sin(angle) / angle;
	}

	// apply (half of) hamming window
	double to_fraction = M_PI / (halfSize - 1);
	for (int i = halfSize; i--; /* */) {
		fimpulse[BLIP_RES + i] *= 0.54 - 0.46 * cos(i * to_fraction);
	}

	// need mirror slightly past center for calculation
	for (int i = BLIP_RES; i--; /* */) {
		fimpulse[BLIP_RES + halfSize + i] =
			fimpulse[BLIP_RES + halfSize - 1 - i];
	}

	// starts at 0
	for (int i = 0; i < BLIP_RES; ++i) {
		fimpulse[i] = 0.0;
	}

	// find rescale factor
	double total = 0.0;
	for (int i = 0; i < halfSize; ++i) {
		total += fimpulse[BLIP_RES + i];
	}
	int kernelUnit = 1 << (BLIP_SAMPLE_BITS - 16);
	double rescale = kernelUnit / (2.0 * total);

	// integrate, first difference, rescale, convert to int
	double sum = 0.0;
	double next = 0.0;
	for (int i = 0; i < IMPULSES_SIZE; ++i) {
		impulses[i] = (int)floor((next - sum) * rescale + 0.5);
		sum += fimpulse[i];
		next += fimpulse[i + BLIP_RES];
	}

	// sum pairs for each phase and add error correction to end of first half
	for (int p = BLIP_RES; p-- >= BLIP_RES / 2; /* */) {
		int p2 = BLIP_RES - 2 - p;
		int error = kernelUnit;
		for (int i = 1; i < IMPULSES_SIZE; i += BLIP_RES) {
			error -= impulses[i + p ];
			error -= impulses[i + p2];
		}
		if (p == p2) {
			// phase = 0.5 impulse uses same half for both sides
			error /= 2;
		}
		impulses[IMPULSES_SIZE - BLIP_RES + p] += error;
	}
}

BlipBuffer::BlipBuffer()
{
	offset = 0;
	accum = 0;
	lastAmp = 0;
	availSamp = 0;
	memset(buffer, 0, sizeof(buffer));
	initImpulse();
}

void BlipBuffer::update(TimeIndex time, int amp)
{
	unsigned tmp = time.toInt() + IMPULSE_WIDTH;
	availSamp = std::max<int>(availSamp, tmp);

	assert(tmp < BUFFER_SIZE);
	int delta = amp - lastAmp;
	lastAmp = amp;

	unsigned phase = time.fractAsInt();
	const int* imp_fwd = &impulses[BLIP_RES - phase];
	const int* imp_rev = &impulses[phase];
	unsigned ofst = time.toInt() + offset;

	buffer[(ofst +  0) & BUFFER_MASK] += imp_fwd[BLIP_RES * 0] * delta;
	buffer[(ofst +  1) & BUFFER_MASK] += imp_fwd[BLIP_RES * 1] * delta;
	buffer[(ofst +  2) & BUFFER_MASK] += imp_fwd[BLIP_RES * 2] * delta;
	buffer[(ofst +  3) & BUFFER_MASK] += imp_fwd[BLIP_RES * 3] * delta;
	buffer[(ofst +  4) & BUFFER_MASK] += imp_fwd[BLIP_RES * 4] * delta;
	buffer[(ofst +  5) & BUFFER_MASK] += imp_fwd[BLIP_RES * 5] * delta;
	buffer[(ofst +  6) & BUFFER_MASK] += imp_fwd[BLIP_RES * 6] * delta;
	buffer[(ofst +  7) & BUFFER_MASK] += imp_fwd[BLIP_RES * 7] * delta;

	buffer[(ofst +  8) & BUFFER_MASK] += imp_rev[BLIP_RES * 7] * delta;
	buffer[(ofst +  9) & BUFFER_MASK] += imp_rev[BLIP_RES * 6] * delta;
	buffer[(ofst + 10) & BUFFER_MASK] += imp_rev[BLIP_RES * 5] * delta;
	buffer[(ofst + 11) & BUFFER_MASK] += imp_rev[BLIP_RES * 4] * delta;
	buffer[(ofst + 12) & BUFFER_MASK] += imp_rev[BLIP_RES * 3] * delta;
	buffer[(ofst + 13) & BUFFER_MASK] += imp_rev[BLIP_RES * 2] * delta;
	buffer[(ofst + 14) & BUFFER_MASK] += imp_rev[BLIP_RES * 1] * delta;
	buffer[(ofst + 15) & BUFFER_MASK] += imp_rev[BLIP_RES * 0] * delta;
}

bool BlipBuffer::readSamples(int* out, unsigned samples, unsigned pitch)
{
	static const int SAMPLE_SHIFT = BLIP_SAMPLE_BITS - 16;
	static const int BASS_SHIFT = 9;

	if ((availSamp <= 0) && (accum < (1 << BASS_SHIFT))) {
		return false; // muted
	}
	availSamp -= samples;

	int acc = accum;
	for (unsigned i = 0; i < samples; ++i) {
		out[i * pitch] = acc >> SAMPLE_SHIFT;
		acc -= acc >> BASS_SHIFT;
		acc += buffer[offset];
		buffer[offset] = 0;
		offset = (offset + 1) & BUFFER_MASK;
	}
	accum = acc;
	return true;
}

} // namespace openmsx
