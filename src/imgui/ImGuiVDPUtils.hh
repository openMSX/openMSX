#ifndef IMGUI_VDP_UTILS_HH
#define IMGUI_VDP_UTILS_HH

// Helpers shared between the viewers of the different VDPs (MSX VDP, SMS VDP, V9990).

#include "function_ref.hh"
#include "gl_vec.hh"

#include <cstdint>
#include <initializer_list>
#include <span>
#include <string>
#include <string_view>
#include <utility>

struct ImDrawList;
struct ImFont;
namespace gl { class Texture; }

namespace openmsx {

// The 'VDP settings' / 'Manual override' panel of the VDP viewers. Each row
// shows a setting as the VDP uses it, a checkbox to override it, and an
// editor for the manual value (only enabled while overridden).
// 'vdpValue' and 'editor' only refer to their content, so list the settings
// in the overridableSettings() call itself (e.g. with tmpStrCat() values).
struct OverridableSetting {
	std::string_view vdpValue; // e.g. "Screen mode: screen 1", grayed out while overridden
	bool* overridden; // nullptr: an informational row, without checkbox or editor
	function_ref<void()> editor;
	bool vdpValueUsed = true; // false: also gray out 'vdpValue' (e.g. it doesn't apply to the VDP's mode)
};
// 'title' heads the column with the VDP's values, which is at least as wide
// as 'widestValue'. 'overrideAll' overrides all settings at once. 'help', when
// not empty, is a help marker next to the "Manual override" checkbox.
void overridableSettings(std::string_view title, std::string_view widestValue, bool& overrideAll,
                         std::initializer_list<OverridableSetting> settings,
                         std::string_view help = {});

// The zoom combo box (1x-8x, stored as 0-7) and the grid checkbox and color.
void zoomAndGridSettings(int& zoom, bool& grid, gl::vec4& gridColor);

// The raster beam checkbox and color, the beam position 'pos' (in 'font'),
// and a help marker with 'help'.
void rasterBeamSettings(bool& rasterBeam, gl::vec4& color, gl::ivec2 pos, ImFont* font, std::string_view help);

// Mark the raster beam at VDP pixel 'pos' on a view of the screen drawn at
// 'scrnPos', with 'zoom' host pixels per VDP pixel.
void addRasterBeam(ImDrawList* drawList, gl::vec2 scrnPos, gl::vec2 zoom, gl::vec2 pos, gl::vec4 color);

// VRAM data as the VDP viewers copy it to the clipboard: the 'count' bytes
// read(address), read(address + 1), ... in hex, one per line.
[[nodiscard]] std::string formatVramData(function_ref<uint8_t(unsigned)> read, unsigned address, unsigned count);

// The checkerboard background of the sprite viewers.
inline constexpr int maxCheckerBoardSize = 255;
// Its settings: both colors and the size of the squares (0 = none);
// 'toolTip' explains where it's used.
void checkerBoardSettings(std::string_view toolTip, gl::vec4& color1, gl::vec4& color2, int& size);

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

// Small hex digit font, e.g. to overlay tile numbers on a name table: a
// texture with the 16 glyphs 0-9 A-F next to each other.
inline constexpr gl::vec2 hexDigitSize{5.0f, 8.0f};
[[nodiscard]] gl::Texture createHexDigitsTexture();

// Add the lowest 'numDigits' hex digits of 'value' (most significant first),
// each digit 'step' further than the previous one. The caller must have
// pushed the hex digits texture and reserved 6 indices and 4 vertices per
// digit (ImDrawList::PrimReserve()).
void addHexDigits(ImDrawList* drawList, gl::vec2 pos, gl::vec2 step, unsigned value, int numDigits);

// Draw a grid of 'numCells' cells (e.g. the characters of a name table) of
// 'cellSize' pixels each, starting at 'pos'. 'uv(column, row)' returns the part
// of 'tex' to draw in that cell.
void addTiles(ImDrawList* drawList, const gl::Texture& tex, gl::vec2 pos, gl::vec2 cellSize, gl::ivec2 numCells,
              function_ref<std::pair<gl::vec2, gl::vec2>(int column, int row)> uv);
// Overlay a hex number on each cell of such a grid: 'number(column, row)' as
// 'numDigits' digits from the 'hexDigits' texture.
void addHexNumbers(ImDrawList* drawList, const gl::Texture& hexDigits, gl::vec2 pos, gl::vec2 cellSize,
                   gl::ivec2 numCells, gl::vec2 step, int numDigits, function_ref<unsigned(int column, int row)> number);

// The context menu of the tile viewers' pattern table: copy the data of a
// pattern, and when 'hasColorData' also its color data, to the clipboard.
bool copyPatternDataPopup(bool hasColorData, function_ref<std::string()> patternData,
                          function_ref<std::string()> colorData);

// Lines for the tile viewer tooltips: "Pattern: <nr>" (also in hex), and
// "<name> address:" followed by the address (range) in 'digits' hex digits.
void printPatternNr(unsigned pattern);
void printAddress(std::string_view name, unsigned address, int digits);
void printAddressRange(std::string_view name, unsigned address, unsigned count, int digits);

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

// The bounding box of a sprite on the 'Rendered sprites' screen (in VDP
// pixels, clipped to that screen), with the sprite's attributes.
struct SpriteBox {
	int x, y, w, h; // box
	int sprite;
	uint8_t vramX, vramY;
	unsigned pattern;

	[[nodiscard]] bool contains(gl::ivec2 p) const {
		return (x <= p.x) && (p.x < (x + w)) &&
		       (y <= p.y) && (p.y < (y + h));
	}
};

// Clip one side of a sprite box, [pos, pos + size), to [0, limit).
// Returns false when nothing of it remains.
[[nodiscard]] bool clipSpriteBoxSide(int& pos, int& size, int limit);

// The "Bounding box" checkbox, its color, and whether to draw it on the
// hovered or on all sprites.
void spriteBoxSettings(bool& draw, gl::vec4& color, int& onAll);

// Outline the 'boxes' on the screen at 'scrnPos', scaled per axis by 'zoom': all
// of them when 'onAll' is set, otherwise those under the mouse ('hoverPos',
// only when 'hovered').
void drawSpriteBoxes(ImDrawList* drawList, gl::vec2 scrnPos, gl::vec2 zoom, std::span<const SpriteBox> boxes,
                     gl::vec4 color, bool onAll, bool hovered, gl::ivec2 hoverPos);

// A bullet with the attributes of each sprite whose box contains 'pos'. The
// pattern number is also shown as 'patternDigits' hex digits.
void printSpriteBoxes(std::span<const SpriteBox> boxes, gl::ivec2 pos, int patternDigits);

// The number of sprites on each line of the 'Rendered sprites' screen ('counts',
// one per line), as a vertical histogram at the cursor position, each line
// 'zoom' pixels tall, so that it lines up with that screen. Lines with more
// than 'limit' sprites are drawn in another color; hovering shows the count.
// The next item comes on the same line. Draws nothing when 'show' is false.
void spritesPerLineHistogram(bool show, std::span<const uint8_t> counts, int limit, float zoom);

// The "Sprites per line" checkbox, and when checked a summary of 'counts': the
// busiest line and the number of lines over 'limit'. 'extraHelp' is added to
// the help marker (e.g. which sprites are counted).
void spritesPerLineSettings(bool& show, std::span<const uint8_t> counts, int limit,
                            std::string_view extraHelp = {});

} // namespace openmsx

#endif
