#ifndef MSXMAPPERIO_HH
#define MSXMAPPERIO_HH

#include "MSXDevice.hh"
#include "SimpleDebuggable.hh"
#include <vector>

namespace openmsx {

class MSXMemoryMapper;

class MSXMapperIO final : public MSXDevice
{
public:
	explicit MSXMapperIO(const DeviceConfig& config);

	byte readIO(word port, EmuTime::param time) override;
	byte peekIO(word port, EmuTime::param time) const override;
	void writeIO(word port, byte value, EmuTime::param time) override;

	void registerMapper(MSXMemoryMapper* mapper);
	void unregisterMapper(MSXMemoryMapper* mapper);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	struct Debuggable final : SimpleDebuggable {
		Debuggable(MSXMotherBoard& motherBoard, const std::string& name);
		byte read(unsigned address, EmuTime::param time) override;
		void write(unsigned address, byte value, EmuTime::param time) override;
	} debuggable;

	std::vector<MSXMemoryMapper*> mappers;

	/**
	 * OR-mask that limits which bits can be read back.
	 * This is set using the MapperReadBackBits tag in the machine config.
	 */
	byte mask;
};
SERIALIZE_CLASS_VERSION(MSXMapperIO, 2);

} // namespace openmsx

#endif
