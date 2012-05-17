// $Id$

#ifndef ROMHOLYQURAN2_HH
#define ROMHOLYQURAN2_HH

#include "MSXRom.hh"

namespace openmsx {

class MSXCPU;

class RomHolyQuran2 : public MSXRom
{
public:
	RomHolyQuran2(MSXMotherBoard& motherBoard, const DeviceConfig& config,
	             std::auto_ptr<Rom> rom);

	virtual void reset(EmuTime::param time);
	virtual byte readMem(word address, EmuTime::param time);
	virtual byte peekMem(word address, EmuTime::param time) const;
	virtual void writeMem(word address, byte value, EmuTime::param time);
	virtual const byte* getReadCacheLine(word start) const;
	virtual byte* getWriteCacheLine(word start) const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	MSXCPU& cpu;
	const byte* bank[4];
	bool decrypt;
};

} // namespace openmsx

#endif
