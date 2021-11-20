#ifndef NULLSOUNDDRIVER_HH
#define NULLSOUNDDRIVER_HH

#include "SoundDriver.hh"

namespace openmsx {

class NullSoundDriver final : public SoundDriver
{
public:
	void mute() override;
	void unmute() override;

	[[nodiscard]] unsigned getFrequency() const override;
	[[nodiscard]] unsigned getSamples() const override;

	void uploadBuffer(float* buffer, unsigned len) override;
};

} // namespace openmsx

#endif
