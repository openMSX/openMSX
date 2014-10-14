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

	SDLSoundDriver(Reactor& reactor,
	               unsigned frequency, unsigned samples);
	~SDLSoundDriver();

	void mute() override;
	void unmute() override;

	unsigned getFrequency() const override;
	unsigned getSamples() const override;

	void uploadBuffer(short* buffer, unsigned len) override;

private:
	void reInit();
	unsigned getBufferFilled() const;
	unsigned getBufferFree() const;
	static void audioCallbackHelper(void* userdata, byte* strm, int len);
	void audioCallback(short* stream, unsigned len);

	Reactor& reactor;
	MemBuffer<short> mixBuffer;
	unsigned mixBufferSize;
	unsigned frequency;
	unsigned fragmentSize;
	unsigned readIdx, writeIdx;
	bool muted;
};

} // namespace openmsx

#endif
