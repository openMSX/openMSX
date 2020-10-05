#ifndef MSXMIRRORDEVICE_HH
#define MSXMIRRORDEVICE_HH

#include "MSXDevice.hh"

namespace openmsx {

class MSXCPUInterface;

class MSXMirrorDevice final : public MSXDevice
{
public:
	explicit MSXMirrorDevice(const DeviceConfig& config);

	byte readMem(word address, EmuTime::param time) override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	const byte* getReadCacheLine(word start) const override;
	byte* getWriteCacheLine(word start) const override;
	byte peekMem(word address, EmuTime::param time) const override;
	bool allowUnaligned() const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	MSXCPUInterface& interface;
	const unsigned addressHigh;
};

} // namespace openmsx

#endif
