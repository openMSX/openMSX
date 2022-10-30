// Heavily based on:
//
// Band-limited sound synthesis and buffering
// Blip_Buffer 0.4.0
// http://www.slack.net/~ant/

#ifndef BLIPBUFFER_HH
#define BLIPBUFFER_HH

#include "FixedPoint.hh"
#include <array>

namespace openmsx {

class BlipBuffer
{
public:
	// Number of bits in phase offset. Fewer than 6 bits (64 phase offsets) results
	// in noticeable broadband noise when synthesizing high frequency square waves.
	static constexpr int BLIP_PHASE_BITS = 10;

	using TimeIndex = FixedPoint<BLIP_PHASE_BITS>;

	BlipBuffer();

	// Update amplitude of waveform at given time. Time is in output sample
	// units and since the last time readSamples() was called.
	void addDelta(TimeIndex time, float delta);

	// Read the given amount of samples into destination buffer.
	template<size_t PITCH>
	bool readSamples(float* out, size_t samples);

private:
	template<size_t PITCH>
	void readSamplesHelper(float* out, size_t samples);

private:
	static constexpr size_t BUFFER_SIZE = 1 << 14;
	static constexpr size_t BUFFER_MASK = BUFFER_SIZE - 1;
	std::array<float, BUFFER_SIZE> buffer;
	size_t offset = 0;
	ptrdiff_t availSamp = 0;
	float accum = 0.0f;
};

} // namespace openmsx

#endif
