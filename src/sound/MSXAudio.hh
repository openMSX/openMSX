// $Id$

#ifndef __MSXAUDIO_HH__
#define __MSXAUDIO_HH__

#include "MSXIODevice.hh"

namespace openmsx {

class Y8950;

class MSXAudio : public MSXIODevice
{
public:
	MSXAudio(Config* config, const EmuTime& time);
	virtual ~MSXAudio();

	virtual void reset(const EmuTime& time);
	virtual byte readIO(byte port, const EmuTime& time);
	virtual void writeIO(byte port, byte value, const EmuTime& time);

private:
	Y8950* y8950;
	int registerLatch;
};

} // namespace openmsx

#endif
