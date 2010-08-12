// $Id$

#ifndef SDLSOUNDDRIVER_HH
#define SDLSOUNDDRIVER_HH

#include "SoundDriver.hh"
#include "MemBuffer.hh"
#include "openmsx.hh"
#include "noncopyable.hh"

namespace openmsx {

class SDLSoundDriver : public SoundDriver, private noncopyable
{
public:
	SDLSoundDriver(unsigned frequency, unsigned samples);
	virtual ~SDLSoundDriver();

	virtual void mute();
	virtual void unmute();

	virtual unsigned getFrequency() const;
	virtual unsigned getSamples() const;

	virtual double uploadBuffer(short* buffer, unsigned len);

private:
	void reInit();
	unsigned getBufferFilled() const;
	unsigned getBufferFree() const;
	static void audioCallbackHelper(void* userdata, byte* strm, int len);
	void audioCallback(short* stream, unsigned len);

	MemBuffer<short> mixBuffer;
	double filledStat; /**< average filled status, 1.0 means filled exactly
	                        the right amount, less than 1.0 mean under
	                        filled, more than 1.0 means overfilled. */
	unsigned frequency;
	unsigned fragmentSize;
	unsigned bufferSize;
	unsigned readIdx, writeIdx;
	bool muted;
};

} // namespace openmsx

#endif
