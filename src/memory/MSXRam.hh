// $Id$

#ifndef __MSXSIMPLE64KB_HH__
#define __MSXSIMPLE64KB_HH__

#include <memory>
#include "MSXMemDevice.hh"

using std::auto_ptr;

namespace openmsx {

class Ram;

class MSXRam : public MSXMemDevice
{
public:
	MSXRam(const XMLElement& config, const EmuTime& time);
	virtual ~MSXRam();
	
	virtual void reset(const EmuTime& time);
	
	virtual byte readMem(word address, const EmuTime& time);
	virtual void writeMem(word address, byte value, const EmuTime& time);  
	virtual const byte* getReadCacheLine(word start) const;
	virtual byte* getWriteCacheLine(word start) const;

private:
	inline bool isInside(word address) const;

	int base;
	int end;
	bool slowDrainOnReset;
	auto_ptr<Ram> ram;
};

} // namespace openmsx
#endif
