#ifndef ROMASCII16KB_HH
#define ROMASCII16KB_HH

#include "RomBlocks.hh"

namespace openmsx {

class RomAscii16kB : public Rom16kBBlocks
{
public:
	RomAscii16kB(const DeviceConfig& config, Rom&& rom);

	void reset(EmuTime::param time) override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	[[nodiscard]] byte* getWriteCacheLine(word address) override;
};

REGISTER_BASE_CLASS(RomAscii16kB, "RomAscii16kB");

} // namespace openmsx

#endif
