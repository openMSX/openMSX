// $Id$

#ifndef RESAMPLEHQ_HH
#define RESAMPLEHQ_HH

#include "ResampleAlgo.hh"
#include "DynamicClock.hh"

namespace openmsx {

class ResampledSoundDevice;

template <unsigned CHANNELS>
class ResampleHQ : public ResampleAlgo
{
public:
	ResampleHQ(ResampledSoundDevice& input,
	           const DynamicClock& hostClock, unsigned emuSampleRate);
	virtual ~ResampleHQ();

	virtual bool generateOutput(int* dataOut, unsigned num,
	                            EmuTime::param time);

private:
	static const unsigned BUF_LEN = 16384;

	void calcOutput(float pos, int* output);
	void prepareData(unsigned emuNum);

	ResampledSoundDevice& input;
	const DynamicClock& hostClock;
	DynamicClock emuClock;

	const float ratio;
	unsigned bufStart;
	unsigned bufEnd;
	unsigned nonzeroSamples;
	unsigned filterLen;
	float buffer[BUF_LEN * CHANNELS];
	float* table;
};

} // namespace openmsx

#endif
