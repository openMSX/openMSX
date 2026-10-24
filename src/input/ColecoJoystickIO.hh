#ifndef COLECOJOYSTICKIO_HH
#define COLECOJOYSTICKIO_HH

#include "Keyboard.hh"

#include "MSXDevice.hh"

#include <array>

namespace openmsx {

class ColecoJoystick;
class JoystickPortIf;

class ColecoJoystickIO final : public MSXDevice
{
public:
	explicit ColecoJoystickIO(const DeviceConfig& config);

	// MSXDevice:
	void reset(EmuTime time) override;
	[[nodiscard]] byte readIO(uint16_t port, EmuTime time) override;
	[[nodiscard]] byte peekIO(uint16_t port, EmuTime time) const override;
	void writeIO(uint16_t port, byte value, EmuTime time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	/** The ColecoVision controller plugged into the given port, or nullptr. */
	[[nodiscard]] const ColecoJoystick* getColecoJoystick(int joyPort) const;

private:
	byte joyMode;
	std::array<JoystickPortIf*, 2> ports;
	Keyboard keyboard;
};
SERIALIZE_CLASS_VERSION(ColecoJoystickIO, 2);

} // namespace openmsx

#endif
