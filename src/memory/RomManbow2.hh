// $Id$

#ifndef ROMMANBOW2_HH
#define ROMMANBOW2_HH

#include "MSXRom.hh"
#include "RomTypes.hh"
#include <memory>

namespace openmsx {

class SCC;
class AmdFlash;

class RomManbow2 : public MSXRom
{
public:
	RomManbow2(MSXMotherBoard& motherBoard, const XMLElement& config,
	           std::auto_ptr<Rom> rom, RomType type);
	virtual ~RomManbow2();

	virtual void reset(const EmuTime& time);
	virtual byte peek(word address, const EmuTime& time) const;
	virtual byte readMem(word address, const EmuTime& time);
	virtual void writeMem(word address, byte value, const EmuTime& time);
	virtual const byte* getReadCacheLine(word address) const;
	virtual byte* getWriteCacheLine(word address) const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void setRom(unsigned region, unsigned block);

	const std::auto_ptr<SCC> scc;
	const std::auto_ptr<AmdFlash> flash;
	byte bank[4];
	bool sccEnabled;
};

REGISTER_MSXDEVICE(RomManbow2, "RomManbow2");

} // namespace openmsx

#endif
