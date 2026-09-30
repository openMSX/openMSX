#ifndef MAKOTOMIX_HH
#define MAKOTOMIX_HH

#include <array>
#include <cstdint>
#include <span>

namespace openmsx {

// Convert YMFM's pre-DAC voice taps into contributions to its clamped stereo
// output. At MAX fidelity each SSG sample is repeated, not averaged.
struct MakotoMix {
	std::array<float, 32> voices = {};

	void update(std::span<const int32_t, 32> channelOutput,
	            std::span<const int32_t, 3> ssg, std::span<const int32_t, 3> mixed, float ssgGain)
	{
		std::array<int, 3> scaledSSG = {ssg[0] * 2 / 3, ssg[1] * 2 / 3, ssg[2] * 2 / 3};
		// YMFM rounds the SSG sum after multiplying by 2/3. Assign the
		// rounding remainder to an active voice so a silent voice stays silent.
		int remainder = mixed[2] - scaledSSG[0] - scaledSSG[1] - scaledSSG[2];
		scaledSSG[ssg[2] ? 2 : ssg[1] ? 1 : 0] += remainder;
		for (unsigned side = 0; side < 2; ++side) {
			int total = 0;
			for (unsigned c = 0; c < 16; ++c) {
				if (c < 6 || c >= 9) {
					auto value = channelOutput[2 * c + side];
					total += value;
					voices[2 * c + side] = float(value);
				}
			}
			// FM, rhythm and ADPCM-B share one DAC clamp in YMFM.
			// Only clipped output needs proportional attenuation of the taps.
			if (total && total != mixed[side]) {
				float dacScale = float(mixed[side]) / float(total);
				for (unsigned c = 0; c < 16; ++c) {
					if (c < 6 || c >= 9) voices[2 * c + side] *= dacScale;
				}
			}
			for (unsigned c = 0; c < 3; ++c) {
				voices[2 * (c + 6) + side] = float(scaledSSG[c]) * ssgGain;
			}
		}
	}
};

} // namespace openmsx
#endif
