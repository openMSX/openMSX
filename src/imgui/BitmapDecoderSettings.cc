#include "BitmapDecoderSettings.hh"

#include "StringOp.hh"
#include "stl.hh"
#include "strCat.hh"
#include "unreachable.hh"
#include "xrange.hh"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <limits>
#include <optional>

namespace openmsx::BitmapDecoder {

std::span<const unsigned> validBitsPerItem()
{
	static constexpr std::array result = {1u, 2u, 4u, 8u, 16u, 24u, 32u};
	return result;
}

std::span<const ColorMode> validColorModes(unsigned bitsPerItem)
{
	using enum ColorMode;
	static constexpr std::array subByte1 = {GRAYSCALE};
	static constexpr std::array subByte  = {PALETTE, GRAYSCALE};
	static constexpr std::array bits8    = {BRG233, GRAYSCALE};
	static constexpr std::array bits16   = {RGB565, BGR565, BRGX5551, BRGYS5551, GRAYSCALE};
	static constexpr std::array bits24   = {RGB888, GRAYSCALE};
	static constexpr std::array bits32   = {RGBA8888, BGRA8888, GRAYSCALE};
	switch (bitsPerItem) {
	case 1:         return subByte1;
	case 2: case 4: return subByte;
	case 8:         return bits8;
	case 16:        return bits16;
	case 24:        return bits24;
	case 32:        return bits32;
	default:        UNREACHABLE;
	}
}

[[nodiscard]] static PixelFormat toPixelFormat(unsigned bitsPerItem, ColorMode mode)
{
	using enum PixelFormat;
	switch (mode) {
	case ColorMode::GRAYSCALE:
		switch (bitsPerItem) {
		case  1: return GRAY1;
		case  2: return GRAY2;
		case  4: return GRAY4;
		case  8: return GRAY8;
		case 16: return GRAY16;
		case 24: return GRAY24;
		case 32: return GRAY32;
		default: UNREACHABLE;
		}
	case ColorMode::PALETTE:   return (bitsPerItem == 2) ? PALETTE2 : PALETTE4;
	case ColorMode::BRG233:    return BRG233;
	case ColorMode::BRGX5551:  return BRGX5551;
	case ColorMode::BRGYS5551: return BRGYS5551;
	case ColorMode::BGR565:    return BGR565;
	case ColorMode::RGB565:    return RGB565;
	case ColorMode::RGB888:    return RGB888;
	case ColorMode::BGRA8888:  return BGRA8888;
	case ColorMode::RGBA8888:  return RGBA8888;
	}
	UNREACHABLE;
}

std::expected<Layout, FieldErrors> resolve(
	const Settings& settings, unsigned availableBytes, Evaluator evaluate)
{
	const unsigned bits = settings.bitsPerItem;
	assert(contains(validBitsPerItem(), bits));
	assert(contains(validColorModes(bits), settings.colorMode));
	assert(!settings.levels.empty());
	const auto numLevels = settings.levels.size();
	const auto outer = numLevels - 1;

	FieldErrors errors;
	bool failed = false;
	auto fail = [&](size_t level, Field field, std::string message) {
		auto& e = errors[fieldIndex(level, field)];
		if (e.empty()) e = std::move(message);
		failed = true;
	};

	// nullopt: auto (or invalid, but then 'failed' is set)
	auto parse = [&](size_t level, Field field, std::string_view text, bool autoAllowed) -> std::optional<uint32_t> {
		StringOp::trim(text, " \t");
		if (text.empty()) {
			if (!autoAllowed) fail(level, field, "Required: only the outermost height can be auto");
			return {};
		}
		auto value = evaluate(text);
		if (!value) {
			fail(level, field, std::move(value.error()));
		} else if (*value == 0) {
			fail(level, field, "Must be at least 1");
		} else {
			return uint32_t(*value);
		}
		return {};
	};

	struct Parsed {
		std::optional<uint32_t> countX, countY, stepX, stepY;
	};
	static_vector<Parsed, MAX_LEVELS> parsed;
	for (auto i : xrange(numLevels)) {
		const auto& s = settings.levels[i];
		parsed.push_back(Parsed{
			.countX = parse(i, Field::COUNT_X, s.countX, false),
			.countY = parse(i, Field::COUNT_Y, s.countY, i == outer),
			.stepX  = parse(i, Field::STEP_X,  s.stepX,  true),
			.stepY  = parse(i, Field::STEP_Y,  s.stepY,  true),
		});
	}
	if (failed) return std::unexpected(std::move(errors));

	// Image size: the error goes to the count at which a dimension first gets too large.
	uint64_t width = 1;
	uint64_t innerHeight = 1; // without the outermost count, when that is auto
	for (auto i : xrange(numLevels)) {
		width *= *parsed[i].countX;
		if (width > MAX_IMAGE_SIZE) {
			fail(i, Field::COUNT_X, strCat("Image wider than ", MAX_IMAGE_SIZE, " pixels"));
			break;
		}
	}
	for (auto i : xrange(numLevels)) {
		if (!parsed[i].countY) break; // the outermost, auto
		innerHeight *= *parsed[i].countY;
		if (innerHeight > MAX_IMAGE_SIZE) {
			fail(i, Field::COUNT_Y, strCat("Image taller than ", MAX_IMAGE_SIZE, " pixels"));
			break;
		}
	}
	if (failed) return std::unexpected(std::move(errors));

	// Steps and extent (the span from the first to the last item, in items),
	// innermost level first. The error for a too large span goes to the step
	// that makes it too large.
	const uint64_t itemsPerByte = (bits < 8) ? (8 / bits) : 1;
	auto roundUp = [&](uint64_t items) { return (items + itemsPerByte - 1) / itemsPerByte * itemsPerByte; };
	// all bit positions, rounded up to whole bytes, must fit in 32 bits
	const uint64_t maxEndItem = (uint64_t(std::numeric_limits<uint32_t>::max()) / 8 * 8) / bits;
	auto tooLarge = [&](size_t level, Field field) {
		fail(level, field, "Too large: the image would span 512MB or more");
	};

	Layout layout{
		.format = toPixelFormat(bits, settings.colorMode),
		.order = (bits < 8) ? settings.bitOrder : (bits > 8) ? settings.byteOrder : Order::MSB_FIRST,
		.levels = {},
	};
	uint64_t endItem = 1; // one past the last item of the levels so far
	for (auto i : xrange(numLevels)) {
		const auto& p = parsed[i];
		uint64_t stepX = p.stepX ? *p.stepX : (i == 0) ? 1 : roundUp(endItem);
		if (stepX > maxEndItem) { tooLarge(i, Field::STEP_X); break; }
		endItem += (*p.countX - 1) * stepX;
		if (endItem > maxEndItem) { tooLarge(i, Field::STEP_X); break; }

		uint64_t stepY = p.stepY ? *p.stepY : roundUp(*p.countX * stepX);
		if (stepY > maxEndItem) { tooLarge(i, Field::STEP_Y); break; }
		uint64_t countY = 0;
		if (p.countY) {
			countY = *p.countY;
		} else {
			// as many rows as fit completely, at least 1, at most what MAX_IMAGE_SIZE allows
			auto available = std::min<uint64_t>(uint64_t(availableBytes) * 8 / bits, maxEndItem);
			auto rows = (available >= endItem) ? ((available - endItem) / stepY + 1) : 1;
			countY = std::min<uint64_t>(rows, MAX_IMAGE_SIZE / innerHeight);
		}
		endItem += (countY - 1) * stepY;
		if (endItem > maxEndItem) { tooLarge(i, Field::STEP_Y); break; }

		layout.levels.push_back(Level{
			.countX = *p.countX,
			.countY = unsigned(countY),
			.stepX = uint32_t(stepX),
			.stepY = uint32_t(stepY),
		});
	}
	if (failed) return std::unexpected(std::move(errors));
	return layout;
}

} // namespace openmsx::BitmapDecoder
