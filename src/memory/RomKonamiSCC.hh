#ifndef ROMKONAMISCC_HH
#define ROMKONAMISCC_HH

#include "RomBlocks.hh"

namespace openmsx {

class SCC;

class RomKonamiSCC final : public Rom8kBBlocks
{
public:
	RomKonamiSCC(const DeviceConfig& config, std::unique_ptr<Rom> rom);
	~RomKonamiSCC();

	void powerUp(EmuTime::param time) override;
	void reset(EmuTime::param time) override;
	byte peekMem(word address, EmuTime::param time) const override;
	byte readMem(word address, EmuTime::param time) override;
	const byte* getReadCacheLine(word address) const override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	byte* getWriteCacheLine(word address) const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	const std::unique_ptr<SCC> scc;
	bool sccEnabled;
};

} // namespace openmsx

#endif
