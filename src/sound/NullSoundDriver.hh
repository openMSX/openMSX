#ifndef NULLSOUNDDRIVER_HH
#define NULLSOUNDDRIVER_HH

#include "SoundDriver.hh"

namespace openmsx {

class NullSoundDriver final : public SoundDriver
{
public:
	void mute() override;
	void unmute() override;

	unsigned getFrequency() const override;
	unsigned getSamples() const override;

	void uploadBuffer(int16_t* buffer, unsigned len) override;
};

} // namespace openmsx

#endif
