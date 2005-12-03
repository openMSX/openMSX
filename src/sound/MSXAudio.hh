// $Id$

#ifndef MSXAUDIO_HH
#define MSXAUDIO_HH

#include "MSXDevice.hh"
#include <memory>

namespace openmsx {

class Y8950;

class MSXAudio : public MSXDevice
{
public:
	MSXAudio(MSXMotherBoard& motherBoard, const XMLElement& config,
	         const EmuTime& time);
	virtual ~MSXAudio();

	virtual void reset(const EmuTime& time);
	virtual byte readIO(word port, const EmuTime& time);
	virtual byte peekIO(word port, const EmuTime& time) const;
	virtual void writeIO(word port, byte value, const EmuTime& time);

private:
	std::auto_ptr<Y8950> y8950;
	int registerLatch;
};

} // namespace openmsx

#endif
