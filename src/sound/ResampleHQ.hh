// $Id: ResampleHQ.hh 6481 2007-05-17 11:13:22Z m9710797 $

#ifndef RESAMPLEHQ_HH
#define RESAMPLEHQ_HH

#include "ResampleAlgo.hh"
#include "FixedPoint.hh"

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
	typedef FixedPoint<16> FilterIndex;
	static const unsigned BUF_LEN = 16384;

	void calcOutput(FilterIndex startFilterIndex, int* output);
	void calcOutput2(float lastPos, int* output);
	void prepareData(unsigned extra);

        Resample& input;
	float ratio;
	float floatIncr;
	float normFactor;
	float lastPos;
	FilterIndex increment;
	unsigned halfFilterLen;
	unsigned bufCurrent;
	unsigned bufEnd;
	unsigned nonzeroSamples;
	unsigned filterLen;
	float buffer[BUF_LEN * CHANNELS];
	float* table;
};

} // namespace openmsx

#endif
