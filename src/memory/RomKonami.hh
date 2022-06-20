#ifndef ROMKONAMI_HH
#define ROMKONAMI_HH

#include "RomBlocks.hh"

namespace openmsx {

class RomKonami : public Rom8kBBlocks
{
public:
	RomKonami(const DeviceConfig& config, Rom&& rom);

	void reset(EmuTime::param time) override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	[[nodiscard]] byte* getWriteCacheLine(word address) const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);
};

REGISTER_BASE_CLASS(RomKonami, "RomKonami");

} // namespace openmsx

#endif
