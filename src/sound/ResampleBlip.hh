// $Id:$

#ifndef RESAMPLEBLIP_HH
#define RESAMPLEBLIP_HH

#include "ResampleAlgo.hh"
#include "BlipBuffer.hh"

namespace openmsx {

class Resample;

template <unsigned CHANNELS>
class ResampleBlip : public ResampleAlgo
{
public:
	ResampleBlip(Resample& input, double ratio);

	virtual bool generateOutput(int* dataOut, unsigned num);

private:
	BlipBuffer blip[CHANNELS];
	Resample& input;
	const double ratio;
	const double invRatio;
	typedef FixedPoint<16> FP;
	FP invRatioFP;
	double lastPos;
	int lastInput[CHANNELS];
};

} // namespace openmsx

#endif
