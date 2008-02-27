// $Id$

#ifndef ROMSHOCKWARE_HH
#define ROMSHOCKWARE_HH

#include "MSXRom.hh"
#include <memory>

namespace openmsx {

class Rom;
class AmdFlash;

class RomShockware : public MSXRom
{
public:
	RomShockware(MSXMotherBoard& motherBoard, const XMLElement& config,
	              const EmuTime& time, std::auto_ptr<Rom> rom);
	virtual ~RomShockware();

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
