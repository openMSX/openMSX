// $Id: ResampleLQ.cc 6476 2007-05-14 18:12:29Z m9710797 $

#include "ResampleLQ.hh"
#include "Resample.hh"
#include <cassert>
#include <cstring>

namespace openmsx {

static const int BUFSIZE = 16384;
static int bufferInt[BUFSIZE + 4] __attribute__((aligned(16)));

template <unsigned CHANNELS>
ResampleLQ<CHANNELS>::ResampleLQ(Resample& input_, double ratio)
	: input(input_), pos(0), step(ratio)
{
	assert(step >= Pos(1)); // can only do downsampling
	for (unsigned j = 0; j < CHANNELS; ++j) {
		lastInput[j] = 0;
	}
}

template <unsigned CHANNELS>
bool ResampleLQ<CHANNELS>::generateOutput(int* dataOut, unsigned num)
{
	Pos end = pos + step * num;
	int numInput = end.toInt();
	assert((numInput + 1) <= BUFSIZE);
	int* buffer = &bufferInt[4 - CHANNELS];
	assert((long(&buffer[1 * CHANNELS]) & 15) == 0);

	if (!input.generateInput(&buffer[1 * CHANNELS], numInput)) {
		int last = 0;
		for (unsigned j = 0; j < CHANNELS; ++j) {
			last |= lastInput[j];
		}
		if (last == 0) {
			return false;
		}
		memset(&buffer[CHANNELS], 0, numInput * CHANNELS * sizeof(int));
	}
	for (unsigned j = 0; j < CHANNELS; ++j) {
		buffer[j] = lastInput[j];
		lastInput[j] = buffer[numInput * CHANNELS + j];
	}
	for (unsigned i = 0; i < num; ++i) {
		int p = pos.toInt();
		Pos fract = pos.fract();
		for (unsigned j = 0; j < CHANNELS; ++j) {
			int s0 = buffer[(p + 0) * CHANNELS + j];
			int s1 = buffer[(p + 1) * CHANNELS + j];
			int out = s0 + (fract * (s1 - s0)).toInt();
			dataOut[i * CHANNELS + j] = out;
		}
		pos += step;
	}
	pos = pos.fract();
	return true;
}

// Force template instantiation.
template class ResampleLQ<1>;
template class ResampleLQ<2>;

} // namespace openmsx
