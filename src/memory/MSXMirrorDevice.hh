#ifndef MSXMIRRORDEVICE_HH
#define MSXMIRRORDEVICE_HH

#include "MSXDevice.hh"

namespace openmsx {

class MSXCPUInterface;

class MSXMirrorDevice final : public MSXDevice
{
public:
	explicit MSXMirrorDevice(const DeviceConfig& config);

	[[nodiscard]] byte readMem(uint16_t address, EmuTime::param time) override;
	void writeMem(uint16_t address, byte value, EmuTime::param time) override;
	[[nodiscard]] const byte* getReadCacheLine(uint16_t start) const override;
	[[nodiscard]] byte* getWriteCacheLine(uint16_t start) override;
	[[nodiscard]] byte peekMem(uint16_t address, EmuTime::param time) const override;
	[[nodiscard]] bool allowUnaligned() const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	MSXCPUInterface& interface;
	const unsigned addressHigh;
};

} // namespace openmsx

#endif
