// $Id$

#ifndef ROMNATIONAL_HH
#define ROMNATIONAL_HH

#include "Rom16kBBlocks.hh"

namespace openmsx {

class SRAM;

class RomNational : public Rom16kBBlocks
{
public:
	RomNational(MSXMotherBoard& motherBoard, const XMLElement& config,
	            const EmuTime& time, std::auto_ptr<Rom> rom);
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
