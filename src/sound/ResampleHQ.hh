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
	// The coefficient table has 2^TAB_BITS rows (sub-sample phases), of
	// which only the first half is stored (the 2nd half is the mirror
	// image). The output is linearly interpolated between the two rows
	// that enclose the exact phase. 128 rows is enough to keep the
	// interpolation error below -99dBFS for all our sample rates (so
	// better than 16-bit).
	static constexpr int TAB_BITS = 7;
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
};

} // namespace openmsx

#endif
