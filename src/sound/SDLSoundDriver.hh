#ifndef SDLSOUNDDRIVER_HH
#define SDLSOUNDDRIVER_HH

#include "SoundDriver.hh"
#include "SDLSurfacePtr.hh"
#include "MemBuffer.hh"
#include <SDL.h>

namespace openmsx {

class Reactor;

class SDLSoundDriver final : public SoundDriver
{
public:
	SDLSoundDriver(const SDLSoundDriver&) = delete;
	SDLSoundDriver& operator=(const SDLSoundDriver&) = delete;

	SDLSoundDriver(Reactor& reactor, unsigned wantedFreq, unsigned samples);
	~SDLSoundDriver() override;

	void mute() override;
	void unmute() override;

	[[nodiscard]] unsigned getFrequency() const override;
	[[nodiscard]] unsigned getSamples() const override;

	void uploadBuffer(float* buffer, unsigned len) override;

private:
	void reInit();
	[[nodiscard]] unsigned getBufferFilled() const;
	[[nodiscard]] unsigned getBufferFree() const;
	static void audioCallbackHelper(void* userdata, uint8_t* strm, int len);
	void audioCallback(float* stream, unsigned len);

private:
	Reactor& reactor;
	SDL_AudioDeviceID deviceID;
	MemBuffer<float> mixBuffer;
	unsigned mixBufferSize;
	unsigned frequency;
	unsigned fragmentSize;
	unsigned readIdx, writeIdx;
	bool muted;
	SDLSubSystemInitializer<SDL_INIT_AUDIO> audioInitializer;
};

} // namespace openmsx

#endif
