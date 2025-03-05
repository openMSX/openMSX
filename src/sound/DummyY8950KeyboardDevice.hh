#ifndef DUMMYY8950KEYBOARDDEVICE_HH
#define DUMMYY8950KEYBOARDDEVICE_HH

#include "Y8950KeyboardDevice.hh"

namespace openmsx {

class DummyY8950KeyboardDevice final : public Y8950KeyboardDevice
{
public:
	void write(uint8_t data, EmuTime::param time) override;
	[[nodiscard]] uint8_t read(EmuTime::param time) override;

	[[nodiscard]] std::string_view getDescription() const override;
	void plugHelper(Connector& connector, EmuTime::param time) override;
	void unplugHelper(EmuTime::param time) override;
};

} // namespace openmsx

#endif
