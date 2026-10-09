#ifndef GUNSTICK_HH
#define GUNSTICK_HH

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

/** MHT Gunstick light gun, emulated with the host mouse: the mouse pointer
  * is the aim point, the left button the trigger.
  *
  *  pin 2 (/BACK, bit 1): light sensor, low while the sensor sees light
  *  pin 6 (/TRG1, bit 4): trigger, low while pulled
  *
  * The sensor model is described in GunstickSensor.hh.
  */
class Gunstick final : public JoystickDevice, private MSXEventListener
                     , private StateChangeListener
{
public:
	Gunstick(MSXMotherBoard& motherBoard,
	         MSXEventDistributor& eventDistributor,
	         StateChangeDistributor& stateChangeDistributor,
	         Display& display);
	~Gunstick() override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	// Pluggable
	[[nodiscard]] zstring_view getName() const override;
	[[nodiscard]] zstring_view getDescription() const override;
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

	bool hostTrigger = false; // host state
	uint8_t status = 0x3F; // msx state (different from host state during
	                       // replay): the LIGHT and TRIGGER bits, 0 = active
};

} // namespace openmsx

#endif
