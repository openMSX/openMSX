// $Id$

#ifndef MSXAUDIO_HH
#define MSXAUDIO_HH

#include <memory>
#include "MSXDevice.hh"

namespace openmsx {

class Y8950;

class MSXAudio : public MSXDevice
{
public:
	MSXAudio(const XMLElement& config, const EmuTime& time);
	virtual ~MSXAudio();

	virtual void reset(const EmuTime& time);
	virtual byte readIO(byte port, const EmuTime& time);
	virtual byte peekIO(byte port, const EmuTime& time) const;
	virtual void writeIO(byte port, byte value, const EmuTime& time);

private:
	std::auto_ptr<Y8950> y8950;
	int registerLatch;
};

} // namespace openmsx

#endif
