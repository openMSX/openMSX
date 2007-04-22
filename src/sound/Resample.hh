// $Id$

#ifndef RESAMPLE_HH
#define RESAMPLE_HH

namespace openmsx {

template <unsigned CHANNELS>
class Resample
{
protected:
	Resample();
	virtual ~Resample();

	void setResampleRatio(double inFreq, double outFreq);
	void generateOutput(float* dataOut, unsigned num);
	virtual void generateInput(float* buffer, unsigned num) = 0;

private:
	static const unsigned BUF_LEN = 16384;

	void calcOutput(int increment, int startFilterIndex,
	                double normFactor, float* output);
	void prepareData(unsigned halfFilterLen, unsigned extra);

	double ratio;
	double lastPos;
	unsigned bufCurrent;
	unsigned bufEnd;
	float buffer[BUF_LEN * CHANNELS];
};

} // namespace openmsx

#endif
