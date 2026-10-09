#ifndef TERMINATORLASER_HH
#define TERMINATORLASER_HH

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

/** ASCII Plus-X Terminator Laser light gun, emulated with the host mouse:
  * the mouse pointer is the aim point, the left button the trigger.
  *
  *  pin 6 (TRG1, bit 4): light sensor, HIGH while the sensor sees light
  *  pin 7 (/TRG2, bit 5): trigger, low while pulled
  *
  * The sensor model is described in GunstickSensor.hh.
  */
class TerminatorLaser final : public JoystickDevice, private MSXEventListener
                            , private StateChangeListener
{
public:
	TerminatorLaser(MSXMotherBoard& motherBoard,
	                MSXEventDistributor& eventDistributor,
	                StateChangeDistributor& stateChangeDistributor,
	                Display& display);
	~TerminatorLaser() override;

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
	uint8_t status = 0x2F; // msx state (different from host state during
	                       // replay): the LIGHT and TRIGGER bits
};

} // namespace openmsx

#endif
