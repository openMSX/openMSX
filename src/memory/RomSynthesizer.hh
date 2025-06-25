#ifndef ROMSYNTHESIZER_HH
#define ROMSYNTHESIZER_HH

#include "RomBlocks.hh"
#include "DACSound8U.hh"

namespace openmsx {

class RomSynthesizer final : public Rom16kBBlocks
{
public:
	RomSynthesizer(const DeviceConfig& config, Rom&& rom);

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
