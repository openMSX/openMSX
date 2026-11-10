#ifndef GUNSTICK_HH
#define GUNSTICK_HH

#include "LightGun.hh"

namespace openmsx {

/** MHT Gunstick light gun, emulated with the host mouse: the mouse pointer
  * is the aim point, the left button the trigger.
  *
  *  pin 2 (/BACK, bit 1): light sensor, low while the sensor sees light
  *  pin 6 (/TRG1, bit 4): trigger, low while pulled
  *
  * The sensor model is described in GunstickSensor.hh.
  */
class Gunstick final : public LightGun
{
public:
	Gunstick(MSXMotherBoard& motherBoard,
	         MSXEventDistributor& eventDistributor,
	         StateChangeDistributor& stateChangeDistributor,
	         Display& display);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	// Pluggable
	[[nodiscard]] zstring_view getName() const override;
	[[nodiscard]] zstring_view getDescription() const override;
};

} // namespace openmsx

#endif
