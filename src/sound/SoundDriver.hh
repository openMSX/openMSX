// $Id$

#ifndef SOUNDDRIVER_HH
#define SOUNDDRIVER_HH

#include "EmuTime.hh"

namespace openmsx {

class SoundDriver
{
public:
	virtual ~SoundDriver() {}

	virtual void lock() = 0;
	virtual void unlock() = 0;

	virtual void mute() = 0;
	virtual void unmute() = 0;

	virtual unsigned getFrequency() const = 0;
	virtual unsigned getSamples() const = 0;

	virtual void updateStream(const EmuTime& time) = 0;

protected:
	SoundDriver() {}
};

} // namespace openmsx

#endif
