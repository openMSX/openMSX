#ifndef ROMKONAMISCC_HH
#define ROMKONAMISCC_HH

#include "RomBlocks.hh"
#include "SCC.hh"

namespace openmsx {

class RomKonamiSCC final : public Rom8kBBlocks
{
public:
	RomKonamiSCC(const DeviceConfig& config, Rom&& rom);

	void powerUp(EmuTime::param time) override;
	void reset(EmuTime::param time) override;
	[[nodiscard]] byte peekMem(word address, EmuTime::param time) const override;
	[[nodiscard]] byte readMem(word address, EmuTime::param time) override;
	[[nodiscard]] const byte* getReadCacheLine(word address) const override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	[[nodiscard]] byte* getWriteCacheLine(word address) const override;

	void serialize(Archive auto& ar, unsigned version);

private:
	SCC scc;
	bool sccEnabled;
};

} // namespace openmsx

#endif
