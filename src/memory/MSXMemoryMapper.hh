#ifndef MSXMEMORYMAPPER_HH
#define MSXMEMORYMAPPER_HH

#include "MSXMemoryMapperBase.hh"

namespace openmsx {

class MSXMemoryMapper final : public MSXMemoryMapperBase
{
public:
	explicit MSXMemoryMapper(const DeviceConfig& config);

	void writeIO(uint16_t port, byte value, EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);
};
SERIALIZE_CLASS_VERSION(MSXMemoryMapper, 2);

} // namespace openmsx

#endif
