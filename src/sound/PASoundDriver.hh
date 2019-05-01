#ifndef PASOUNDDRIVER_HH
#define PASOUNDDRIVER_HH

#include "components.hh"
#if COMPONENT_PORTAUDIO

#include "SoundDriver.hh"
#include <portaudio.h>

namespace openmsx {

class Reactor;

class PASoundDriver final : public SoundDriver
{
public:
	PASoundDriver(const PASoundDriver&) = delete;
	PASoundDriver& operator=(const PASoundDriver&) = delete;

	PASoundDriver(Reactor& reactor, unsigned wantedFreq, unsigned samples);
	~PASoundDriver() override;

	void mute() override;
	void unmute() override;

	unsigned getFrequency() const override;
	unsigned getSamples() const override;

	void uploadBuffer(float* buffer, unsigned len) override;

private:
	PaStream* stream;

	Reactor& reactor;
	unsigned frequency;
	unsigned samples;
	bool muted;
};

} // namespace openmsx

#endif
#endif
