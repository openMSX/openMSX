#ifndef RESAMPLEBLIP_HH
#define RESAMPLEBLIP_HH

#include "ResampleAlgo.hh"
#include "BlipBuffer.hh"
#include <array>

namespace openmsx {

class DynamicClock;
class ResampledSoundDevice;

template<unsigned CHANNELS>
class ResampleBlip final : public ResampleAlgo
{
public:
	ResampleBlip(ResampledSoundDevice& input, const DynamicClock& hostClock);

	bool generateOutputImpl(float* dataOut, size_t num,
	                        EmuTime time) override;

private:
	std::array<BlipBuffer, CHANNELS> blip;
	const DynamicClock& hostClock; // time of the last host-sample,
	                               //    ticks once per host sample
	using FP = FixedPoint<16>;
	const FP step;
	std::array<float, CHANNELS> lastInput;
};

} // namespace openmsx

#endif
