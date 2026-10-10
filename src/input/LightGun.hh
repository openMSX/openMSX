#ifndef LIGHTGUN_HH
#define LIGHTGUN_HH

#include "JoystickDevice.hh"
#include "LightGunSensor.hh"
#include "MSXEventListener.hh"
#include "StateChangeListener.hh"

#include <cstdint>

namespace openmsx {

class Display;
class MSXEventDistributor;
class MSXMotherBoard;
class StateChangeDistributor;

/** Base class for the light guns, emulated with the host mouse: the mouse
  * pointer is the aim point, the left button the trigger. The guns only
  * differ in the joystick port pins they use. The sensor output is recorded
  * as a state change, so a replay doesn't depend on the rendered frames.
  */
class LightGun : public JoystickDevice, private MSXEventListener
               , private StateChangeListener
{
public:
	struct Pins {
		uint8_t light;   // joystick port bit of the light sensor
		uint8_t trigger; // joystick port bit of the trigger, low while pulled
		bool lightHigh;  // the light bit is high while the sensor sees light
	};

	LightGun(MSXMotherBoard& motherBoard,
	         MSXEventDistributor& eventDistributor,
	         StateChangeDistributor& stateChangeDistributor,
	         Display& display, uint8_t id, Pins pins);
	~LightGun() override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	[[nodiscard]] uint8_t withLight(uint8_t s, bool light) const;
	[[nodiscard]] uint8_t withTrigger(uint8_t s, bool pulled) const;

	// Pluggable
	void plugHelper(Connector& connector, EmuTime time) override;
	void unplugHelper(EmuTime time) override;

	// JoystickDevice
	[[nodiscard]] uint8_t read(EmuTime time) override;
	void write(uint8_t value, EmuTime time) override;

	// MSXEventListener
	void signalMSXEvent(const Event& event,
	                    EmuTime time) noexcept override;
	// StateChangeListener
	void signalStateChange(const StateChange& event) override;
	void stopReplay(EmuTime time) noexcept override;

private:
	MSXEventDistributor& eventDistributor;
	StateChangeDistributor& stateChangeDistributor;
	Display& display;
	LightGunSensor sensor;
	const uint8_t id; // distinguishes the guns in the state changes
	const Pins pins;

	bool hostTrigger = false; // host state
	uint8_t status; // msx state (different from host state during replay):
	                // the joystick port bits
};

} // namespace openmsx

#endif
