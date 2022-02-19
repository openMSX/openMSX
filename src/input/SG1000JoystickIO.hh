#ifndef SG1000JOYSTICKIO_HH
#define SG1000JOYSTICKIO_HH

#include "MSXDevice.hh"

namespace openmsx {

class JoystickPortIf;

/** I/O port access to the joysticks for the Sega SG-1000.
  */
class SG1000JoystickIO final : public MSXDevice
{
public:
	explicit SG1000JoystickIO(const DeviceConfig& config);
	~SG1000JoystickIO();

	byte readIO(word port, EmuTime::param time) override;
	byte peekIO(word port, EmuTime::param time) const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	JoystickPortIf* ports[2];
};

} // namespace openmsx

#endif
