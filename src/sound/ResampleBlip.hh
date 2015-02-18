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

	bool generateOutput(int* dataOut, unsigned num,
	                    EmuTime::param time) override;

private:
	BlipBuffer blip[CHANNELS];
	ResampledSoundDevice& input;
	const DynamicClock& hostClock; // time of the last host-sample,
	                               //    ticks once per host sample
	DynamicClock emuClock;         // time of the last emu-sample,
	                               //    ticks once per emu-sample
	using FP = FixedPoint<16>;
	const FP step;
	int lastInput[CHANNELS];
};

} // namespace openmsx

#endif
