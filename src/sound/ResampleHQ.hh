#ifndef RESAMPLEHQ_HH
#define RESAMPLEHQ_HH

#include "ResampleAlgo.hh"
#include "DynamicClock.hh"
#include <vector>

namespace openmsx {

class ResampledSoundDevice;

template <unsigned CHANNELS>
class ResampleHQ final : public ResampleAlgo
{
public:
	ResampleHQ(ResampledSoundDevice& input,
	           const DynamicClock& hostClock, unsigned emuSampleRate);
	~ResampleHQ();

	bool generateOutput(int* dataOut, unsigned num,
	                    EmuTime::param time) override;

private:
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
	std::vector<float> buffer;
	float* table;
};

} // namespace openmsx

#endif
