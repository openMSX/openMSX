// $Id$

#ifndef RESAMPLEHQ_HH
#define RESAMPLEHQ_HH

#include "ResampleAlgo.hh"

namespace openmsx {

class ResampledSoundDevice;

template <unsigned CHANNELS>
class ResampleHQ : public ResampleAlgo
{
public:
	ResampleHQ(ResampledSoundDevice& input, double ratio);
	virtual ~ResampleHQ();

	virtual bool generateOutput(int* dataOut, unsigned num);

private:
	static const unsigned BUF_LEN = 16384;

	void calcOutput(float lastPos, int* output);
	void prepareData(unsigned request);

	ResampledSoundDevice& input;
	const float ratio;
	float lastPos;
	unsigned bufStart;
	unsigned bufEnd;
	unsigned nonzeroSamples;
	unsigned filterLen;
	float buffer[BUF_LEN * CHANNELS];
	float* table;
};

} // namespace openmsx

#endif
