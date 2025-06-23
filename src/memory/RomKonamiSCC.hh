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
	[[nodiscard]] byte peekMem(uint16_t address, EmuTime::param time) const override;
	[[nodiscard]] byte readMem(uint16_t address, EmuTime::param time) override;
	[[nodiscard]] const byte* getReadCacheLine(uint16_t address) const override;
	void writeMem(uint16_t address, byte value, EmuTime::param time) override;
	[[nodiscard]] byte* getWriteCacheLine(uint16_t address) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void bankSwitch(unsigned page, unsigned block);

private:
	SCC scc;
	bool sccEnabled;
};

} // namespace openmsx

#endif
