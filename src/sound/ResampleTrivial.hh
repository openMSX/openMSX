#ifndef RESAMPLETRIVIAL_HH
#define RESAMPLETRIVIAL_HH

#include "ResampleAlgo.hh"

namespace openmsx {

class ResampledSoundDevice;

class ResampleTrivial final : public ResampleAlgo
{
public:
	ResampleTrivial(ResampledSoundDevice& input);
	virtual bool generateOutput(int* dataOut, unsigned num,
	                            EmuTime::param time);

private:
	ResampledSoundDevice& input;
};

} // namespace openmsx

#endif
