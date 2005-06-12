// $Id$

#ifndef NULLSOUNDDRIVER_HH
#define NULLSOUNDDRIVER_HH

#include "SoundDriver.hh"

namespace openmsx {

class NullSoundDriver : public SoundDriver
{
public:
	NullSoundDriver();
	virtual ~NullSoundDriver();

	virtual void lock();
	virtual void unlock();

	virtual void mute();
	virtual void unmute();

	virtual unsigned getFrequency() const;
	virtual unsigned getSamples() const;

	virtual void updateStream(const EmuTime& time);
};

} // namespace openmsx

#endif
