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

	ResampledSoundDevice& input;
	const DynamicClock& hostClock;
	DynamicClock emuClock;
	using FP = FixedPoint<14>;
	const FP step;
	int lastInput[2 * CHANNELS];
};

template <unsigned CHANNELS>
class ResampleLQDown final : public ResampleLQ<CHANNELS>
{
public:
	ResampleLQDown(ResampledSoundDevice& input,
	               const DynamicClock& hostClock, unsigned emuSampleRate);
private:
	bool generateOutput(int* dataOut, unsigned num,
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
	bool generateOutput(int* dataOut, unsigned num,
	                    EmuTime::param time) override;
	using FP = typename ResampleLQ<CHANNELS>::FP;
};

} // namespace openmsx

#endif
