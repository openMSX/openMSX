#ifndef ROMMAJUTSUSHI_HH
#define ROMMAJUTSUSHI_HH

#include "RomKonami.hh"
#include "DACSound8U.hh"

namespace openmsx {

class RomMajutsushi final : public RomKonami
{
public:
	RomMajutsushi(const DeviceConfig& config, Rom&& rom);

	void reset(EmuTime time) override;
	void writeMem(uint16_t address, byte value, EmuTime time) override;
	[[nodiscard]] byte* getWriteCacheLine(uint16_t address) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	DACSound8U dac;
};

} // namespace openmsx

#endif
