#ifndef SG1000JOYSTICKIO_HH
#define SG1000JOYSTICKIO_HH

#include "MSXDevice.hh"
#include <array>

namespace openmsx {

class JoystickPortIf;

/** I/O port access to the joysticks for the Sega SG-1000.
  */
class SG1000JoystickIO final : public MSXDevice
{
public:
	explicit SG1000JoystickIO(const DeviceConfig& config);

	[[nodiscard]] byte readIO(uint16_t port, EmuTime::param time) override;
	[[nodiscard]] byte peekIO(uint16_t port, EmuTime::param time) const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	std::array<JoystickPortIf*, 2> ports;
};

} // namespace openmsx

#endif
