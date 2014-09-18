#ifndef ROMSUPERSWANGI_HH
#define ROMSUPERSWANGI_HH

#include "RomBlocks.hh"

namespace openmsx {

class RomSuperSwangi final : public Rom16kBBlocks
{
public:
	RomSuperSwangi(const DeviceConfig& config, std::unique_ptr<Rom> rom);

	virtual void reset(EmuTime::param time);
	virtual void writeMem(word address, byte value, EmuTime::param time);
	virtual byte* getWriteCacheLine(word address) const;
};

} // namespace openmsx

#endif
