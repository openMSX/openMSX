#ifndef RESAMPLETRIVIAL_HH
#define RESAMPLETRIVIAL_HH

#include "ResampleAlgo.hh"

namespace openmsx {

class ResampledSoundDevice;

class ResampleTrivial final : public ResampleAlgo
{
public:
	explicit ResampleTrivial(ResampledSoundDevice& input);
	bool generateOutput(int* dataOut, unsigned num,
	                    EmuTime::param time) override;

private:
	ResampledSoundDevice& input;
};

} // namespace openmsx

#endif
