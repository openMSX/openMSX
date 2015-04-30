#ifndef ROMNATIONAL_HH
#define ROMNATIONAL_HH

#include "RomBlocks.hh"

namespace openmsx {

class RomNational final : public Rom16kBBlocks
{
public:
	RomNational(const DeviceConfig& config, Rom&& rom);

	void reset(EmuTime::param time) override;
	byte peekMem(word address, EmuTime::param time) const override;
	byte readMem(word address, EmuTime::param time) override;
	const byte* getReadCacheLine(word address) const override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	byte* getWriteCacheLine(word address) const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	int sramAddr;
	byte control;
	byte bankSelect[4];
};

} // namespace openmsx

#endif
