#ifndef MSXMAPPERIO_HH
#define MSXMAPPERIO_HH

#include "MSXDevice.hh"
#include "SimpleDebuggable.hh"
#include <vector>

namespace openmsx {

class MSXMapperIO final : public MSXDevice
{
public:
	explicit MSXMapperIO(const DeviceConfig& config);

	void reset(EmuTime::param time) override;
	byte readIO(word port, EmuTime::param time) override;
	byte peekIO(word port, EmuTime::param time) const override;
	void writeIO(word port, byte value, EmuTime::param time) override;

	/**
	 * Every MSXMemoryMapper must (un)register its size.
	 * This is used to influence the result returned in readIO().
	 */
	void registerMapper(unsigned blocks);
	void unregisterMapper(unsigned blocks);

	/**
	 * Returns the actual selected page for the given bank.
	 */
	byte getSelectedPage(byte bank) const {	return registers[bank]; }

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void write(unsigned address, byte value);

	/**
	 * Updates the "mask" field after a mapper was registered or unregistered.
	 */
	void updateMask();

	struct Debuggable final : SimpleDebuggable {
		Debuggable(MSXMotherBoard& motherBoard, const std::string& name);
		byte read(unsigned address) override;
		void write(unsigned address, byte value) override;
	} debuggable;

	std::vector<unsigned> mapperSizes; // sorted
	byte registers[4];

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

} // namespace openmsx

#endif
