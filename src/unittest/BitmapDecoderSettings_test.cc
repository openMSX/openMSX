#include "catch.hpp"
#include "BitmapDecoderSettings.hh"

#include "StringOp.hh"
#include "xrange.hh"

#include <initializer_list>
#include <string>
#include <vector>

using namespace openmsx;
using namespace openmsx::BitmapDecoder;

static Settings makeSettings(unsigned bitsPerItem, ColorMode mode, std::initializer_list<LevelSettings> levels)
{
	Settings result{.bitsPerItem = bitsPerItem, .colorMode = mode, .levels = {}};
	for (const auto& l : levels) result.levels.push_back(l);
	return result;
}

// Numbers only; the viewer also accepts symbols and Tcl expressions.
static constexpr auto evalNumber = [](std::string_view text) -> std::expected<unsigned, std::string> {
	if (auto value = StringOp::stringTo<unsigned>(text)) return *value;
	return std::unexpected(std::string("not a number"));
};

static Layout resolveOk(const Settings& settings, unsigned availableBytes = 0x10000)
{
	auto result = resolve(settings, availableBytes, evalNumber);
	REQUIRE(result.has_value());
	return *result;
}

// The indices (see fieldIndex()) of the fields that have an error.
static std::vector<size_t> errorFields(const Settings& settings, unsigned availableBytes = 0x10000)
{
	auto result = resolve(settings, availableBytes, evalNumber);
	REQUIRE(!result.has_value());
	std::vector<size_t> fields;
	for (auto i : xrange(result.error().size())) {
		if (!result.error()[i].empty()) fields.push_back(i);
	}
	return fields;
}

TEST_CASE("BitmapDecoderSettings: valid choices")
{
	CHECK(std::vector(validBitsPerItem().begin(), validBitsPerItem().end()) ==
	      std::vector<unsigned>{1, 2, 4, 8, 16, 24, 32});
	// every valid combination resolves, to a format of the right size
	for (auto bits : validBitsPerItem()) {
		auto modes = validColorModes(bits);
		REQUIRE(!modes.empty());
		for (auto mode : modes) {
			auto layout = resolveOk(makeSettings(bits, mode, {{"4", "4", "", ""}}));
			CHECK(bitsPerItem(layout.format) == bits);
		}
	}
	CHECK(resolveOk(makeSettings(2, ColorMode::PALETTE, {{"1", "1", "", ""}})).format == PixelFormat::PALETTE2);
	CHECK(resolveOk(makeSettings(4, ColorMode::PALETTE, {{"1", "1", "", ""}})).format == PixelFormat::PALETTE4);
	CHECK(resolveOk(makeSettings(1, ColorMode::GRAYSCALE, {{"1", "1", "", ""}})).format == PixelFormat::GRAY1);
	CHECK(resolveOk(makeSettings(32, ColorMode::GRAYSCALE, {{"1", "1", "", ""}})).format == PixelFormat::GRAY32);
}

TEST_CASE("BitmapDecoderSettings: order")
{
	auto settings = makeSettings(1, ColorMode::GRAYSCALE, {{"8", "1", "", ""}});
	settings.bitOrder = Order::LSB_FIRST;
	settings.byteOrder = Order::MSB_FIRST;
	CHECK(resolveOk(settings).order == Order::LSB_FIRST);
	settings.bitsPerItem = 16;
	CHECK(resolveOk(settings).order == Order::MSB_FIRST);
	settings.byteOrder = Order::LSB_FIRST;
	CHECK(resolveOk(settings).order == Order::LSB_FIRST);
}

TEST_CASE("BitmapDecoderSettings: design examples")
{
	using enum ColorMode;
	SECTION("A: map, low byte of each word") {
		auto layout = resolveOk(makeSettings(8, BRG233, {{"128", "64", "2", ""}}));
		CHECK(layout.format == PixelFormat::BRG233);
		REQUIRE(layout.levels.size() == 1);
		CHECK(layout.levels[0] == Level{128, 64, 2, 256});
		CHECK(layout.imageSize() == gl::ivec2(128, 64));
		CHECK(layout.dataSize() == 16383); // up to and including the last item
	}
	SECTION("A: map, bit 7 of each word's low byte") {
		auto layout = resolveOk(makeSettings(1, GRAYSCALE, {{"128", "64", "16", ""}}));
		REQUIRE(layout.levels.size() == 1);
		CHECK(layout.levels[0] == Level{128, 64, 16, 2048});
	}
	SECTION("B: BIOS font") {
		auto layout = resolveOk(makeSettings(1, GRAYSCALE, {{"8", "8", "", ""}, {"16", "16", "", ""}}));
		REQUIRE(layout.levels.size() == 2);
		CHECK(layout.levels[0] == Level{8, 8, 1, 8});
		CHECK(layout.levels[1] == Level{16, 16, 64, 1024});
		CHECK(layout.imageSize() == gl::ivec2(128, 128));
		CHECK(layout.dataSize() == 2048);
	}
	SECTION("C: 16x16 sprites") {
		auto layout = resolveOk(makeSettings(1, GRAYSCALE,
			{{"8", "8", "", ""}, {"2", "2", "128", "64"}, {"8", "4", "", ""}}));
		REQUIRE(layout.levels.size() == 3);
		CHECK(layout.levels[0] == Level{8, 8, 1, 8});
		CHECK(layout.levels[1] == Level{2, 2, 128, 64});
		CHECK(layout.levels[2] == Level{8, 4, 256, 2048});
		CHECK(layout.imageSize() == gl::ivec2(128, 64));
		CHECK(layout.dataSize() == 1024);
	}
	SECTION("D: screen 5 super-tiles") {
		auto layout = resolveOk(makeSettings(4, PALETTE,
			{{"8", "8", "", ""}, {"4", "4", "", ""}, {"16", "16", "", ""}}), 0x20000);
		REQUIRE(layout.levels.size() == 3);
		CHECK(layout.levels[0] == Level{8, 8, 1, 8});
		CHECK(layout.levels[1] == Level{4, 4, 64, 256});
		CHECK(layout.levels[2] == Level{16, 16, 1024, 16384});
		CHECK(layout.imageSize() == gl::ivec2(512, 512));
		CHECK(layout.dataSize() == 131072);
	}
	SECTION("E: transposed patterns") {
		auto layout = resolveOk(makeSettings(1, GRAYSCALE, {{"8", "8", "8", "1"}, {"16", "16", "", ""}}));
		REQUIRE(layout.levels.size() == 2);
		CHECK(layout.levels[0] == Level{8, 8, 8, 1});
		CHECK(layout.levels[1] == Level{16, 16, 64, 1024});
		CHECK(layout.dataSize() == 2048);
	}
}

TEST_CASE("BitmapDecoderSettings: auto steps")
{
	using enum ColorMode;
	SECTION("rows are rounded up to whole bytes") {
		auto layout = resolveOk(makeSettings(1, GRAYSCALE, {{"3", "2", "", ""}}));
		CHECK(layout.levels[0] == Level{3, 2, 1, 8});
		layout = resolveOk(makeSettings(4, GRAYSCALE, {{"3", "2", "", ""}}));
		CHECK(layout.levels[0] == Level{3, 2, 1, 4});
		layout = resolveOk(makeSettings(24, RGB888, {{"3", "2", "", ""}}));
		CHECK(layout.levels[0] == Level{3, 2, 1, 3});
	}
	SECTION("tiles are rounded up to whole bytes") {
		// 3x3 pixels at 2 bits: extent 3 + 2 * 4 = 11 items, rounded up to 12
		auto layout = resolveOk(makeSettings(2, GRAYSCALE, {{"3", "3", "", ""}, {"2", "2", "", ""}}));
		CHECK(layout.levels[0] == Level{3, 3, 1, 4});
		CHECK(layout.levels[1] == Level{2, 2, 12, 24});
	}
	SECTION("whitespace and hex") {
		auto layout = resolveOk(makeSettings(8, GRAYSCALE, {{" 0x10 ", "\t2", " ", "0b100"}}));
		CHECK(layout.levels[0] == Level{16, 2, 1, 4});
	}
}

TEST_CASE("BitmapDecoderSettings: auto height")
{
	using enum ColorMode;
	auto rows = [](const Settings& settings, unsigned availableBytes) {
		return resolveOk(settings, availableBytes).levels.back().countY;
	};
	auto linear = makeSettings(8, GRAYSCALE, {{"4", "", "", ""}});
	CHECK(rows(linear, 12) == 3);
	CHECK(rows(linear, 11) == 2); // the third row doesn't fit completely
	CHECK(rows(linear, 4) == 1);
	CHECK(rows(linear, 3) == 1);  // nothing fits: still one row
	CHECK(rows(linear, 0) == 1);
	CHECK(rows(linear, 0xffff'ffff) == MAX_IMAGE_SIZE);

	auto font = makeSettings(1, GRAYSCALE, {{"8", "8", "", ""}, {"16", "", "", ""}});
	CHECK(rows(font, 2048) == 16);
	CHECK(rows(font, 2047) == 15);
	CHECK(rows(font, 0x10000) == MAX_IMAGE_SIZE / 8);

	// without the 512MB limit, 16 rows would fit
	auto bigRows = makeSettings(8, GRAYSCALE, {{"1", "", "", "0x10000000"}});
	CHECK(rows(bigRows, 0xffff'ffff) == 2);
}

TEST_CASE("BitmapDecoderSettings: field errors")
{
	using enum ColorMode;
	using enum Field;
	auto one = [](std::string countX, std::string countY, std::string stepX, std::string stepY) {
		return makeSettings(8, GRAYSCALE, {{std::move(countX), std::move(countY), std::move(stepX), std::move(stepY)}});
	};
	SECTION("parse errors") {
		CHECK(errorFields(one("abc", "1", "", "")) == std::vector{fieldIndex(0, COUNT_X)});
		CHECK(errorFields(one("1", "1", "1x", "")) == std::vector{fieldIndex(0, STEP_X)});
		CHECK(errorFields(one("1", "1", "", "-1")) == std::vector{fieldIndex(0, STEP_Y)});
		CHECK(errorFields(one("0", "1", "", "")) == std::vector{fieldIndex(0, COUNT_X)});
		CHECK(errorFields(one("1", "1", "0", "")) == std::vector{fieldIndex(0, STEP_X)});
		CHECK(errorFields(one("1", "1", "0x100000000", "")) == std::vector{fieldIndex(0, STEP_X)});
		CHECK(errorFields(one("1", "1", "99999999999999999999999", "")) == std::vector{fieldIndex(0, STEP_X)});
	}
	SECTION("several errors at once") {
		CHECK(errorFields(one("x", "0", "y", "")) ==
		      std::vector{fieldIndex(0, COUNT_X), fieldIndex(0, COUNT_Y), fieldIndex(0, STEP_X)});
	}
	SECTION("only the outermost height can be auto") {
		CHECK(errorFields(one("", "1", "", "")) == std::vector{fieldIndex(0, COUNT_X)});
		CHECK(errorFields(makeSettings(1, GRAYSCALE, {{"8", "", "", ""}, {"", "", "", ""}})) ==
		      std::vector{fieldIndex(0, COUNT_Y), fieldIndex(1, COUNT_X)});
	}
	SECTION("image too large: the count where it first exceeds the maximum") {
		CHECK(resolve(one("2048", "2048", "", ""), 0, evalNumber).has_value());
		CHECK(errorFields(one("2049", "1", "", "")) == std::vector{fieldIndex(0, COUNT_X)});
		CHECK(errorFields(one("1", "2049", "", "")) == std::vector{fieldIndex(0, COUNT_Y)});
		CHECK(errorFields(makeSettings(1, GRAYSCALE, {{"64", "64", "", ""}, {"33", "32", "", ""}})) ==
		      std::vector{fieldIndex(1, COUNT_X)});
		CHECK(errorFields(makeSettings(1, GRAYSCALE, {{"64", "64", "", ""}, {"32", "33", "", ""}})) ==
		      std::vector{fieldIndex(1, COUNT_Y)});
		CHECK(errorFields(makeSettings(1, GRAYSCALE, {{"4096", "1", "", ""}, {"1", "1", "", ""}})) ==
		      std::vector{fieldIndex(0, COUNT_X)});
	}
	SECTION("span of 512MB or more: the step that makes it too large") {
		// the largest span: the last item is bit 0 of byte 0x1fff'fffe
		auto largest = makeSettings(1, GRAYSCALE, {{"1", "2", "", "0xfffffff0"}});
		CHECK(resolveOk(largest).dataSize() == 0x1fff'ffff);
		CHECK(errorFields(makeSettings(1, GRAYSCALE, {{"1", "2", "", "0xfffffff8"}})) ==
		      std::vector{fieldIndex(0, STEP_Y)});
		CHECK(errorFields(one("2", "1", "0x20000000", "")) == std::vector{fieldIndex(0, STEP_X)});
		// an auto step can also be too large
		CHECK(errorFields(makeSettings(1, GRAYSCALE, {{"2", "1", "0xfffffff0", ""}})) ==
		      std::vector{fieldIndex(0, STEP_Y)});
		CHECK(errorFields(makeSettings(8, GRAYSCALE, {{"1", "2", "", "0x10000000"}, {"3", "1", "", ""}})) ==
		      std::vector{fieldIndex(1, STEP_X)});
	}
}

TEST_CASE("BitmapDecoderSettings: evaluator")
{
	std::vector<std::string> seen;
	auto evaluate = [&](std::string_view text) -> std::expected<unsigned, std::string> {
		seen.emplace_back(text);
		if (text == "16 * 8") return 128;
		if (text == "8") return 8;
		return std::unexpected(std::string("invalid expression"));
	};
	auto settings = makeSettings(1, ColorMode::GRAYSCALE, {{" 8 ", "8", "", ""}, {"2", "", " 16 * 8\t", ""}});
	settings.levels[1].countX = "8";
	auto layout = resolve(settings, 0x10000, evaluate);
	REQUIRE(layout.has_value());
	CHECK(layout->levels[1] == Level{8, MAX_IMAGE_SIZE / 8, 128, 1024}); // auto height, capped
	// trimmed, and empty fields are auto without calling the evaluator
	CHECK(seen == std::vector<std::string>{"8", "8", "8", "16 * 8"});

	settings.levels[1].countX = "oops";
	auto errors = resolve(settings, 0x10000, evaluate);
	REQUIRE(!errors.has_value());
	CHECK(errors.error()[fieldIndex(1, Field::COUNT_X)] == "invalid expression");
}
