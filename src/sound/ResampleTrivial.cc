// $Id:$

#include "ResampleTrivial.hh"
#include "Resample.hh"

namespace openmsx {

ResampleTrivial::ResampleTrivial(Resample& input_)
	: input(input_)
{
}

bool ResampleTrivial::generateOutput(int* dataOut, unsigned num)
{
	return input.generateInput(dataOut, num);
}

} // namespace openmsx
