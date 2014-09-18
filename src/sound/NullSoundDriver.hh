#ifndef NULLSOUNDDRIVER_HH
#define NULLSOUNDDRIVER_HH

#include "SoundDriver.hh"

namespace openmsx {

class NullSoundDriver final : public SoundDriver
{
public:
	virtual void mute();
	virtual void unmute();

	virtual unsigned getFrequency() const;
	virtual unsigned getSamples() const;

	virtual void uploadBuffer(short* buffer, unsigned len);
};

} // namespace openmsx

#endif
