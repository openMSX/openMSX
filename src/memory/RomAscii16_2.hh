// $Id$

#ifndef ROMASCII16_2_HH
#define ROMASCII16_2_HH

#include "RomAscii16kB.hh"

namespace openmsx {

class RomAscii16_2 : public RomAscii16kB
{
public:
	RomAscii16_2(MSXMotherBoard& motherBoard, const XMLElement& config,
	            std::auto_ptr<Rom> rom);
	virtual ~RomAscii16_2();

	virtual void reset(const EmuTime& time);
	virtual byte readMem(word address, const EmuTime& time);
	virtual const byte* getReadCacheLine(word address) const;
	virtual void writeMem(word address, byte value, const EmuTime& time);
	virtual byte* getWriteCacheLine(word address) const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	byte sramEnabled;
};

REGISTER_MSXDEVICE(RomAscii16_2, "RomAscii16_2");

} // namespace openmsx

#endif
