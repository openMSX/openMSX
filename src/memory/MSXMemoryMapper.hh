#ifndef MSXMEMORYMAPPER_HH
#define MSXMEMORYMAPPER_HH

#include "MSXMemoryMapperBase.hh"

namespace openmsx {

class MSXMemoryMapper final : public MSXMemoryMapperBase
{
public:
	explicit MSXMemoryMapper(const DeviceConfig& config);

	void writeIO(word port, byte value, EmuTime::param time) override;

	void serialize(Archive auto& ar, unsigned version);
};
SERIALIZE_CLASS_VERSION(MSXMemoryMapper, 2);

} // namespace openmsx

#endif
