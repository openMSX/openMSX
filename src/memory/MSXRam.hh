// $Id$

#ifndef __MSXSIMPLE64KB_HH__
#define __MSXSIMPLE64KB_HH__

#include <memory>
#include "MSXDevice.hh"

using std::auto_ptr;

namespace openmsx {

class Ram;

class MSXRam : public MSXDevice
{
public:
	MSXRam(const XMLElement& config, const EmuTime& time);
	virtual ~MSXRam();
	
	virtual void reInit(const EmuTime& time);
	virtual byte readMem(word address, const EmuTime& time);
	virtual void writeMem(word address, byte value, const EmuTime& time);  
	virtual const byte* getReadCacheLine(word start) const;
	virtual byte* getWriteCacheLine(word start) const;

private:
	inline bool isInside(word address) const;

	int base;
	int end;
	auto_ptr<Ram> ram;
};

} // namespace openmsx
#endif
