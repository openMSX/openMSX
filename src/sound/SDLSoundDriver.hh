#ifndef SDLSOUNDDRIVER_HH
#define SDLSOUNDDRIVER_HH

#include "SoundDriver.hh"
#include "MemBuffer.hh"
#include "openmsx.hh"

namespace openmsx {

class Reactor;

class SDLSoundDriver final : public SoundDriver
{
public:
	SDLSoundDriver(const SDLSoundDriver&) = delete;
	SDLSoundDriver& operator=(const SDLSoundDriver&) = delete;

	SDLSoundDriver(Reactor& reactor, unsigned wantedFreq, unsigned samples);
	~SDLSoundDriver();

	void mute() override;
	void unmute() override;

	unsigned getFrequency() const override;
	unsigned getSamples() const override;

	void uploadBuffer(int16_t* buffer, unsigned len) override;

private:
	void reInit();
	unsigned getBufferFilled() const;
	unsigned getBufferFree() const;
	static void audioCallbackHelper(void* userdata, byte* strm, int len);
	void audioCallback(int16_t* stream, unsigned len);

	Reactor& reactor;
	MemBuffer<int16_t> mixBuffer;
	unsigned mixBufferSize;
	unsigned frequency;
	unsigned fragmentSize;
	unsigned readIdx, writeIdx;
	bool muted;
};

} // namespace openmsx

#endif
