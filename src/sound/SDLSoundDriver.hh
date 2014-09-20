#ifndef SDLSOUNDDRIVER_HH
#define SDLSOUNDDRIVER_HH

#include "SoundDriver.hh"
#include "MemBuffer.hh"
#include "openmsx.hh"
#include "noncopyable.hh"

namespace openmsx {

class Reactor;

class SDLSoundDriver final : public SoundDriver, private noncopyable
{
public:
	SDLSoundDriver(Reactor& reactor,
	               unsigned frequency, unsigned samples);
	~SDLSoundDriver();

	virtual void mute();
	virtual void unmute();

	virtual unsigned getFrequency() const;
	virtual unsigned getSamples() const;

	virtual void uploadBuffer(short* buffer, unsigned len);

private:
	void reInit();
	unsigned getBufferFilled() const;
	unsigned getBufferFree() const;
	static void audioCallbackHelper(void* userdata, byte* strm, int len);
	void audioCallback(short* stream, unsigned len);

	Reactor& reactor;
	MemBuffer<short> mixBuffer;
	unsigned frequency;
	unsigned fragmentSize;
	unsigned readIdx, writeIdx;
	bool muted;
};

} // namespace openmsx

#endif
