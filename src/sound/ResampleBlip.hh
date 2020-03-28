#ifndef RESAMPLEBLIP_HH
#define RESAMPLEBLIP_HH

#include "ResampleAlgo.hh"
#include "BlipBuffer.hh"
#include "DynamicClock.hh"

namespace openmsx {

class ResampledSoundDevice;

template <unsigned CHANNELS>
class ResampleBlip final : public ResampleAlgo
{
public:
	ResampleBlip(ResampledSoundDevice& input,
	             const DynamicClock& hostClock, unsigned emuSampleRate);

	bool generateOutputImpl(float* dataOut, unsigned num,
	                        EmuTime::param time) override;

private:
	BlipBuffer blip[CHANNELS];
	const DynamicClock& hostClock; // time of the last host-sample,
	                               //    ticks once per host sample
	using FP = FixedPoint<16>;
	const FP step;
	float lastInput[CHANNELS];
};

} // namespace openmsx

#endif
