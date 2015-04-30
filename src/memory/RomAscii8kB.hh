#ifndef ROMASCII8KB_HH
#define ROMASCII8KB_HH

#include "RomBlocks.hh"

namespace openmsx {

class RomAscii8kB : public Rom8kBBlocks
{
public:
	RomAscii8kB(const DeviceConfig& config, Rom&& rom);
	virtual ~RomAscii8kB() {}

	void reset(EmuTime::param time) override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	byte* getWriteCacheLine(word address) const override;
};

} // namespace openmsx

#endif
