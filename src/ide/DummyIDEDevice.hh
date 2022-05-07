#ifndef DUMMYIDEDEVICE_HH
#define DUMMYIDEDEVICE_HH

#include "IDEDevice.hh"
#include "serialize_meta.hh"

namespace openmsx {

class DummyIDEDevice final : public IDEDevice
{
public:
	void reset(EmuTime::param time) override;
	[[nodiscard]] word readData(EmuTime::param time) override;
	[[nodiscard]] byte readReg(nibble reg, EmuTime::param time) override;
	void writeData(word value, EmuTime::param time) override;
	void writeReg(nibble reg, byte value, EmuTime::param time) override;

	void serialize(Archive auto& ar, unsigned version);
};

} // namespace openmsx

#endif
