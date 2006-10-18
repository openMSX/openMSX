// $Id$

#ifndef SOUNDDRIVER_HH
#define SOUNDDRIVER_HH

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

	virtual double uploadBuffer(short* buffer, unsigned len) = 0;

protected:
	SoundDriver() {}
};

} // namespace openmsx

#endif
