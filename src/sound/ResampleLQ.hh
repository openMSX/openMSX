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
	typedef FixedPoint<14> FP;
	const FP step;
	int lastInput[2 * CHANNELS];
};

template <unsigned CHANNELS>
class ResampleLQDown : public ResampleLQ<CHANNELS>
{
public:
	ResampleLQDown(ResampledSoundDevice& input,
	               const DynamicClock& hostClock, unsigned emuSampleRate);
private:
	virtual bool generateOutput(int* dataOut, unsigned num,
	                            EmuTime::param time);
	typedef typename ResampleLQ<CHANNELS>::FP FP;
};

template <unsigned CHANNELS>
class ResampleLQUp : public ResampleLQ<CHANNELS>
{
public:
	ResampleLQUp(ResampledSoundDevice& input,
	             const DynamicClock& hostClock, unsigned emuSampleRate);
private:
	virtual bool generateOutput(int* dataOut, unsigned num,
	                            EmuTime::param time);
	typedef typename ResampleLQ<CHANNELS>::FP FP;
};

} // namespace openmsx

#endif
