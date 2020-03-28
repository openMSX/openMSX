#ifndef RESAMPLEHQ_HH
#define RESAMPLEHQ_HH

#include "ResampleAlgo.hh"
#include "DynamicClock.hh"
#include <cstdint>
#include <vector>

namespace openmsx {

class ResampledSoundDevice;

template <unsigned CHANNELS>
class ResampleHQ final : public ResampleAlgo
{
public:
	ResampleHQ(ResampledSoundDevice& input,
	           const DynamicClock& hostClock, unsigned emuSampleRate);
	~ResampleHQ() override;

	bool generateOutputImpl(float* dataOut, unsigned num,
	                        EmuTime::param time) override;

private:
	void calcOutput(float pos, float* output);
	void prepareData(unsigned emuNum);

	const DynamicClock& hostClock;

	const float ratio;
	unsigned bufStart;
	unsigned bufEnd;
	unsigned nonzeroSamples;
	unsigned filterLen;
	std::vector<float> buffer;
	float* table;
	int16_t* permute;
};

} // namespace openmsx

#endif
