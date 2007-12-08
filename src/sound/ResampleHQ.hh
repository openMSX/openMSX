// $Id: ResampleHQ.hh 6481 2007-05-17 11:13:22Z m9710797 $

#ifndef RESAMPLEHQ_HH
#define RESAMPLEHQ_HH

#include "ResampleAlgo.hh"

namespace openmsx {

class Resample;

template <unsigned CHANNELS>
class ResampleHQ : public ResampleAlgo
{
public:
	ResampleHQ(Resample& input, double ratio);
	virtual ~ResampleHQ();

	virtual bool generateOutput(int* dataOut, unsigned num);

private:
	static const unsigned BUF_LEN = 16384;

	void calcOutput(float lastPos, int* output);
	void prepareData(unsigned request);

	Resample& input;
	float ratio;
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
