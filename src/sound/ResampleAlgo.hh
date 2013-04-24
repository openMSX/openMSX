#ifndef RESAMPLEALGO_HH
#define RESAMPLEALGO_HH

#include "EmuTime.hh"

namespace openmsx {

class ResampleAlgo
{
public:
	virtual ~ResampleAlgo() {}
	virtual bool generateOutput(int* dataOut, unsigned num,
	                            EmuTime::param time) = 0;
};

} // namespace openmsx

#endif
