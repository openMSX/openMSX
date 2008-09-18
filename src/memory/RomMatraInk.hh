// $Id$

#ifndef ROMMATRAINK_HH
#define ROMMATRAINK_HH

#include "MSXRom.hh"
#include "serialize.hh"
#include <memory>

namespace openmsx {

class Rom;
class AmdFlash;

class RomMatraInk : public MSXRom
{
public:
	RomMatraInk(MSXMotherBoard& motherBoard, const XMLElement& config,
	            std::auto_ptr<Rom> rom);
	virtual ~RomMatraInk();

	virtual void reset(const EmuTime& time);
	virtual byte peek(word address, const EmuTime& time) const;
	virtual byte readMem(word address, const EmuTime& time);
	virtual void writeMem(word address, byte value, const EmuTime& time);
	virtual const byte* getReadCacheLine(word address) const;
	virtual byte* getWriteCacheLine(word address) const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	const std::auto_ptr<AmdFlash> flash;
};

} // namespace openmsx

#endif
