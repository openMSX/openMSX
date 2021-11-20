#ifndef COLECOJOYSTICKIO_HH
#define COLECOJOYSTICKIO_HH

#include "MSXDevice.hh"
#include "Keyboard.hh"


namespace openmsx {

class JoystickPortIf;

class ColecoJoystickIO final : public MSXDevice
{
public:
	explicit ColecoJoystickIO(const DeviceConfig& config);

	// MSXDevice:
	void reset(EmuTime::param time) override;
	[[nodiscard]] byte readIO(word port, EmuTime::param time) override;
	[[nodiscard]] byte peekIO(word port, EmuTime::param time) const override;
	void writeIO(word port, byte value, EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	byte joyMode;
	JoystickPortIf* ports[2];
	Keyboard keyboard;
};
SERIALIZE_CLASS_VERSION(ColecoJoystickIO, 2);

} // namespace openmsx

#endif
