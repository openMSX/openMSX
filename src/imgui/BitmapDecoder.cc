#include "BitmapDecoder.hh"

#include "endian.hh"
#include "enumerate.hh"
#include "narrow.hh"
#include "one_of.hh"
#include "ranges.hh"
#include "xrange.hh"

#include <algorithm>
#include <cassert>

namespace openmsx::BitmapDecoder {

uint32_t Layout::dataSize() const
{
	uint32_t endItem = 1; // one past the last item
	for (const auto& level : levels) {
		assert(level.countX != 0);
		assert(level.countY != 0);
		endItem += (level.countX - 1) * level.stepX + (level.countY - 1) * level.stepY;
	}
	return (endItem * bitsPerItem(format) + 7) / 8;
}

gl::ivec2 Layout::imageSize() const
{
	gl::ivec2 size(1, 1);
	for (const auto& level : levels) {
		size *= gl::ivec2(narrow<int>(level.countX), narrow<int>(level.countY));
	}
	return size;
}

// 'addr' in bits for items smaller than a byte, otherwise in bytes
template<unsigned BITS, Order ORDER>
[[nodiscard]] static uint32_t readItem(std::span<const uint8_t> data, uint32_t addr)
{
	if constexpr (BITS < 8) {
		unsigned byte = data[addr / 8];
		auto bit = unsigned(addr % 8);
		assert(bit + BITS <= 8); // never crosses a byte boundary
		auto shift = (ORDER == Order::MSB_FIRST) ? (8 - BITS - bit) : bit;
		return (byte >> shift) & ((1u << BITS) - 1);
	} else {
		constexpr unsigned BYTES = BITS / 8;
		auto bytes = subspan<BYTES>(data, addr);
		constexpr bool BE = ORDER == Order::MSB_FIRST;
		if constexpr (BYTES == 1) {
			return bytes[0];
		} else if constexpr (BYTES == 2 && !BE) {
			return Endian::read_UA_L16(bytes.data());
		} else if constexpr (BYTES == 4) {
			return BE ? Endian::read_UA_B32(bytes.data()) : Endian::read_UA_L32(bytes.data());
		} else {
			uint32_t result = 0;
			for (auto i : xrange(BYTES)) {
				result = (result << 8) | bytes[(ORDER == Order::MSB_FIRST) ? i : (BYTES - 1 - i)];
			}
			return result;
		}
	}
}

[[nodiscard]] static constexpr uint32_t rgba(uint32_t r, uint32_t g, uint32_t b, uint32_t a = 255)
{
	return (r << 0) | (g << 8) | (b << 16) | (a << 24);
}
[[nodiscard]] static constexpr uint32_t scale2(uint32_t v) { return v * 0x55; }
[[nodiscard]] static constexpr uint32_t scale3(uint32_t v) { return (v * 255 + 3) / 7; }
[[nodiscard]] static constexpr uint32_t scale5(uint32_t v) { return (v << 3) | (v >> 2); }
[[nodiscard]] static constexpr uint32_t scale6(uint32_t v) { return (v << 2) | (v >> 4); }

template<PixelFormat FORMAT>
[[nodiscard]] static uint32_t toRGBA(uint32_t v, std::span<const uint32_t, 16> palette)
{
	using enum PixelFormat;
	constexpr auto BITS = bitsPerItem(FORMAT);
	if constexpr (FORMAT == one_of(GRAY1, GRAY2, GRAY4, GRAY8)) {
		auto g = v * (255 / ((1u << BITS) - 1));
		return rgba(g, g, g);
	} else if constexpr (FORMAT == one_of(GRAY16, GRAY24, GRAY32)) {
		auto g = v >> (BITS - 8);
		return rgba(g, g, g);
	} else if constexpr (FORMAT == one_of(PALETTE2, PALETTE4)) {
		return palette[v];
	} else if constexpr (FORMAT == BRG233) {
		return rgba(scale3((v >> 2) & 7), scale3(v >> 5), scale2(v & 3));
	} else if constexpr (FORMAT == BRGX5551) {
		return rgba(scale5((v >> 5) & 31), scale5((v >> 10) & 31), scale5(v & 31));
	} else if constexpr (FORMAT == BRGYS5551) {
		if (v & 0x8000) return 0;
		return rgba(scale5((v >> 5) & 31), scale5((v >> 10) & 31), scale5(v & 31));
	} else if constexpr (FORMAT == BGR565) {
		return rgba(scale5(v >> 11), scale6((v >> 5) & 63), scale5(v & 31));
	} else if constexpr (FORMAT == RGB565) {
		return rgba(scale5(v & 31), scale6((v >> 5) & 63), scale5(v >> 11));
	} else if constexpr (FORMAT == RGB888) {
		return v | 0xFF000000;
	} else if constexpr (FORMAT == BGRA8888) {
		return rgba((v >> 16) & 255, (v >> 8) & 255, v & 255, v >> 24);
	} else {
		static_assert(FORMAT == RGBA8888);
		return v; // already in the output format
	}
}

// Steps through the indices of several levels as one mixed-radix number, the
// first added level changes fastest, and keeps track of the matching offset.
// While carrying, the offset may temporarily wrap around; unsigned arithmetic
// makes the end result exact anyway.
class LevelCounter {
public:
	void add(unsigned count, uint32_t step) {
		digits.push_back(Digit{.count = count, .step = step});
	}

	void next() {
		for (auto& d : digits) {
			offset += d.step;
			if (++d.index < d.count) return;
			offset -= d.count * d.step;
			d.index = 0;
		}
	}

	[[nodiscard]] uint32_t getOffset() const { return offset; }

private:
	struct Digit {
		unsigned count = 1;
		unsigned index = 0;
		uint32_t step = 0;
	};
	static_vector<Digit, MAX_LEVELS> digits;
	uint32_t offset = 0;
};

// The number of items (each 'itemSize' long), out of 'count' items starting at
// 'addr' and spaced 'step' apart, that lie completely before 'dataEnd'; all in
// the same address unit. Addresses only increase, so these are the first items.
[[nodiscard]] static uint32_t itemsInside(uint32_t addr, uint32_t step, uint32_t count,
                                          unsigned itemSize, uint32_t dataEnd)
{
	assert(step != 0);
	if (addr + itemSize > dataEnd) return 0;
	return std::min(count, (dataEnd - itemSize - addr) / step + 1);
}

// Without tiling and in natural order the image is one sequence of items. For
// items smaller than a byte this is much faster than the general loop: all
// items of a byte are extracted at once. Decoding in chunks keeps the data in
// the L1 cache for the conversion pass.
template<PixelFormat FORMAT, Order ORDER>
static void decodeContiguous(std::span<const uint32_t, 16> palette, std::span<const uint8_t> data,
                             uint32_t dataBits, std::span<uint32_t> output)
{
	constexpr auto BITS = bitsPerItem(FORMAT);
	static_assert(BITS < 8);
	constexpr unsigned PER_BYTE = 8 / BITS;
	constexpr bool SEPARATE_PASS = FORMAT != one_of(PixelFormat::PALETTE2, PixelFormat::PALETTE4);
	constexpr size_t CHUNK = 256; // a multiple of PER_BYTE

	// only unrolled when 'out' has a static extent, as it has for full bytes
	auto unpack = [&](unsigned byte, auto out) {
		for (auto j : xrange(out.size())) {
			auto shift = (ORDER == Order::MSB_FIRST) ? (8 - BITS * (j + 1)) : (BITS * j);
			auto value = (byte >> shift) & ((1u << BITS) - 1);
			out[j] = SEPARATE_PASS ? value : toRGBA<FORMAT>(value, palette);
		}
	};
	auto n = std::min(output.size(), size_t(dataBits / BITS)); // items completely inside the data
	size_t byteIdx = 0;
	for (size_t i = 0; i < n; i += CHUNK) {
		auto chunk = output.subspan(i, std::min(CHUNK, n - i));
		auto fullBytes = chunk.size() / PER_BYTE;
		for (auto j : xrange(fullBytes)) {
			unpack(data[byteIdx++], subspan<PER_BYTE>(chunk, j * PER_BYTE));
		}
		if (chunk.size() % PER_BYTE) unpack(data[byteIdx], chunk.subspan(fullBytes * PER_BYTE));
		if constexpr (SEPARATE_PASS) {
			for (auto& p : chunk) p = toRGBA<FORMAT>(p, palette);
		}
	}
	std::ranges::fill(output.subspan(n), 0);
}

template<PixelFormat FORMAT, Order ORDER>
static void decodeImpl(const Layout& layout, std::span<const uint32_t, 16> palette,
                       std::span<const uint8_t> data, std::span<uint32_t> output)
{
	constexpr auto BITS = bitsPerItem(FORMAT);
	// All addresses below are in bits for items smaller than a byte, otherwise in bytes.
	constexpr unsigned UNIT = (BITS < 8) ? 1 : 8;
	constexpr unsigned ITEM = BITS / UNIT; // the size of an item, in address units
	auto dataEnd = uint32_t(std::min<size_t>(data.size(), layout.dataSize()) * (8 / UNIT));
	if constexpr (BITS < 8) {
		const auto& level = layout.levels.front();
		if (layout.levels.size() == 1 && level.stepX == 1 && level.stepY == level.countX) {
			decodeContiguous<FORMAT, ORDER>(palette, data, dataEnd, output);
			return;
		}
	}

	auto size = layout.imageSize();
	auto width = size_t(size.x);

	// The image is decoded in runs: the pixels of one row of one pixel-level block.
	const auto& pixelLevel = layout.levels.front();
	uint32_t runLength = pixelLevel.countX;
	uint32_t pixelStep = pixelLevel.stepX * ITEM;

	uint32_t rowEnd = ITEM; // one past the end of a row, relative to its start
	LevelCounter rows;
	LevelCounter firstRun;
	for (auto [i, level] : enumerate(layout.levels)) {
		rowEnd += (level.countX - 1) * level.stepX * ITEM;
		rows.add(level.countY, level.stepY * ITEM);
		if (i != 0) firstRun.add(level.countX, level.stepX * ITEM);
	}

	// Most conversions are faster in a separate pass, which the compiler
	// vectorizes. Palette lookups can't be vectorized, so for those converting
	// while decoding is faster.
	constexpr bool SEPARATE_PASS = FORMAT != one_of(PixelFormat::PALETTE2, PixelFormat::PALETTE4);
	auto convert = [&](std::span<uint32_t> pixels) {
		for (auto& p : pixels) p = toRGBA<FORMAT>(p, palette);
	};

	for (auto y : xrange(size_t(size.y))) {
		auto line = output.subspan(y * width, width);
		auto rowStart = rows.getOffset();
		bool complete = rowStart + rowEnd <= dataEnd;
		auto runs = firstRun;
		for (size_t x = 0; x < width; x += runLength) {
			auto run = line.subspan(x, runLength);
			auto addr = rowStart + runs.getOffset();
			auto n = complete ? runLength
			                  : itemsInside(addr, pixelStep, runLength, ITEM, dataEnd);
			for (auto& p : run.first(n)) {
				auto value = readItem<BITS, ORDER>(data, addr);
				p = SEPARATE_PASS ? value : toRGBA<FORMAT>(value, palette);
				addr += pixelStep;
			}
			if (!complete) {
				if constexpr (SEPARATE_PASS) convert(run.first(n));
				std::ranges::fill(run.subspan(n), 0);
			}
			runs.next();
		}
		if (SEPARATE_PASS && complete) convert(line);
		rows.next();
	}
}

template<PixelFormat FORMAT>
static void decodeFormat(const Layout& layout, auto&&... args)
{
	if constexpr (bitsPerItem(FORMAT) == 8) {
		decodeImpl<FORMAT, Order::MSB_FIRST>(layout, args...); // order doesn't matter
	} else if (layout.order == Order::MSB_FIRST) {
		decodeImpl<FORMAT, Order::MSB_FIRST>(layout, args...);
	} else {
		decodeImpl<FORMAT, Order::LSB_FIRST>(layout, args...);
	}
}

void decode(const Layout& layout, std::span<const uint32_t, 16> palette,
            std::span<const uint8_t> data, std::span<uint32_t> output)
{
	assert(!layout.levels.empty());
	assert(std::ranges::all_of(layout.levels, [](const Level& l) {
		return l.countX != 0 && l.countY != 0 && l.stepX != 0 && l.stepY != 0;
	}));
	assert(output.size() == size_t(layout.imageSize().x) * size_t(layout.imageSize().y));

	switch (layout.format) {
	using enum PixelFormat;
	case GRAY1:     decodeFormat<GRAY1    >(layout, palette, data, output); break;
	case GRAY2:     decodeFormat<GRAY2    >(layout, palette, data, output); break;
	case GRAY4:     decodeFormat<GRAY4    >(layout, palette, data, output); break;
	case GRAY8:     decodeFormat<GRAY8    >(layout, palette, data, output); break;
	case GRAY16:    decodeFormat<GRAY16   >(layout, palette, data, output); break;
	case GRAY24:    decodeFormat<GRAY24   >(layout, palette, data, output); break;
	case GRAY32:    decodeFormat<GRAY32   >(layout, palette, data, output); break;
	case PALETTE2:  decodeFormat<PALETTE2 >(layout, palette, data, output); break;
	case PALETTE4:  decodeFormat<PALETTE4 >(layout, palette, data, output); break;
	case BRG233:    decodeFormat<BRG233   >(layout, palette, data, output); break;
	case BRGX5551:  decodeFormat<BRGX5551 >(layout, palette, data, output); break;
	case BRGYS5551: decodeFormat<BRGYS5551>(layout, palette, data, output); break;
	case BGR565:    decodeFormat<BGR565   >(layout, palette, data, output); break;
	case RGB565:    decodeFormat<RGB565   >(layout, palette, data, output); break;
	case RGB888:    decodeFormat<RGB888   >(layout, palette, data, output); break;
	case BGRA8888:  decodeFormat<BGRA8888 >(layout, palette, data, output); break;
	case RGBA8888:  decodeFormat<RGBA8888 >(layout, palette, data, output); break;
	default: UNREACHABLE;
	}
}

} // namespace openmsx::BitmapDecoder
