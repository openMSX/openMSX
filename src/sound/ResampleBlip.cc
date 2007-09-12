// $Id:$

#include "ResampleBlip.hh"
#include "Resample.hh"
#include <cassert>

namespace openmsx {

template <unsigned CHANNELS>
ResampleBlip<CHANNELS>::ResampleBlip(Resample& input_, double ratio_)
	: input(input_)
	, ratio(ratio_)
	, invRatio(1.0 / ratio_)
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
	int buf[required * CHANNELS + 3];
	if (input.generateInput(buf, required)) {
		double pos = lastPos * invRatio;
		for (int i = 0; i < required; ++i) {
			for (unsigned ch = 0; ch < CHANNELS; ++ch) {
				if (buf[CHANNELS * i + ch] != lastInput[ch]) {
					lastInput[ch] = buf[CHANNELS * i + ch];
					blip[ch].update(BlipBuffer::TimeIndex(pos),
					                lastInput[ch]);
				}
			}
			pos += invRatio;
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
	lastPos -= len;
	assert(lastPos >= 0.0);

	bool result = false;
	for (unsigned ch = 0; ch < CHANNELS; ++ch) {
		result |= blip[ch].readSamples(dataOut + ch, num, CHANNELS);
	}
	return result;
}

// Force template instantiation.
template class ResampleBlip<1>;
template class ResampleBlip<2>;

} // namespace openmsx
