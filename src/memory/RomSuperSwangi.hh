#ifndef ROMSUPERSWANGI_HH
#define ROMSUPERSWANGI_HH

#include "RomBlocks.hh"

namespace openmsx {

class RomSuperSwangi final : public Rom16kBBlocks
{
public:
	RomSuperSwangi(const DeviceConfig& config, Rom&& rom);

	void reset(EmuTime time) override;
	void writeMem(uint16_t address, byte value, EmuTime time) override;
	[[nodiscard]] byte* getWriteCacheLine(uint16_t address) override;
};

} // namespace openmsx

#endif
