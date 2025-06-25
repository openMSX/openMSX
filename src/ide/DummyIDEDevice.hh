#ifndef DUMMYIDEDEVICE_HH
#define DUMMYIDEDEVICE_HH

#include "IDEDevice.hh"

namespace openmsx {

class DummyIDEDevice final : public IDEDevice
{
public:
	void reset(EmuTime time) override;
	[[nodiscard]] uint16_t readData(EmuTime time) override;
	[[nodiscard]] uint8_t readReg(uint4_t reg, EmuTime time) override;
	void writeData(uint16_t value, EmuTime time) override;
	void writeReg(uint4_t reg, uint8_t value, EmuTime time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);
};

} // namespace openmsx

#endif
