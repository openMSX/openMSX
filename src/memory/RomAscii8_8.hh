// $Id$

#ifndef ROMASCII8_8_HH
#define ROMASCII8_8_HH

#include "RomBlocks.hh"

namespace openmsx {

class SRAM;

class RomAscii8_8 : public Rom8kBBlocks
{
public:
	enum SubType { ASCII8_8, KOEI_8, KOEI_32, WIZARDRY };
	RomAscii8_8(MSXMotherBoard& motherBoard, const XMLElement& config,
	            std::auto_ptr<Rom> rom, SubType subType);
	virtual ~RomAscii8_8();

	virtual void reset(const EmuTime& time);
	virtual void writeMem(word address, byte value, const EmuTime& time);
	virtual byte* getWriteCacheLine(word address) const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	const byte sramEnableBit;
	const byte sramPages;
	byte sramEnabled;
	byte sramBlock[8];
};

REGISTER_MSXDEVICE(RomAscii8_8, "RomAscii8_8");

} // namespace openmsx

#endif
