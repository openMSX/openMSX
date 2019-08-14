#ifndef RESAMPLEALGO_HH
#define RESAMPLEALGO_HH

#include "EmuTime.hh"

namespace openmsx {

class ResampleAlgo
{
public:
	virtual ~ResampleAlgo() = default;
	virtual bool generateOutput(float* dataOut, unsigned num,
	                            EmuTime::param time) = 0;
};

} // namespace openmsx

#endif
