#ifndef RESAMPLETRIVIAL_HH
#define RESAMPLETRIVIAL_HH

#include "ResampleAlgo.hh"

namespace openmsx {

class ResampledSoundDevice;

class ResampleTrivial final : public ResampleAlgo
{
public:
	explicit ResampleTrivial(ResampledSoundDevice& input);
	bool generateOutputImpl(float* dataOut, size_t num,
	                        EmuTime time) override;
};

} // namespace openmsx

#endif
