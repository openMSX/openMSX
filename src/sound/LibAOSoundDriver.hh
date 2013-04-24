#ifndef LIBAOSOUNDDRIVER_HH
#define LIBAOSOUNDDRIVER_HH

#include "SoundDriver.hh"
#include "noncopyable.hh"

struct ao_device;

namespace openmsx {

class LibAOSoundDriver : public SoundDriver, private noncopyable
{
public:
	LibAOSoundDriver(unsigned sampleRate, unsigned bufferSize);
	virtual ~LibAOSoundDriver();

	virtual void mute();
	virtual void unmute();

	virtual unsigned getFrequency() const;
	virtual unsigned getSamples() const;

	virtual void uploadBuffer(short* buffer, unsigned len);

private:
	ao_device* device;
	unsigned sampleRate;
	unsigned bufferSize;
};

} // namespace openmsx

#endif // LIBAOSOUNDDRIVER_HH
