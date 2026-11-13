#ifndef Y8960PAN_HH
#define Y8960PAN_HH

#include <cmath>
#include <cstdint>
#include <numbers>

namespace openmsx::y8960 {

struct PanGain { float left, right; };

/** The pan law of the Y8960's pan registers (OPLL 50h-58h, SSGS 28h-2Eh,
  * OPL2 F6h-FFh, SCC 9900h-9904h): 1 is hard left, 128 the centre, 255 hard
  * right, and 0 is hard left like 1.
  *
  * The specification only gives 1, 128 and 255; 0 is taken as the RTL's pan
  * table takes it. Between them this is power panning, L = cos(t),
  * R = sin(t) with t running from 0 to pi/2, so the centre sits 3dB down on
  * both sides.
  */
[[nodiscard]] inline PanGain panGain(uint8_t pan)
{
	if (pan == 128) {
		// one constant for both sides, so that a centred channel is
		// exactly symmetric rather than cos and sin a rounding apart
		constexpr float c = std::numbers::sqrt2_v<float> / 2.0f;
		return {.left = c, .right = c};
	}
	if (pan == 0) pan = 1;
	float t = float(pan - 1) / 254.0f * (std::numbers::pi_v<float> / 2.0f);
	return {.left = std::cos(t), .right = std::sin(t)};
}

/** The gain of a volume register (SSGS 20h-26h, the mixer's 7FF7h-7FFAh
  * and 7FFEh): 0 mutes, 1-127 is linear in steps of 1/128, and any value
  * with bit 7 set is unity. So 128, the documented maximum, is unity and 127
  * just below it. The specification gives only the end points. */
[[nodiscard]] inline float volumeGain(uint8_t volume)
{
	if (volume & 0x80) return 1.0f;
	return float(volume) / 128.0f;
}

} // namespace openmsx::y8960

#endif
