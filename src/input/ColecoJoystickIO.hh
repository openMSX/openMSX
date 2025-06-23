#ifndef COLECOJOYSTICKIO_HH
#define COLECOJOYSTICKIO_HH

#include "MSXDevice.hh"
#include "Keyboard.hh"
#include <array>

namespace openmsx {

class JoystickPortIf;

class ColecoJoystickIO final : public MSXDevice
{
public:
	explicit ColecoJoystickIO(const DeviceConfig& config);

	// MSXDevice:
	void reset(EmuTime::param time) override;
	[[nodiscard]] byte readIO(uint16_t port, EmuTime::param time) override;
	[[nodiscard]] byte peekIO(uint16_t port, EmuTime::param time) const override;
	void writeIO(uint16_t port, byte value, EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	byte joyMode;
	std::array<JoystickPortIf*, 2> ports;
	Keyboard keyboard;
};
SERIALIZE_CLASS_VERSION(ColecoJoystickIO, 2);

} // namespace openmsx

#endif
