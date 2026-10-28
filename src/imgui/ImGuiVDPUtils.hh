#ifndef IMGUI_VDP_UTILS_HH
#define IMGUI_VDP_UTILS_HH

// Helpers shared between the viewers of the different VDPs (MSX VDP, SMS VDP).

#include "function_ref.hh"

#include <cstdint>
#include <span>

namespace openmsx {

// One row of a 1-bit-per-pixel pattern, the most significant bit is the
// leftmost pixel.
inline void drawPatternRow8(uint8_t pattern, uint32_t fgCol, uint32_t bgCol, std::span<uint32_t, 8> out)
{
	out[0] = (pattern & 0x80) ? fgCol : bgCol;
	out[1] = (pattern & 0x40) ? fgCol : bgCol;
	out[2] = (pattern & 0x20) ? fgCol : bgCol;
	out[3] = (pattern & 0x10) ? fgCol : bgCol;
	out[4] = (pattern & 0x08) ? fgCol : bgCol;
	out[5] = (pattern & 0x04) ? fgCol : bgCol;
	out[6] = (pattern & 0x02) ? fgCol : bgCol;
	out[7] = (pattern & 0x01) ? fgCol : bgCol;
}

// Same for the 6-pixel wide characters of the text modes (bits 7-2).
inline void drawPatternRow6(uint8_t pattern, uint32_t fgCol, uint32_t bgCol, std::span<uint32_t, 6> out)
{
	out[0] = (pattern & 0x80) ? fgCol : bgCol;
	out[1] = (pattern & 0x40) ? fgCol : bgCol;
	out[2] = (pattern & 0x20) ? fgCol : bgCol;
	out[3] = (pattern & 0x10) ? fgCol : bgCol;
	out[4] = (pattern & 0x08) ? fgCol : bgCol;
	out[5] = (pattern & 0x04) ? fgCol : bgCol;
}

// Render the patterns of the TMS9918-style character modes into an image (for
// the tile viewers), 32 patterns per row. 'pat(i)' returns byte 'i' of the
// pattern table, 'col(i)' byte 'i' of the color table.
// - text: 256 patterns of 6x8 pixels in 'fg' and 'bg' (192x64 pixels)
// - graphics I: 256 patterns of 8x8 pixels, one color byte per 8 patterns
//   (256x64 pixels)
// - graphics II: 'banks' times 256 patterns of 8x8 pixels, one color byte per
//   pattern row (256 x 64*banks pixels)
// - multicolor: the 4 colors of each pattern as 2x2 pixels, for each of the 4
//   pairs of pattern rows (64x64 pixels)
void renderTextPatterns(function_ref<uint8_t(unsigned)> pat, uint32_t fg, uint32_t bg,
                        std::span<uint32_t> output);
void renderGraphic1Patterns(function_ref<uint8_t(unsigned)> pat, function_ref<uint8_t(unsigned)> col,
                            std::span<const uint32_t, 16> palette, std::span<uint32_t> output);
void renderGraphic2Patterns(function_ref<uint8_t(unsigned)> pat, function_ref<uint8_t(unsigned)> col,
                            std::span<const uint32_t, 16> palette, int banks, std::span<uint32_t> output);
void renderMulticolorPatterns(function_ref<uint8_t(unsigned)> pat,
                              std::span<const uint32_t, 16> palette, std::span<uint32_t> output);

} // namespace openmsx

#endif
