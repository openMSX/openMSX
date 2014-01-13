#include "BlipBuffer.hh"
#include "likely.hh"
#include <algorithm>
#include <cstring>
#include <cassert>

namespace openmsx {

// This defines the table
//    static const int impulses[BLIP_RES][BLIP_IMPULSE_WIDTH] = { ... };
#include "BlipTable.ii"


BlipBuffer::BlipBuffer()
{
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
			buffer[ofst + i] += impulses[phase][i] * delta;
		}
	} else {
		for (int i = 0; i < BLIP_IMPULSE_WIDTH; ++i) {
			buffer[(ofst + i) & BUFFER_MASK] += impulses[phase][i] * delta;
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
