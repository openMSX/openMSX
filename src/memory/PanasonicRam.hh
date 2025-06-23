#ifndef PANASONICRAM_HH
#define PANASONICRAM_HH

#include "MSXMemoryMapperBase.hh"

namespace openmsx {

class PanasonicMemory;

class PanasonicRam final : public MSXMemoryMapperBase
{
public:
	explicit PanasonicRam(const DeviceConfig& config);

	void writeMem(uint16_t address, byte value, EmuTime::param time) override;
	[[nodiscard]] byte* getWriteCacheLine(uint16_t start) override;
	void writeIO(uint16_t port, byte value, EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	PanasonicMemory& panasonicMemory;
};

} // namespace openmsx

#endif
