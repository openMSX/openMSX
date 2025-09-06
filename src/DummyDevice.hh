#ifndef DUMMYDEVICE_HH
#define DUMMYDEVICE_HH

#include "MSXDevice.hh"

namespace openmsx {

class DummyDevice final : public MSXDevice
{
public:
	explicit DummyDevice(const DeviceConfig& config);
	void reset(EmuTime time) override;
	void getNameList(TclObject& result) const override;

	byte readMem(uint16_t address, EmuTime time) override;
	byte peekMem(uint16_t address, EmuTime time) const override;
	void writeMem(uint16_t address, byte value, EmuTime time) override;
	const byte* getReadCacheLine(uint16_t start) const override;
	byte* getWriteCacheLine(uint16_t start) override;
};

} // namespace openmsx

#endif
