// $Id$

#ifndef __MSXMOONSOUND_HH__
#define __MSXMOONSOUND_HH__

#include "MSXIODevice.hh"

namespace openmsx {

class MSXMoonSound : public MSXIODevice
{
public:
	MSXMoonSound(Config* config, const EmuTime& time);
	virtual ~MSXMoonSound();
	
	virtual void reset(const EmuTime& time);
	virtual byte readIO(byte port, const EmuTime& time);
	virtual void writeIO(byte port, byte value, const EmuTime& time);

private:
	class YMF262* ymf262;
	class YMF278* ymf278;
	int opl3latch;
	byte opl4latch;
};

} // namespace openmsx

#endif
