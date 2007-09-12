// $Id: ResampleLQ.cc 6476 2007-05-14 18:12:29Z m9710797 $

#include "ResampleLQ.hh"
#include "Resample.hh"
#include <algorithm>
#include <cassert>
#include <cstring>

namespace openmsx {

template <unsigned CHANNELS>
ResampleLQ<CHANNELS>::ResampleLQ(Resample& input_, double ratio)
	: input(input_), pos(0), step(ratio), bufStart(0), bufEnd(0)
{
}

template <unsigned CHANNELS>
void ResampleLQ<CHANNELS>::fillBuffer(unsigned pos, unsigned num)
{
	if (!input.generateInput(&buffer[pos * CHANNELS], num)) {
		memset(&buffer[pos * CHANNELS], 0, num * CHANNELS * sizeof(int));
	}
}

template <unsigned CHANNELS>
void ResampleLQ<CHANNELS>::prepareData(unsigned missing)
{
	unsigned free = BUF_LEN - bufEnd;
	if (missing < free) {
		// new data fits without wrapping
		fillBuffer(bufEnd, missing); // 3 extra, ok
		bufEnd += missing;
		assert(bufEnd < BUF_LEN);
	} else {
		// partly fill at end, partly at begin
		fillBuffer(bufEnd, free); // 3 extra, ok
		fillBuffer(0, missing - free); // 3 extra, ok
		bufEnd = missing - free;
		assert(bufEnd < bufStart);
	}
}

template <unsigned CHANNELS>
bool ResampleLQ<CHANNELS>::generateOutput(int* dataOut, unsigned num)
{
	bool anyNonZero = false;
	for (unsigned i = 0; i < num; ++i) {
		// interpolate between these two positions
		int p0 = pos.toInt();
		int p1 = p0 + 1;

		// need to reload buffer?
		int available = (bufEnd - bufStart) & BUF_MASK;
		if (available <= p1) {
			int request = std::min<int>((step * (num - i)).toInt() + 2 + 1,
			                            BUF_LEN - 1);
			int missing = request - available;
			assert(missing > 0);
			prepareData(missing);
		}

		// actually interpolate
		int i0 = (p0 + bufStart) & BUF_MASK;
		int i1 = (p1 + bufStart) & BUF_MASK;
		Pos fract = pos.fract();
		for (unsigned j = 0; j < CHANNELS; ++j) {
			int s0 = buffer[i0 * CHANNELS + j];
			int s1 = buffer[i1 * CHANNELS + j];
			int out = s0 + (fract * (s1 - s0)).toInt();
			dataOut[i * CHANNELS + j] = out;
			anyNonZero |= out;
		}

		// figure out the next index
		bufStart = (bufStart + p0) & BUF_MASK;
		pos = fract + step;
	}
	return anyNonZero;
}

// Force template instantiation.
template class ResampleLQ<1>;
template class ResampleLQ<2>;

} // namespace openmsx
