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
	/**
	 * Updates the "mask" field after a mapper was registered or unregistered.
	 */
	void updateMask();

	struct Debuggable final : SimpleDebuggable {
		Debuggable(MSXMotherBoard& motherBoard, const std::string& name);
		byte read(unsigned address, EmuTime::param time) override;
		void write(unsigned address, byte value, EmuTime::param time) override;
	} debuggable;

	std::vector<MSXMemoryMapper*> mappers;

	/**
	 * The limit on which bits can be read back as imposed by the engine.
	 * The S1990 engine of the MSX turbo R has such a limit, but other engines
	 * do not.
	 */
	byte engineMask;

	/**
	 * Effective read mask: a combination of engineMask and the limit imposed
	 * by the sizes of the inserted mappers.
	 */
	byte mask;
};
SERIALIZE_CLASS_VERSION(MSXMapperIO, 2);

} // namespace openmsx

#endif
