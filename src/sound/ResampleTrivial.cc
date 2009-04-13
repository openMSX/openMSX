// $Id$

#include "ResampleTrivial.hh"
#include "Resample.hh"
#include <cassert>

namespace openmsx {

ResampleTrivial::ResampleTrivial(Resample& input_)
	: input(input_)
{
}

bool ResampleTrivial::generateOutput(int* dataOut, unsigned num)
{
	assert((long(dataOut) & 15) == 0); // must be 16-byte aligned
	return input.generateInput(dataOut, num);
}

} // namespace openmsx
