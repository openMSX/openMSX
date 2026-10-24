#ifndef TERMINATORLASER_HH
#define TERMINATORLASER_HH

#include "LightGun.hh"

namespace openmsx {

/** ASCII Plus-X Terminator Laser light gun, emulated with the host mouse:
  * the mouse pointer is the aim point, the left button the trigger.
  *
  *  pin 6 (TRG1, bit 4): light sensor, HIGH while the sensor sees light
  *  pin 7 (/TRG2, bit 5): trigger, low while pulled
  *
  * The sensor model is described in GunstickSensor.hh.
  */
class TerminatorLaser final : public LightGun
{
public:
	TerminatorLaser(MSXMotherBoard& motherBoard,
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
