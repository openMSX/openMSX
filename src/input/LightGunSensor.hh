#ifndef LIGHTGUNSENSOR_HH
#define LIGHTGUNSENSOR_HH

#include "EmuTime.hh"

#include "gl_vec.hh"

#include <optional>

namespace openmsx {

class Display;
class MSXMotherBoard;
class VDP;

/** What the light guns have in common: the light sensor is aimed at the host
  * mouse pointer and looks at the frame the VDP is drawing. The sensor model
  * itself is described in GunstickSensor.hh.
  */
class LightGunSensor
{
public:
	LightGunSensor(MSXMotherBoard& motherBoard, Display& display);

	/** Does the sensor see light at the given time? */
	[[nodiscard]] bool senseLight(EmuTime time);

private:
	[[nodiscard]] std::optional<gl::ivec2> getAim() const;

private:
	MSXMotherBoard& motherBoard;
	Display& display;
	// Pluggables can be created before the machine's devices, so the VDP
	// is looked up on first use.
	VDP* vdp = nullptr;
};

} // namespace openmsx

#endif
