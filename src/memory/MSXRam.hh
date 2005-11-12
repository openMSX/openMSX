// $Id$

#ifndef MSXSIMPLE64KB_HH
#define MSXSIMPLE64KB_HH

#include "MSXDevice.hh"
#include <memory>

namespace openmsx {

class Ram;

class MSXRam : public MSXDevice
{
public:
	MSXRam(MSXMotherBoard& motherBoard, const XMLElement& config,
	       const EmuTime& time);

	virtual void powerUp(const EmuTime& time);
	virtual byte readMem(word address, const EmuTime& time);
	virtual void writeMem(word address, byte value, const EmuTime& time);
	virtual const byte* getReadCacheLine(word start) const;
	virtual byte* getWriteCacheLine(word start) const;

private:
	inline word translate(word address) const;
	unsigned base;
	unsigned size;
	std::auto_ptr<Ram> ram;
};

} // namespace openmsx

#endif
