#ifndef IMGUI_V9990_UTILS_HH
#define IMGUI_V9990_UTILS_HH

// Helpers shared between the V9990 viewers (ImGuiV9990Tiles,
// ImGuiV9990Sprites and ImGuiV9990Bitmap).

#include "V9990.hh"
#include "V9990ModeEnum.hh"

#include "enumerate.hh"
#include "narrow.hh"

#include <array>
#include <cstdint>

namespace openmsx::v9990_gui {

// Combo string for the 4 sub-palettes selected by the palette offset register (R#13).
inline constexpr const char* paletteStr = "0 (0-15)\0001 (16-31)\0002 (32-47)\0003 (48-63)\000";

[[nodiscard]] inline const char* toString(V9990DisplayMode mode)
{
	static constexpr std::array<const char*, 10> names = {
		"P1", "P2", "B0", "B1", "B2", "B3", "B4", "B5", "B6", "B7"
	};
	return names[size_t(mode)];
}

[[nodiscard]] inline const char* toString(V9990ColorMode mode)
{
	static constexpr std::array<const char*, 10> names = {
		"PP", "BYUV", "BYUVP", "BYJK", "BYJKP", "BD16", "BD8", "BP6", "BP4", "BP2"
	};
	return names[size_t(mode)];
}

// 5-bit per component RGB to 32-bit RGBA (as used by the textures).
[[nodiscard]] inline uint32_t rgb5ToColor(int r, int g, int b)
{
	auto c5to8 = [](int c) { c &= 31; return uint32_t((c << 3) | (c >> 2)); };
	return (c5to8(r) << 0) | (c5to8(g) << 8) | (c5to8(b) << 16) | (255u << 24);
}

// The 64 palette entries of the V9990, converted to RGBA.
[[nodiscard]] inline std::array<uint32_t, 64> getPalette64(const V9990& v9990)
{
	std::array<uint32_t, 64> result;
	for (auto [i, col] : enumerate(result)) {
		auto [r, g, b, ys] = v9990.getPalette(narrow<int>(i));
		col = rgb5ToColor(r, g, b);
	}
	return result;
}

} // namespace openmsx::v9990_gui

#endif
