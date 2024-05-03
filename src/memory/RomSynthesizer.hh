#ifndef ROMSYNTHESIZER_HH
#define ROMSYNTHESIZER_HH

#include "RomBlocks.hh"
#include "DACSound8U.hh"

namespace openmsx {

class RomSynthesizer final : public Rom16kBBlocks
{
public:
	RomSynthesizer(const DeviceConfig& config, Rom&& rom);

	void reset(EmuTime::param time) override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	[[nodiscard]] byte* getWriteCacheLine(word address) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	DACSound8U dac;
};

} // namespace openmsx

#endif
