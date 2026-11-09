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
	static constexpr int TAB_BITS = 12; // number of (sub-sample) phases = 2^TAB_BITS
	static constexpr size_t TAB_LEN = size_t(1) << TAB_BITS;
	static constexpr size_t HALF_TAB_LEN = TAB_LEN / 2;

public:
	ResampleHQ(ResampledSoundDevice& input, const DynamicClock& hostClock);
	ResampleHQ(const ResampleHQ&) = delete;
	ResampleHQ(ResampleHQ&&) = delete;
	ResampleHQ& operator=(const ResampleHQ&) = delete;
	ResampleHQ& operator=(ResampleHQ&&) = delete;
	~ResampleHQ() override;

	bool generateOutputImpl(float* dataOut, size_t num,
	                        EmuTime time) override;

private:
	void calcOutput(unsigned intPos, uint32_t fracPos, float* output);
	void prepareData(unsigned emuNum);

private:
	const DynamicClock& hostClock;
	const float ratio;
	const uint64_t step; // 'ratio' as 32.32 fixed point
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
