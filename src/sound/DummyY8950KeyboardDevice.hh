#ifndef DUMMYY8950KEYBOARDDEVICE_HH
#define DUMMYY8950KEYBOARDDEVICE_HH

#include "Y8950KeyboardDevice.hh"

namespace openmsx {

class DummyY8950KeyboardDevice final : public Y8950KeyboardDevice
{
public:
	void write(uint8_t data, EmuTime time) override;
	[[nodiscard]] uint8_t read(EmuTime time) override;

	[[nodiscard]] zstring_view getDescription() const override;
	void plugHelper(Connector& connector, EmuTime time) override;
	void unplugHelper(EmuTime time) override;
};

} // namespace openmsx

#endif
