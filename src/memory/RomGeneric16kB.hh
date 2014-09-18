#ifndef ROMGENERIC16KB_HH
#define ROMGENERIC16KB_HH

#include "RomBlocks.hh"

namespace openmsx {

class RomGeneric16kB final : public Rom16kBBlocks
{
public:
	RomGeneric16kB(const DeviceConfig& config, std::unique_ptr<Rom> rom);

	virtual void reset(EmuTime::param time);
	virtual void writeMem(word address, byte value, EmuTime::param time);
	virtual byte* getWriteCacheLine(word address) const;
};

} // namespace openmsx

#endif
