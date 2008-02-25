// $Id$

#ifndef ROMPLAINFLASH_HH
#define ROMPLAINFLASH_HH

#include "MSXRom.hh"
#include <memory>

namespace openmsx {

class Rom;
class AmdFlash;

class RomPlainFlash : public MSXRom
{
public:
	RomPlainFlash(MSXMotherBoard& motherBoard, const XMLElement& config,
	              const EmuTime& time, std::auto_ptr<Rom> rom);
	virtual ~RomPlainFlash();

	virtual void reset(const EmuTime& time);
	virtual byte peek(word address, const EmuTime& time) const;
	virtual byte readMem(word address, const EmuTime& time);
	virtual void writeMem(word address, byte value, const EmuTime& time);
	virtual const byte* getReadCacheLine(word address) const;
	virtual byte* getWriteCacheLine(word address) const;

private:
	const std::auto_ptr<AmdFlash> flash;
};

} // namespace openmsx

#endif
