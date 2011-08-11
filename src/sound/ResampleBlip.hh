// $Id$

#ifndef RESAMPLEBLIP_HH
#define RESAMPLEBLIP_HH

#include "ResampleAlgo.hh"
#include "BlipBuffer.hh"

namespace openmsx {

class ResampledSoundDevice;

template <unsigned CHANNELS>
class ResampleBlip : public ResampleAlgo
{
public:
	ResampleBlip(ResampledSoundDevice& input, double ratio);

	virtual bool generateOutput(int* dataOut, unsigned num);

private:
	BlipBuffer blip[CHANNELS];
	ResampledSoundDevice& input;
	const double ratio;
	const double invRatio;
	double lastPos;
	typedef FixedPoint<16> FP;
	const FP invRatioFP;
	int lastInput[CHANNELS];
};

} // namespace openmsx

#endif
