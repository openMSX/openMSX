// $Id$

#ifndef ROMPANASONIC_HH
#define ROMPANASONIC_HH

#include "RomBlocks.hh"

namespace openmsx {

class RomPanasonic : public Rom8kBBlocks
{
public:
	RomPanasonic(MSXMotherBoard& motherBoard, const XMLElement& config,
	             std::auto_ptr<Rom> rom);
	virtual ~RomPanasonic();

	virtual void reset(const EmuTime& time);
	virtual byte peekMem(word address, const EmuTime& time) const;
	virtual byte readMem(word address, const EmuTime& time);
	virtual const byte* getReadCacheLine(word address) const;
	virtual void writeMem(word address, byte value, const EmuTime& time);
	virtual byte* getWriteCacheLine(word address) const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void changeBank(byte region, int bank);
	
	int maxSRAMBank;

	int bankSelect[8];
	byte control;
};

REGISTER_MSXDEVICE(RomPanasonic, "RomPanasonic");

} // namespace openmsx

#endif
