#ifndef RESAMPLELQ_HH
#define RESAMPLELQ_HH

#include "ResampleAlgo.hh"
#include "FixedPoint.hh"
#include <array>
#include <memory>

namespace openmsx {

class DynamicClock;
class ResampledSoundDevice;

template<unsigned CHANNELS>
class ResampleLQ : public ResampleAlgo
{
public:
	static std::unique_ptr<ResampleLQ<CHANNELS>> create(
		ResampledSoundDevice& input, const DynamicClock& hostClock);

protected:
	ResampleLQ(ResampledSoundDevice& input, const DynamicClock& hostClock);
	[[nodiscard]] bool fetchData(EmuTime::param time, unsigned& valid);

protected:
	const DynamicClock& hostClock;
	using FP = FixedPoint<14>;
	const FP step;
	std::array<float, 2 * CHANNELS> lastInput;
};

template<unsigned CHANNELS>
class ResampleLQDown final : public ResampleLQ<CHANNELS>
{
public:
	ResampleLQDown(ResampledSoundDevice& input, const DynamicClock& hostClock);
private:
	bool generateOutputImpl(float* dataOut, size_t num,
	                        EmuTime::param time) override;
	using FP = typename ResampleLQ<CHANNELS>::FP;
};

template<unsigned CHANNELS>
class ResampleLQUp final : public ResampleLQ<CHANNELS>
{
public:
	ResampleLQUp(ResampledSoundDevice& input, const DynamicClock& hostClock);
private:
	bool generateOutputImpl(float* dataOut, size_t num,
	                        EmuTime::param time) override;
	using FP = typename ResampleLQ<CHANNELS>::FP;
};

} // namespace openmsx

#endif
