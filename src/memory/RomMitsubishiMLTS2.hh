#ifndef ROMMITSUBISHIMLTS2_HH
#define ROMMITSUBISHIMLTS2_HH

#include "RomBlocks.hh"
#include <memory>

// PLEASE NOTE!
//
// This mapper is work in progress. It's just a guess based on some reverse
// engineering of the ROM by BiFi. It even contains some debug prints :)

namespace openmsx {

class Ram;

class RomMitsubishiMLTS2 : public Rom8kBBlocks
{
public:
	RomMitsubishiMLTS2(const DeviceConfig& config, Rom&& rom);
	virtual ~RomMitsubishiMLTS2();

	virtual void reset(EmuTime::param time);
	virtual void writeMem(word address, byte value, EmuTime::param time);
	virtual byte readMem(word address, EmuTime::param time) override;
	virtual byte peekMem(word address, EmuTime::param time) const override;
	virtual byte* getWriteCacheLine(word address) const override;
	virtual const byte* getReadCacheLine(word address) const override;

private:
	const std::unique_ptr<Ram> ram;
};

} // namespace openmsx

#endif
