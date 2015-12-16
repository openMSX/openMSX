#ifndef ROMMSXWRITE_HH
#define ROMMSXWRITE_HH

#include "RomBlocks.hh"

namespace openmsx {

class RomMSXWrite : public Rom16kBBlocks
{
public:
	RomMSXWrite(const DeviceConfig& config, Rom&& rom);
	virtual ~RomMSXWrite() {}

	void reset(EmuTime::param time) override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	byte* getWriteCacheLine(word address) const override;
};

REGISTER_BASE_CLASS(RomMSXWrite, "RomMSXWrite");

} // namespace openmsx

#endif
