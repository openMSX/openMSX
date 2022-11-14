#ifndef RESAMPLEHQ_HH
#define RESAMPLEHQ_HH

#include "ResampleAlgo.hh"
#include <cstdint>
#include <span>
#include <vector>

namespace openmsx {

class DynamicClock;
class ResampledSoundDevice;

template<unsigned CHANNELS>
class ResampleHQ final : public ResampleAlgo
{
public:
	static constexpr size_t TAB_LEN = 4096;
	static constexpr size_t HALF_TAB_LEN = TAB_LEN / 2;

public:
	ResampleHQ(ResampledSoundDevice& input, const DynamicClock& hostClock);
	~ResampleHQ() override;
	ResampleHQ(const ResampleHQ&) = delete;
	ResampleHQ& operator=(const ResampleHQ&) = delete;

	bool generateOutputImpl(float* dataOut, size_t num,
	                        EmuTime::param time) override;

private:
	void calcOutput(float pos, float* output);
	void prepareData(unsigned emuNum);

private:
	const DynamicClock& hostClock;
	const float ratio;
	unsigned bufStart;
	unsigned bufEnd;
	unsigned nonzeroSamples = 0;
	unsigned filterLen;
	std::vector<float> buffer;
	float* table;
	std::span<const int16_t, HALF_TAB_LEN> permute;
};

} // namespace openmsx

#endif
