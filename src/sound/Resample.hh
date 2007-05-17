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
	bool generateOutput(int* dataOut, unsigned num);
	virtual bool generateInput(int* buffer, unsigned num) = 0;

private:
	typedef FixedPoint<16> FilterIndex;
	static const unsigned BUF_LEN = 16384;

	void calcOutput(FilterIndex startFilterIndex, int* output);
	void calcOutput2(float lastPos, int* output);
	void prepareData(unsigned extra);

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
