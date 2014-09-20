#ifndef PANASONICRAM_HH
#define PANASONICRAM_HH

#include "MSXMemoryMapper.hh"

namespace openmsx {

class PanasonicMemory;

class PanasonicRam final : public MSXMemoryMapper
{
public:
	explicit PanasonicRam(const DeviceConfig& config);

	void writeMem(word address, byte value, EmuTime::param time) override;
	byte* getWriteCacheLine(word start) const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	PanasonicMemory& panasonicMemory;
};

} // namespace openmsx

#endif
