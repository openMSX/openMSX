// $Id$

#ifndef __ROMNATIONAL_HH__
#define __ROMNATIONAL_HH__

#include "Rom16kBBlocks.hh"

namespace openmsx {

class SRAM;

class RomNational : public Rom16kBBlocks
{
public:
	RomNational(const XMLElement& config, const EmuTime& time,
	            std::auto_ptr<Rom> rom);
	virtual ~RomNational();
	
	virtual void reset(const EmuTime& time);
	virtual byte readMem(word address, const EmuTime& time);
	virtual const byte* getReadCacheLine(word address) const;
	virtual void writeMem(word address, byte value, const EmuTime& time);
	virtual byte* getWriteCacheLine(word address) const;

private:
	const std::auto_ptr<SRAM> sram;
	int sramAddr;
	byte control;
	byte bankSelect[4];
};

} // namespace openmsx

#endif
