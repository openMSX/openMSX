#ifndef SDLSOUNDDRIVER_HH
#define SDLSOUNDDRIVER_HH

#include "SoundDriver.hh"

#include "SDLSurfacePtr.hh"

#include "MemBuffer.hh"

#include <SDL3/SDL.h>

namespace openmsx {

class Reactor;

class SDLSoundDriver final : public SoundDriver
{
public:
	SDLSoundDriver(Reactor& reactor, unsigned wantedFreq, unsigned samples);
	SDLSoundDriver(const SDLSoundDriver&) = delete;
	SDLSoundDriver(SDLSoundDriver&&) = delete;
	SDLSoundDriver& operator=(const SDLSoundDriver&) = delete;
	SDLSoundDriver& operator=(SDLSoundDriver&&) = delete;
	~SDLSoundDriver() override;

	void mute() override;
	void unmute() override;

	[[nodiscard]] unsigned getFrequency() const override;
	[[nodiscard]] unsigned getSamples() const override;

	void uploadBuffer(std::span<const StereoFloat> buffer) override;

private:
	void reInit();
	[[nodiscard]] unsigned getBufferFilled() const;
	[[nodiscard]] unsigned getBufferFree() const;
	static void audioCallbackHelper(void* userdata, SDL_AudioStream* stream, int additional_amount, int total_amount);
	void audioCallback();

private:
	Reactor& reactor;
	SDL_AudioStream* stream;
	MemBuffer<StereoFloat> mixBuffer;
	unsigned frequency;
	unsigned fragmentSize;
	unsigned readIdx = 0, writeIdx = 0;
	bool muted = true;
	[[no_unique_address]] SDLSubSystemInitializer<SDL_INIT_AUDIO> audioInitializer;
};

} // namespace openmsx

#endif
