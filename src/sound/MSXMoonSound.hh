// $Id$

#ifndef __MSXMOONSOUND_HH__
#define __MSXMOONSOUND_HH__

#include <memory>
#include "MSXIODevice.hh"

using std::auto_ptr;

namespace openmsx {

class YMF262;
class YMF278;

class MSXMoonSound : public MSXIODevice
{
public:
	MSXMoonSound(const XMLElement& config, const EmuTime& time);
	virtual ~MSXMoonSound();
	
	virtual void reset(const EmuTime& time);
	virtual byte readIO(byte port, const EmuTime& time);
	virtual void writeIO(byte port, byte value, const EmuTime& time);

private:
	auto_ptr<YMF262> ymf262;
	auto_ptr<YMF278> ymf278;
	int opl3latch;
	byte opl4latch;
};

} // namespace openmsx

#endif
