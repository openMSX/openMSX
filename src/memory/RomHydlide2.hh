// $Id$

#ifndef ROMHYDLIDE2_HH
#define ROMHYDLIDE2_HH

#include "RomAscii16kB.hh"

namespace openmsx {

class SRAM;

class RomHydlide2 : public RomAscii16kB
{
public:
	RomHydlide2(MSXMotherBoard& motherBoard, const XMLElement& config,
	            const EmuTime& time, std::auto_ptr<Rom> rom);
	virtual ~RomHydlide2();

	virtual void reset(const EmuTime& time);
	virtual byte readMem(word address, const EmuTime& time);
	virtual const byte* getReadCacheLine(word address) const;
	virtual void writeMem(word address, byte value, const EmuTime& time);
	virtual byte* getWriteCacheLine(word address) const;

private:
	const std::auto_ptr<SRAM> sram;
	byte sramEnabled;
};

} // namespace openmsx

#endif
