#include "ResampleTrivial.hh"
#include "ResampledSoundDevice.hh"
#include <cassert>

namespace openmsx {

ResampleTrivial::ResampleTrivial(ResampledSoundDevice& input_)
	: input(input_)
{
}

bool ResampleTrivial::generateOutput(int* dataOut, unsigned num,
                                     EmuTime::param /*time*/)
{
#ifdef __SSE2__
	assert((uintptr_t(dataOut) & 15) == 0); // must be 16-byte aligned
#endif
	return input.generateInput(dataOut, num);
}

} // namespace openmsx
