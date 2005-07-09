// $Id$

#ifndef SDLSOUNDDRIVER_HH
#define SDLSOUNDDRIVER_HH

#include "SoundDriver.hh"
#include "Schedulable.hh"
#include "SettingListener.hh"
#include <SDL.h>

namespace openmsx {

class Mixer;
class IntegerSetting;
class BooleanSetting;

class SDLSoundDriver : public SoundDriver, private Schedulable,
                       private SettingListener
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

	virtual void updateStream(const EmuTime& time);

private:
	static void audioCallbackHelper(void* userdata, Uint8* strm, int len);
	void audioCallback(short* stream, unsigned len);
	void updtStrm(unsigned samples, const EmuTime& start,
	              const EmuDuration& sampDur);
	void updtStrm2(unsigned samples, const EmuTime& start,
	               const EmuDuration& sampDur);
	void reInit();

	// Schedulable
	virtual void executeUntil(const EmuTime& time, int userData);
	virtual const std::string& schedName() const;

	// SettingListener
	virtual void update(const Setting* setting);

	Mixer& mixer;
	SDL_AudioSpec audioSpec;

	bool muted;
	short* mixBuffer;
	unsigned bufferSize;
	unsigned readPtr, writePtr;
	EmuTime prevTime;
	EmuDuration interval1;
	EmuDuration intervalAverage;

	IntegerSetting& speedSetting;
	BooleanSetting& throttleSetting;
};

} // namespace openmsx

#endif
