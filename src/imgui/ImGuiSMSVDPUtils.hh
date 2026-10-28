#ifndef IMGUI_SMS_VDP_UTILS_HH
#define IMGUI_SMS_VDP_UTILS_HH

// Helpers shared between the SMS VDP viewers.

#include <array>
#include <cstdint>

namespace openmsx {

// The 8 pens (0-15) of one row of a mode 4 pattern, from its 4 bit planes.
[[nodiscard]] inline std::array<uint8_t, 8> mode4Pens(uint8_t plane0, uint8_t plane1, uint8_t plane2, uint8_t plane3)
{
	std::array<uint8_t, 8> pens;
	for (int x = 0; x < 8; ++x) {
		pens[x] = uint8_t((((plane3 >> (7 - x)) & 1) << 3) |
		                  (((plane2 >> (7 - x)) & 1) << 2) |
		                  (((plane1 >> (7 - x)) & 1) << 1) |
		                   ((plane0 >> (7 - x)) & 1));
	}
	return pens;
}

} // namespace openmsx

#endif
