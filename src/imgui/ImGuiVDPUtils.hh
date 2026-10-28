#ifndef IMGUI_VDP_UTILS_HH
#define IMGUI_VDP_UTILS_HH

// Helpers shared between the viewers of the different VDPs (MSX VDP, SMS VDP).

#include "function_ref.hh"
#include "gl_vec.hh"

#include <cstdint>
#include <span>
#include <utility>

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

// Combo box items: the color numbers 0-15, and the same followed by "none"
// (for the color 0 replacement).
inline constexpr const char* colorItems = "0\0001\0002\0003\0004\0005\0006\0007\0008\0009\00010\00011\00012\00013\00014\00015\000";
inline constexpr const char* color0Items = "0\0001\0002\0003\0004\0005\0006\0007\0008\0009\00010\00011\00012\00013\00014\00015\000none\000";

// The "fg" and "bg" combo boxes to edit a color byte (TMS9918-style:
// foreground in the upper, background in the lower 4 bits).
void comboFgBgColor(int& colorValue);

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

// Render the monochrome (TMS9918-style) sprite patterns into a 256x64 image
// (the color comes from the sprite attributes, drawn by tinting this image):
// either all 256 patterns of 8x8 or all 64 sprites of 16x16 (each composed
// of the patterns 4s, 4s+1, 4s+2, 4s+3). 'pat(i)' returns byte 'i' (0-0x7ff)
// of the sprite pattern table.
void renderSpritePatterns8 (function_ref<uint8_t(unsigned)> pat, std::span<uint32_t> output);
void renderSpritePatterns16(function_ref<uint8_t(unsigned)> pat, std::span<uint32_t> output);
// The layout of that image: the number of cells (32x8 patterns or 16x4
// sprites), the pattern number shown in 'cell' (for 16x16 the first of the 4),
// and the part of the image that shows 'pattern' (for 16x16 the sprite of
// the 4 patterns starting at 'pattern & ~3').
[[nodiscard]] inline gl::vec2 spritePatternCells(int size)
{
	return (size == 16) ? gl::vec2{16.0f, 4.0f} : gl::vec2{32.0f, 8.0f};
}
[[nodiscard]] inline unsigned spritePatternAt(gl::ivec2 cell, int size)
{
	return (size == 16) ? 4 * (16 * cell.y + cell.x) : (32 * cell.y + cell.x);
}
[[nodiscard]] std::pair<gl::vec2, gl::vec2> spritePatternUV(unsigned pattern, int size);

} // namespace openmsx

#endif
