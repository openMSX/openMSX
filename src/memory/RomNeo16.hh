#ifndef ROMNEO16_HH
#define ROMNEO16_HH

#include "RomBlocks.hh"

namespace openmsx {

class RomNeo16 final : public Rom16kBBlocks
{
public:
	RomNeo16(const DeviceConfig& config, Rom&& rom);

	void reset(EmuTime::param time) override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	[[nodiscard]] byte* getWriteCacheLine(word address) const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	std::array<uint16_t, 3> blockReg;
};

} // namespace openmsx

#endif
