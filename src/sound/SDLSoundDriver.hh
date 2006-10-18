// $Id$

#ifndef SDLSOUNDDRIVER_HH
#define SDLSOUNDDRIVER_HH

#include "SoundDriver.hh"
#include "openmsx.hh"
#include "noncopyable.hh"

namespace openmsx {

class Mixer;

class SDLSoundDriver : public SoundDriver, private noncopyable
{
public:
	SDLSoundDriver(Mixer& mixer, unsigned frequency, unsigned samples);
	virtual ~SDLSoundDriver();

	virtual void lock();
	virtual void unlock();

	virtual void mute();
	virtual void unmute();

	virtual unsigned getFrequency() const;
	virtual unsigned getSamples() const;

	virtual double uploadBuffer(short* buffer, unsigned len);

private:
	static void audioCallbackHelper(void* userdata, byte* strm, int len);
	void audioCallback(short* stream, unsigned len);

	Mixer& mixer;
	unsigned frequency;
	short* mixBuffer;
	unsigned bufferSize;
	unsigned bufferMask;
	unsigned readIdx, writeIdx;
	int available; // available samples at last callback
	bool muted;
};

} // namespace openmsx

#endif
