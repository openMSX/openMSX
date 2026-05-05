#ifndef PLOTTER_FONT_HH
#define PLOTTER_FONT_HH

#include "gl_vec.hh"

#include <cstdint>
#include <span>
#include <vector>

namespace openmsx {

enum class PlotterFontVariant : uint8_t {
	International,
	DIN,
};

enum class PlotterFontCommandType : uint8_t {
	Move,
	Draw,
};

struct PlotterFontCommand
{
	PlotterFontCommandType type;
	gl::vec2 delta;
	uint8_t raw;
};

[[nodiscard]] std::span<const uint8_t> getPRNC41Glyph(
	PlotterFontVariant variant, uint8_t code);

[[nodiscard]] std::vector<PlotterFontCommand> decodePRNC41Glyph(
	PlotterFontVariant variant, uint8_t code);

} // namespace openmsx

#endif
