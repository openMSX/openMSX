// $Id$

#ifndef RESAMPLE_HH
#define RESAMPLE_HH

#include "FixedPoint.hh"

namespace openmsx {

template <unsigned CHANNELS>
class Resample
{
protected:
	Resample();
	virtual ~Resample();

	void setResampleRatio(double inFreq, double outFreq);
	bool generateOutput(float* dataOut, unsigned num);
	virtual bool generateInput(float* buffer, unsigned num) = 0;

private:
	typedef FixedPoint<16> FilterIndex;
	static const unsigned BUF_LEN = 16384;

	void calcOutput(FilterIndex startFilterIndex, float* output);
	void prepareData(unsigned extra);

	double ratio;
	double floatIncr;
	double normFactor;
	double lastPos;
	FilterIndex increment;
	unsigned halfFilterLen;
	unsigned bufCurrent;
	unsigned bufEnd;
	unsigned nonzeroSamples;
	float buffer[BUF_LEN * CHANNELS];
};

} // namespace openmsx

#endif
