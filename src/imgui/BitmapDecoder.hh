#ifndef BITMAP_DECODER_HH
#define BITMAP_DECODER_HH

// Decode a byte stream into an RGBA image, given a valid layout.
// No dependency on ImGui or OpenGL.

#include "gl_vec.hh"
#include "static_vector.hh"
#include "unreachable.hh"

#include <cstdint>
#include <span>

namespace openmsx::BitmapDecoder {

// Level 0 is the pixel level, levels 1 and 2 are the tiling levels.
inline constexpr size_t MAX_LEVELS = 3;

// The encoding of one item: its size in bits and how its value maps to a color.
// In the packed formats the channels are listed from the least to the most
// significant bit of the value.
enum class PixelFormat : uint8_t {
	GRAY1, GRAY2, GRAY4, GRAY8,
	GRAY16, GRAY24, GRAY32, // only the 8 most significant bits are used
	PALETTE2, PALETTE4,     // index in the 16-entry palette
	BRG233,
	BRGX5551, BRGYS5551,    // bit 15: ignored / set means transparent
	BGR565, RGB565,
	RGB888,
	BGRA8888, RGBA8888,
};

[[nodiscard]] constexpr unsigned bitsPerItem(PixelFormat format)
{
	switch (format) {
	using enum PixelFormat;
	case GRAY1:
		return 1;
	case GRAY2: case PALETTE2:
		return 2;
	case GRAY4: case PALETTE4:
		return 4;
	case GRAY8: case BRG233:
		return 8;
	case GRAY16: case BRGX5551: case BRGYS5551: case BGR565: case RGB565:
		return 16;
	case GRAY24: case RGB888:
		return 24;
	case GRAY32: case BGRA8888: case RGBA8888:
		return 32;
	}
	UNREACHABLE;
}

// For items of less than 8 bits: the order of the items within a byte.
// For items of more than 8 bits: the order of the bytes within an item
// (MSB_FIRST is big endian).
enum class Order : uint8_t { MSB_FIRST, LSB_FIRST };

// Counts and steps are at least 1 (also a step that doesn't matter because its
// count is 1). All steps in items (of bitsPerItem() bits), so every item is
// aligned: smaller than a byte it never crosses a byte boundary, otherwise it
// starts at one.
struct Level {
	unsigned countX = 1;
	unsigned countY = 1;
	uint32_t stepX = 1;
	uint32_t stepY = 1;

	[[nodiscard]] bool operator==(const Level&) const = default;
};

// Has at least one level. dataSize() * 8 fits in 32 bits, so all bit positions
// in the image fit in 32 bits, and the image spans less than 512MB.
struct Layout {
	PixelFormat format = PixelFormat::BRG233;
	Order order = Order::MSB_FIRST;
	static_vector<Level, MAX_LEVELS> levels; // innermost (pixel level) first

	// Number of data bytes this layout requires.
	[[nodiscard]] uint32_t dataSize() const;

	// Width and height of the decoded image, in pixels.
	[[nodiscard]] gl::ivec2 imageSize() const;
};

// Requires a layout as accepted by resolve() in BitmapDecoderSettings.hh.
// 'palette' is only used for the PALETTE formats.
// 'data' starts at the first byte of the image and may be shorter than
// layout.dataSize(): items that are not completely inside 'data' become
// transparent.
// 'output' must have exactly imageSize().x * imageSize().y entries; all of them
// are written, so it doesn't need to be initialized.
// Colors are RGBA packed in a uint32_t with red in the lowest byte (the format
// uploaded with GL_RGBA); fully transparent is 0.
void decode(const Layout& layout, std::span<const uint32_t, 16> palette,
            std::span<const uint8_t> data, std::span<uint32_t> output);

} // namespace openmsx::BitmapDecoder

#endif
