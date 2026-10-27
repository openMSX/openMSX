#include "catch.hpp"
#include "BitmapDecoder.hh"
#include "one_of.hh"
#include "xrange.hh"

#include <array>
#include <cstdint>
#include <utility>
#include <vector>

using namespace openmsx;
using namespace openmsx::BitmapDecoder;
using enum PixelFormat;

static constexpr uint32_t rgba(uint32_t r, uint32_t g, uint32_t b, uint32_t a = 255)
{
	return (r << 0) | (g << 8) | (b << 16) | (a << 24);
}
static constexpr uint32_t gray(uint32_t v) { return rgba(v, v, v); }
static constexpr uint32_t BLACK = gray(0);
static constexpr uint32_t WHITE = gray(255);

static constexpr std::array<uint32_t, 16> palette = {
	0x1000, 0x1001, 0x1002, 0x1003, 0x1004, 0x1005, 0x1006, 0x1007,
	0x1008, 0x1009, 0x100a, 0x100b, 0x100c, 0x100d, 0x100e, 0x100f,
};

// One level of 'w' x 'h' items, packed row after row.
static Layout linear(PixelFormat format, Order order, unsigned w, unsigned h)
{
	return Layout{.format = format, .order = order, .levels = {
		Level{.countX = w, .countY = h, .stepX = 1, .stepY = w}}};
}

static std::vector<uint32_t> decode(const Layout& layout, std::span<const uint8_t> data)
{
	auto size = layout.imageSize();
	std::vector<uint32_t> result(size_t(size.x) * size_t(size.y), 0xdeadbeef);
	BitmapDecoder::decode(layout, palette, data, result);
	return result;
}

TEST_CASE("BitmapDecoder: sizes")
{
	SECTION("8x8 font, 16x16 characters") {
		Layout layout{.format = GRAY1, .levels = {
			Level{.countX =  8, .countY =  8, .stepX =  1, .stepY =    8},
			Level{.countX = 16, .countY = 16, .stepX = 64, .stepY = 1024}}};
		CHECK(layout.imageSize() == gl::ivec2(128, 128));
		CHECK(layout.dataSize() == 2048);
	}
	SECTION("partial last byte") {
		auto layout = linear(GRAY4, Order::MSB_FIRST, 3, 1);
		CHECK(layout.imageSize() == gl::ivec2(3, 1));
		CHECK(layout.dataSize() == 2);
	}
	SECTION("overlapping rows") {
		Layout layout{.format = GRAY8, .levels = {
			Level{.countX = 4, .countY = 3, .stepX = 1, .stepY = 1}}};
		CHECK(layout.imageSize() == gl::ivec2(4, 3));
		CHECK(layout.dataSize() == 6);
	}
	SECTION("largest image") {
		// the last item is bit 0 of byte 0x1fff'fffe, so dataSize() * 8 is 0xffff'fff8
		Layout layout{.format = GRAY1, .levels = {
			Level{.countX = 1, .countY = 2, .stepX = 1, .stepY = 0xffff'fff0}}};
		CHECK(layout.dataSize() == 0x1fff'ffff);
	}
}

TEST_CASE("BitmapDecoder: bit order")
{
	std::array<uint8_t, 1> data = {0x80};
	CHECK(decode(linear(GRAY1, Order::MSB_FIRST, 8, 1), data) ==
	      std::vector{WHITE, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK});
	CHECK(decode(linear(GRAY1, Order::LSB_FIRST, 8, 1), data) ==
	      std::vector{BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, BLACK, WHITE});

	std::array<uint8_t, 1> data2 = {0b00'01'10'11};
	CHECK(decode(linear(GRAY2, Order::MSB_FIRST, 4, 1), data2) ==
	      std::vector{gray(0), gray(85), gray(170), gray(255)});
	CHECK(decode(linear(GRAY2, Order::LSB_FIRST, 4, 1), data2) ==
	      std::vector{gray(255), gray(170), gray(85), gray(0)});
	CHECK(decode(linear(PALETTE2, Order::MSB_FIRST, 4, 1), data2) ==
	      std::vector{palette[0], palette[1], palette[2], palette[3]});

	std::array<uint8_t, 2> data4 = {0x12, 0xf0};
	CHECK(decode(linear(PALETTE4, Order::MSB_FIRST, 4, 1), data4) ==
	      std::vector{palette[1], palette[2], palette[15], palette[0]});
	CHECK(decode(linear(PALETTE4, Order::LSB_FIRST, 4, 1), data4) ==
	      std::vector{palette[2], palette[1], palette[0], palette[15]});
	CHECK(decode(linear(GRAY4, Order::MSB_FIRST, 2, 2), data4) ==
	      std::vector{gray(0x11), gray(0x22), gray(0xff), gray(0x00)});
}

TEST_CASE("BitmapDecoder: byte order")
{
	// grayscale uses the most significant 8 bits
	std::array<uint8_t, 4> data = {0x12, 0x34, 0x56, 0x78};
	CHECK(decode(linear(GRAY16, Order::MSB_FIRST, 2, 1), data) == std::vector{gray(0x12), gray(0x56)});
	CHECK(decode(linear(GRAY16, Order::LSB_FIRST, 2, 1), data) == std::vector{gray(0x34), gray(0x78)});
	CHECK(decode(linear(GRAY24, Order::MSB_FIRST, 1, 1), data) == std::vector{gray(0x12)});
	CHECK(decode(linear(GRAY24, Order::LSB_FIRST, 1, 1), data) == std::vector{gray(0x56)});
	CHECK(decode(linear(GRAY32, Order::MSB_FIRST, 1, 1), data) == std::vector{gray(0x12)});
	CHECK(decode(linear(GRAY32, Order::LSB_FIRST, 1, 1), data) == std::vector{gray(0x78)});
}

TEST_CASE("BitmapDecoder: pixel formats")
{
	auto one = [](PixelFormat format, std::vector<uint8_t> bigEndian) {
		auto result = decode(linear(format, Order::MSB_FIRST, 1, 1), bigEndian);
		REQUIRE(result.size() == 1);
		return result[0];
	};

	CHECK(one(GRAY8, {0x80}) == gray(0x80));
	CHECK(one(BRG233, {0b111'000'00}) == rgba(  0, 255,   0));
	CHECK(one(BRG233, {0b000'111'00}) == rgba(255,   0,   0));
	CHECK(one(BRG233, {0b000'000'11}) == rgba(  0,   0, 255));
	CHECK(one(BRG233, {0b001'100'01}) == rgba(146,  36,  85));

	CHECK(one(BRGX5551, {0b0'11111'00, 0b000'00000}) == rgba(  0, 255,   0));
	CHECK(one(BRGX5551, {0b0'00000'11, 0b111'00000}) == rgba(255,   0,   0));
	CHECK(one(BRGX5551, {0b0'00000'00, 0b000'11111}) == rgba(  0,   0, 255));
	CHECK(one(BRGX5551, {0b1'00000'00, 0b000'00000}) == BLACK);
	CHECK(one(BRGX5551, {0b0'10000'00, 0b000'00001}) == rgba(  0, 132,   8));
	CHECK(one(BRGYS5551, {0b0'11111'00, 0b000'00000}) == rgba(0, 255, 0));
	CHECK(one(BRGYS5551, {0b1'11111'00, 0b000'00000}) == 0);

	CHECK(one(BGR565, {0b11111'000, 0b000'00000}) == rgba(255,   0,   0));
	CHECK(one(BGR565, {0b00000'111, 0b111'00000}) == rgba(  0, 255,   0));
	CHECK(one(BGR565, {0b00000'000, 0b000'11111}) == rgba(  0,   0, 255));
	CHECK(one(BGR565, {0b00000'100, 0b000'00000}) == rgba(  0, 130,   0));
	CHECK(one(RGB565, {0b11111'000, 0b000'00000}) == rgba(  0,   0, 255));
	CHECK(one(RGB565, {0b00000'000, 0b000'11111}) == rgba(255,   0,   0));

	CHECK(one(RGB888, {0x11, 0x22, 0x33}) == rgba(0x33, 0x22, 0x11));
	CHECK(one(BGRA8888, {0x80, 0x11, 0x22, 0x33}) == rgba(0x11, 0x22, 0x33, 0x80));
	CHECK(one(RGBA8888, {0x80, 0x11, 0x22, 0x33}) == rgba(0x33, 0x22, 0x11, 0x80));

	// little endian RGBA8888 is R, G, B, A in memory
	std::array<uint8_t, 4> data = {0x11, 0x22, 0x33, 0x80};
	CHECK(decode(linear(RGBA8888, Order::LSB_FIRST, 1, 1), data) ==
	      std::vector{rgba(0x11, 0x22, 0x33, 0x80)});
}

TEST_CASE("BitmapDecoder: steps")
{
	SECTION("transposed: each byte is a column, MSB at the top") {
		Layout layout{.format = GRAY1, .levels = {
			Level{.countX = 8, .countY = 8, .stepX = 8, .stepY = 1}}};
		std::array<uint8_t, 8> data = {0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x81};
		auto image = decode(layout, data);
		for (int y = 0; y < 8; ++y) {
			for (int x = 0; x < 8; ++x) {
				bool set = (x == y) || (x == 7 && y == 0);
				CHECK(image[y * 8 + x] == (set ? WHITE : BLACK));
			}
		}
	}
	SECTION("16x16 sprite: 8x8 quadrants in order 0 2 / 1 3") {
		Layout layout{.format = GRAY1, .levels = {
			Level{.countX = 8, .countY = 8, .stepX =   1, .stepY =  8},
			Level{.countX = 2, .countY = 2, .stepX = 128, .stepY = 64}}};
		CHECK(layout.imageSize() == gl::ivec2(16, 16));
		CHECK(layout.dataSize() == 32);
		std::array<uint8_t, 32> data = {};
		data[ 0] = 0x80; // quadrant 0, row 0: pixel (0, 0)
		data[ 8] = 0x80; // quadrant 1, row 0: pixel (0, 8)
		data[16] = 0x01; // quadrant 2, row 0: pixel (15, 0)
		data[31] = 0x01; // quadrant 3, row 7: pixel (15, 15)
		auto image = decode(layout, data);
		for (int y = 0; y < 16; ++y) {
			for (int x = 0; x < 16; ++x) {
				bool set = gl::ivec2(x, y) == one_of(gl::ivec2(0, 0), gl::ivec2(0, 8),
				                                     gl::ivec2(15, 0), gl::ivec2(15, 15));
				CHECK(image[y * 16 + x] == (set ? WHITE : BLACK));
			}
		}
	}
	SECTION("three levels") {
		// 2x1 pixels per tile, 2x2 tiles per group, 2x1 groups
		Layout layout{.format = GRAY8, .levels = {
			Level{.countX = 2, .countY = 1, .stepX = 1, .stepY = 1},
			Level{.countX = 2, .countY = 2, .stepX = 2, .stepY = 4},
			Level{.countX = 2, .countY = 1, .stepX = 8, .stepY = 1}}};
		CHECK(layout.imageSize() == gl::ivec2(8, 2));
		CHECK(layout.dataSize() == 16);
		std::array<uint8_t, 16> data;
		for (int i = 0; i < 16; ++i) data[i] = uint8_t(i);
		CHECK(decode(layout, data) == std::vector{
			gray(0), gray(1), gray(2), gray(3),   gray( 8), gray( 9), gray(10), gray(11),
			gray(4), gray(5), gray(6), gray(7),   gray(12), gray(13), gray(14), gray(15)});
	}
	SECTION("overlapping rows read the same items") {
		Layout layout{.format = GRAY8, .levels = {
			Level{.countX = 3, .countY = 2, .stepX = 1, .stepY = 1}}};
		std::array<uint8_t, 4> data = {0x00, 0xff, 0x00, 0xff};
		CHECK(decode(layout, data) == std::vector{BLACK, WHITE, BLACK, WHITE, BLACK, WHITE});
	}
	SECTION("steps are in items") {
		std::array<uint8_t, 6> data = {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc};
		// high nibble of every byte
		CHECK(decode(Layout{.format = GRAY4, .levels = {
		              Level{.countX = 3, .countY = 1, .stepX = 2, .stepY = 1}}}, data) ==
		      std::vector{gray(0x11), gray(0x33), gray(0x55)});
		// every other 16-bit word, big endian
		CHECK(decode(Layout{.format = GRAY16, .levels = {
		              Level{.countX = 2, .countY = 1, .stepX = 2, .stepY = 1}}}, data) ==
		      std::vector{gray(0x12), gray(0x9a)});
		// rows of one 24-bit item, one item apart
		CHECK(decode(Layout{.format = RGB888, .levels = {
		              Level{.countX = 1, .countY = 2, .stepX = 1, .stepY = 1}}}, data) ==
		      std::vector{rgba(0x56, 0x34, 0x12), rgba(0xbc, 0x9a, 0x78)});
	}
}

TEST_CASE("BitmapDecoder: missing data is transparent")
{
	SECTION("whole rows and part of a row") {
		std::array<uint8_t, 5> data = {};
		CHECK(decode(linear(GRAY8, Order::MSB_FIRST, 3, 3), data) ==
		      std::vector{BLACK, BLACK, BLACK, BLACK, BLACK, 0u, 0u, 0u, 0u});
	}
	SECTION("item partially inside") {
		std::array<uint8_t, 3> data = {};
		CHECK(decode(linear(GRAY16, Order::MSB_FIRST, 2, 1), data) == std::vector{BLACK, 0u});
	}
	SECTION("per run, with tiles") {
		// two 2x2 tiles side by side, each tile 4 bytes
		Layout layout{.format = GRAY8, .levels = {
			Level{.countX = 2, .countY = 2, .stepX = 1, .stepY = 2},
			Level{.countX = 2, .countY = 1, .stepX = 4, .stepY = 1}}};
		std::array<uint8_t, 7> data = {};
		CHECK(decode(layout, data) == std::vector{BLACK, BLACK, BLACK, BLACK,
		                                          BLACK, BLACK, BLACK, 0u});
	}
	SECTION("sub-byte items") {
		std::array<uint8_t, 1> data = {0xff};
		CHECK(decode(linear(GRAY4, Order::MSB_FIRST, 2, 2), data) ==
		      std::vector{WHITE, WHITE, 0u, 0u});
	}
	SECTION("more data than needed") {
		std::array<uint8_t, 4> data = {0x00, 0xff, 0x00, 0x00};
		CHECK(decode(linear(GRAY8, Order::MSB_FIRST, 2, 1), data) == std::vector{BLACK, WHITE});
	}
	SECTION("no data at all") {
		CHECK(decode(linear(GRAY8, Order::MSB_FIRST, 2, 1), {}) == std::vector{0u, 0u});
	}
}

TEST_CASE("BitmapDecoder: natural order matches the general loop")
{
	// Sub-byte items in natural order take a separate path. An extra level with
	// count 1 describes the same image, but goes through the general loop.
	std::vector<uint8_t> data(2000);
	for (auto i : xrange(data.size())) data[i] = uint8_t(i * 37 + 11);
	for (auto format : {GRAY1, GRAY2, GRAY4, PALETTE2, PALETTE4}) {
		for (auto order : {Order::MSB_FIRST, Order::LSB_FIRST}) {
			// widths that end in the middle of a byte, rows spanning several chunks
			for (auto [w, h] : {std::pair{3u, 5u}, std::pair{301u, 7u}, std::pair{1000u, 2u}}) {
				auto natural = linear(format, order, w, h);
				auto general = natural;
				general.levels.push_back(Level{});
				auto full = natural.dataSize();
				for (auto size : {0u, 1u, 2u, full / 2, full - 1, full}) {
					auto part = std::span(data).first(size);
					CHECK(decode(natural, part) == decode(general, part));
				}
			}
		}
	}
}
