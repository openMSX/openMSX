// $Id:$

#ifndef RESAMPLETRIVIAL_HH
#define RESAMPLETRIVIAL_HH

#include "ResampleAlgo.hh"

namespace openmsx {

class Resample;

class ResampleTrivial : public ResampleAlgo
{
public:
	ResampleTrivial(Resample& input);
	virtual bool generateOutput(int* dataOut, unsigned num);

private:
	Resample& input;
};

} // namespace openmsx

#endif
