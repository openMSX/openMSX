// $Id: ResampleLQ.hh 6476 2007-05-14 18:12:29Z m9710797 $

#ifndef RESAMPLELQ_HH
#define RESAMPLELQ_HH

#include "ResampleAlgo.hh"
#include "FixedPoint.hh"

namespace openmsx {

class Resample;

template <unsigned CHANNELS>
class ResampleLQ : public ResampleAlgo
{
public:
	ResampleLQ(Resample& input, double ratio);
	virtual bool generateOutput(int* dataOut, unsigned num);

private:
	Resample& input;
	typedef FixedPoint<16> Pos;
	Pos pos;
	const Pos step;
	int lastInput[CHANNELS];
};

} // namespace openmsx

#endif
