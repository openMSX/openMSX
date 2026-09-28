#ifndef MAKOTOMIX_HH
#define MAKOTOMIX_HH

#include <array>
#include <cstdint>

namespace openmsx {

// Convert YMFM's pre-DAC voice taps into contributions to its clamped stereo
// output. At MAX fidelity each SSG sample is repeated, not averaged.
struct MakotoMix {
	std::array<float, 32> voices = {};

	void update(const std::array<int32_t, 32>& channelOutput,
	            const int32_t* ssg, const int32_t* mixed, float ssgGain)
	{
		std::array<int, 3> scaledSSG = {ssg[0] * 2 / 3, ssg[1] * 2 / 3, ssg[2] * 2 / 3};
		// YMFM rounds the SSG sum after multiplying by 2/3. Assign the
		// rounding remainder to an active voice so a silent voice stays silent.
		int remainder = mixed[2] - scaledSSG[0] - scaledSSG[1] - scaledSSG[2];
		scaledSSG[ssg[2] ? 2 : ssg[1] ? 1 : 0] += remainder;
		for (unsigned side = 0; side < 2; ++side) {
			int total = 0;
			for (unsigned c = 0; c < 16; ++c) {
				if (c < 6 || c >= 9) total += channelOutput[2 * c + side];
			}
			// FM, rhythm and ADPCM-B share one DAC clamp in YMFM. Its
			// pre-clamp taps cannot be independently clamped; distribute
			// the aggregate attenuation proportionally across these taps.
			float dacScale = total ? float(mixed[side]) / float(total) : 1.0f;
			for (unsigned c = 0; c < 16; ++c) {
				float value = (c >= 6 && c < 9)
					? float(scaledSSG[c - 6]) * ssgGain
					: float(channelOutput[2 * c + side]) * dacScale;
				voices[2 * c + side] = value;
			}
		}
	}
};

} // namespace openmsx
#endif
