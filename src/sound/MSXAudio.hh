// $Id$

#ifndef __MSXAUDIO_HH__
#define __MSXAUDIO_HH__

#include <memory>
#include "MSXIODevice.hh"

using std::auto_ptr;

namespace openmsx {

class Y8950;

class MSXAudio : public MSXIODevice
{
public:
	MSXAudio(const XMLElement& config, const EmuTime& time);
	virtual ~MSXAudio();

	virtual void reset(const EmuTime& time);
	virtual byte readIO(byte port, const EmuTime& time);
	virtual void writeIO(byte port, byte value, const EmuTime& time);

private:
	auto_ptr<Y8950> y8950;
	int registerLatch;
};

} // namespace openmsx

#endif
