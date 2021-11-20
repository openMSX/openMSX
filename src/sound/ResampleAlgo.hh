#ifndef RESAMPLEALGO_HH
#define RESAMPLEALGO_HH

#include "EmuTime.hh"
#include "ResampledSoundDevice.hh"

namespace openmsx {

class DynamicClock;

class ResampleAlgo
{
public:
	virtual ~ResampleAlgo() = default;

	bool generateOutput(float* dataOut, unsigned num, EmuTime::param time)
	{
		bool result = generateOutputImpl(dataOut, num, time);
		auto& emuClk = getEmuClock(); (void)emuClk;
		assert(emuClk.getTime() <= time);
		assert(emuClk.getFastAdd(1) > time);
		return result;
	}

protected:
	ResampleAlgo(ResampledSoundDevice& input_) : input(input_) {}
	[[nodiscard]] DynamicClock& getEmuClock() const { return input.getEmuClock(); }
	virtual bool generateOutputImpl(float* dataOut, unsigned num,
	                                EmuTime::param time) = 0;

protected:
	ResampledSoundDevice& input;
};

} // namespace openmsx

#endif
