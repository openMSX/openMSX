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
	getEmuClock() += num;
	return input.generateInput(dataOut, num);
}

} // namespace openmsx
