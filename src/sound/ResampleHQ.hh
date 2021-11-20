#ifndef RESAMPLEHQ_HH
#define RESAMPLEHQ_HH

#include "ResampleAlgo.hh"
#include <cstdint>
#include <vector>

namespace openmsx {

class DynamicClock;
class ResampledSoundDevice;

template<unsigned CHANNELS>
class ResampleHQ final : public ResampleAlgo
{
public:
	ResampleHQ(ResampledSoundDevice& input, const DynamicClock& hostClock);
	~ResampleHQ() override;
	ResampleHQ(const ResampleHQ&) = delete;
	ResampleHQ& operator=(const ResampleHQ&) = delete;

	bool generateOutputImpl(float* dataOut, unsigned num,
	                        EmuTime::param time) override;

private:
	void calcOutput(float pos, float* output);
	void prepareData(unsigned emuNum);

private:
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
