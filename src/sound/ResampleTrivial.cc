// $Id$

#include "ResampleTrivial.hh"
#include "Resample.hh"
#include "build-info.hh"
#include <cassert>

namespace openmsx {

ResampleTrivial::ResampleTrivial(Resample& input_)
	: input(input_)
{
}

bool ResampleTrivial::generateOutput(int* dataOut, unsigned num)
{
#if ASM_X86
	assert((long(dataOut) & 15) == 0); // must be 16-byte aligned
#endif
	return input.generateInput(dataOut, num);
}

} // namespace openmsx
