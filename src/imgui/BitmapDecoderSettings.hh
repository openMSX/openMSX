#ifndef BITMAP_DECODER_SETTINGS_HH
#define BITMAP_DECODER_SETTINGS_HH

// The user input for a BitmapDecoder::Layout, as typed, and its validation.
// No dependency on ImGui or OpenGL.

#include "BitmapDecoder.hh"

#include "function_ref.hh"

#include <array>
#include <expected>
#include <span>
#include <string>
#include <string_view>

namespace openmsx::BitmapDecoder {

// The color choice as presented to the user, independent of bitsPerItem.
// Together with bitsPerItem it selects a PixelFormat.
enum class ColorMode : uint8_t {
	GRAYSCALE, // shown as 'black & white' for 1 bit per item
	PALETTE,
	BRG233,
	BRGX5551,
	BRGYS5551,
	BGR565,
	RGB565,
	RGB888,
	BGRA8888,
	RGBA8888,
};

[[nodiscard]] std::span<const unsigned> validBitsPerItem(); // 1, 2, 4, 8, 16, 24, 32
[[nodiscard]] std::span<const ColorMode> validColorModes(unsigned bitsPerItem);

// Counts and steps are strings: an empty string means 'auto'. Only the
// countY of the outermost level may be auto. Steps are in items (pixels), on
// all levels.
struct LevelSettings {
	std::string countX;
	std::string countY;
	std::string stepX;
	std::string stepY;
};

// The bit and byte order are kept separately, so that each choice is kept when
// switching bitsPerItem; resolve() picks the one that applies.
struct Settings {
	unsigned bitsPerItem = 8;
	ColorMode colorMode = ColorMode::GRAYSCALE; // one of validColorModes(bitsPerItem)
	Order bitOrder = Order::MSB_FIRST;  // only for bitsPerItem < 8
	Order byteOrder = Order::LSB_FIRST; // only for bitsPerItem > 8 (little endian)
	static_vector<LevelSettings, MAX_LEVELS> levels = {LevelSettings{}}; // innermost first
};

// Maximum width and height of the image, in pixels.
inline constexpr unsigned MAX_IMAGE_SIZE = 2048;

enum class Field : uint8_t { COUNT_X, COUNT_Y, STEP_X, STEP_Y };
inline constexpr size_t NUM_FIELDS = 4;
[[nodiscard]] constexpr size_t fieldIndex(size_t level, Field field)
{
	return level * NUM_FIELDS + size_t(field);
}

// One message per input field, empty when the field is fine; index: fieldIndex().
// Every error is attributed to (at least) one field.
using FieldErrors = std::array<std::string, MAX_LEVELS * NUM_FIELDS>;

// Evaluates the (trimmed, non-empty) text of a count or step field, or returns
// an error message. The viewer uses parseAddressExpr(), so that a field can
// also hold a symbol or an expression such as "16 * 8".
using Evaluator = function_ref<std::expected<unsigned, std::string>(std::string_view)>;

// Evaluates the strings, fills in the auto values and selects the PixelFormat.
// 'availableBytes' is the number of bytes from the start of the data to the
// end of the input; it is only used to compute an auto height. An auto height
// is at least 1 (even when not one row fits) and at most what MAX_IMAGE_SIZE
// allows.
// Requires a valid bitsPerItem and colorMode, and 1 to MAX_LEVELS levels.
[[nodiscard]] std::expected<Layout, FieldErrors> resolve(
	const Settings& settings, unsigned availableBytes, Evaluator evaluate);

} // namespace openmsx::BitmapDecoder

#endif
