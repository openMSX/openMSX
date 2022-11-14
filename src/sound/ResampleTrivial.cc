#include "ResampleTrivial.hh"
#include "ResampledSoundDevice.hh"
#include <cassert>

namespace openmsx {

ResampleTrivial::ResampleTrivial(ResampledSoundDevice& input_)
	: ResampleAlgo(input_)
{
}

bool ResampleTrivial::generateOutputImpl(float* dataOut, size_t num,
                                         EmuTime::param /*time*/)
{
#ifdef __SSE2__
	assert((uintptr_t(dataOut) & 15) == 0); // must be 16-byte aligned
#endif
	getEmuClock() += num;
	return input.generateInput(dataOut, num);
}

} // namespace openmsx
