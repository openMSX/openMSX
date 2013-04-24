#ifndef PANASONICRAM_HH
#define PANASONICRAM_HH

#include "MSXMemoryMapper.hh"

namespace openmsx {

class PanasonicMemory;

class PanasonicRam : public MSXMemoryMapper
{
public:
	explicit PanasonicRam(const DeviceConfig& config);

	virtual void writeMem(word address, byte value, EmuTime::param time);
	virtual byte* getWriteCacheLine(word start) const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	PanasonicMemory& panasonicMemory;
};

} // namespace openmsx

#endif
