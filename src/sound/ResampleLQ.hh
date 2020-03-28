#ifndef RESAMPLELQ_HH
#define RESAMPLELQ_HH

#include "ResampleAlgo.hh"
#include "DynamicClock.hh"
#include "FixedPoint.hh"
#include <memory>

namespace openmsx {

class ResampledSoundDevice;

template <unsigned CHANNELS>
class ResampleLQ : public ResampleAlgo
{
public:
	static std::unique_ptr<ResampleLQ<CHANNELS>> create(
		ResampledSoundDevice& input,
		const DynamicClock& hostClock, unsigned emuSampleRate);

protected:
	ResampleLQ(ResampledSoundDevice& input,
	           const DynamicClock& hostClock, unsigned emuSampleRate);
	bool fetchData(EmuTime::param time, unsigned& valid);

	const DynamicClock& hostClock;
	using FP = FixedPoint<14>;
	const FP step;
	float lastInput[2 * CHANNELS];
};

template <unsigned CHANNELS>
class ResampleLQDown final : public ResampleLQ<CHANNELS>
{
public:
	ResampleLQDown(ResampledSoundDevice& input,
	               const DynamicClock& hostClock, unsigned emuSampleRate);
private:
	bool generateOutputImpl(float* dataOut, unsigned num,
	                        EmuTime::param time) override;
	using FP = typename ResampleLQ<CHANNELS>::FP;
};

template <unsigned CHANNELS>
class ResampleLQUp final : public ResampleLQ<CHANNELS>
{
public:
	ResampleLQUp(ResampledSoundDevice& input,
	             const DynamicClock& hostClock, unsigned emuSampleRate);
private:
	bool generateOutputImpl(float* dataOut, unsigned num,
	                        EmuTime::param time) override;
	using FP = typename ResampleLQ<CHANNELS>::FP;
};

} // namespace openmsx

#endif
