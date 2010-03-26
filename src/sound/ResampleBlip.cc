// $Id$

#include "ResampleBlip.hh"
#include "Resample.hh"
#include "static_assert.hh"
#include "likely.hh"
#include "vla.hh"
#include "build-info.hh"
#include <algorithm>
#include <cassert>

namespace openmsx {

template <unsigned CHANNELS>
ResampleBlip<CHANNELS>::ResampleBlip(Resample& input_, double ratio_)
	: input(input_)
	, ratio(ratio_)
	, invRatio(1.0 / ratio_)
	, invRatioFP(invRatio)
{
	lastPos = 0.0;
	for (unsigned ch = 0; ch < CHANNELS; ++ch) {
		lastInput[ch] = 0;
	}
}

template <unsigned CHANNELS>
bool ResampleBlip<CHANNELS>::generateOutput(int* dataOut, unsigned num)
{
	double len = num * ratio;
	int required = int(ceil(len - lastPos));
	if (required > 0) {
		// 3 extra for padding, CHANNELS extra for sentinel
#if ASM_X86
		VLA_ALIGNED(int, buf, required * CHANNELS + std::max(3u, CHANNELS), 16);
#else
		VLA(int, buf, required * CHANNELS + std::max(3u, CHANNELS));
#endif
		if (input.generateInput(buf, required)) {
			for (unsigned ch = 0; ch < CHANNELS; ++ch) {
				// In case of PSG (and to a lesser degree SCC) it happens
				// very often that two consecutive samples have the same
				// value. We can benefit from this by setting a sentinel
				// at the end of the buffer and move the end-of-loop test
				// into the 'samples differ' branch.
				assert(required > 0);
				buf[CHANNELS * required + ch] =
					buf[CHANNELS * (required - 1) + ch] + 1;
				FP pos(lastPos * invRatio);
				int last = lastInput[ch]; // local var is slightly faster
				int i = 0;
				while (true) {
					if (unlikely(buf[CHANNELS * i + ch] != last)) {
						if (i == required) {
							break;
						}
						last = buf[CHANNELS * i + ch];
						blip[ch].update(BlipBuffer::TimeIndex(pos),
						                last);
					}
					pos += invRatioFP;
					++i;
				}
				lastInput[ch] = last;
			}
		} else {
			// input all zero
			for (unsigned ch = 0; ch < CHANNELS; ++ch) {
				if (lastInput[ch] != 0) {
					lastInput[ch] = 0;
					double pos = lastPos * invRatio;
					blip[ch].update(BlipBuffer::TimeIndex(pos), 0);
				}
			}
		}
		lastPos += required;
	}
	lastPos -= len;
	assert(lastPos >= 0.0);

	bool results[CHANNELS];
	for (unsigned ch = 0; ch < CHANNELS; ++ch) {
		results[ch] = blip[ch].readSamples(dataOut + ch, num, CHANNELS);
	}
	STATIC_ASSERT((CHANNELS == 1) || (CHANNELS == 2));
	bool result;
	if (CHANNELS == 1) {
		result = results[0];
	} else {
		if (results[0] == results[1]) {
			// Both muted or both unmuted
			result = results[0];
		} else {
			// One channel muted, the other not.
			// We have to set the muted channel to all-zero.
			unsigned offset = results[0] ? 1 : 0;
			for (unsigned i = 0; i < num; ++i) {
				dataOut[2 * i + offset] = 0;
			}
			result = true;
		}
	}
	return result;
}

// Force template instantiation.
template class ResampleBlip<1>;
template class ResampleBlip<2>;

} // namespace openmsx
