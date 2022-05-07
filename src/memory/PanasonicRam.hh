#ifndef PANASONICRAM_HH
#define PANASONICRAM_HH

#include "MSXMemoryMapperBase.hh"

namespace openmsx {

class PanasonicMemory;

class PanasonicRam final : public MSXMemoryMapperBase
{
public:
	explicit PanasonicRam(const DeviceConfig& config);

	void writeMem(word address, byte value, EmuTime::param time) override;
	[[nodiscard]] byte* getWriteCacheLine(word start) const override;
	void writeIO(word port, byte value, EmuTime::param time) override;

	void serialize(Archive auto& ar, unsigned version);

private:
	PanasonicMemory& panasonicMemory;
};

} // namespace openmsx

#endif
