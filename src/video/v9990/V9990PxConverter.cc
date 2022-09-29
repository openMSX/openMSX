#include "V9990PxConverter.hh"
#include "V9990.hh"
#include "V9990VRAM.hh"
#include "ScopedAssign.hh"
#include "build-info.hh"
#include "components.hh"
#include "ranges.hh"
#include <array>
#include <cassert>
#include <algorithm>
#include <cstdint>
#include <optional>

namespace openmsx {

template<std::unsigned_integral Pixel>
V9990P1Converter<Pixel>::V9990P1Converter(V9990& vdp_, std::span<const Pixel, 64> palette64_)
	: vdp(vdp_), vram(vdp.getVRAM())
	, palette64(palette64_)
{
}

template<std::unsigned_integral Pixel>
V9990P2Converter<Pixel>::V9990P2Converter(V9990& vdp_, std::span<const Pixel, 64> palette64_)
	: vdp(vdp_), vram(vdp.getVRAM()), palette64(palette64_)
{
}

struct P1Policy {
	static byte readNameTable(V9990VRAM& vram, unsigned addr) {
		return vram.readVRAMP1(addr);
	}
	static byte readPatternTable(V9990VRAM& vram, unsigned addr) {
		return vram.readVRAMP1(addr);
	}
	static byte readSpriteAttr(V9990VRAM& vram, unsigned addr) {
		return vram.readVRAMP1(addr);
	}
	static unsigned spritePatOfst(byte spriteNo, byte spriteY) {
		return (128 * ((spriteNo & 0xF0) + spriteY))
		     + (  8 *  (spriteNo & 0x0F));
	}
	static constexpr unsigned SCREEN_WIDTH = 256;
	static constexpr unsigned IMAGE_WIDTH = 2 * SCREEN_WIDTH;
	static constexpr unsigned NAME_CHARS = IMAGE_WIDTH / 8;
	static constexpr unsigned PATTERN_CHARS = SCREEN_WIDTH / 8;
};
struct P1BackgroundPolicy : P1Policy {
	template<std::unsigned_integral Pixel> static void draw1(
		std::span<const Pixel, 16> palette, Pixel* __restrict buffer,
		byte* __restrict /*info*/, size_t p)
	{
		*buffer = palette[p];
	}
	static constexpr bool DRAW_BACKDROP = true;
};
struct P1ForegroundPolicy : P1Policy {
	template<std::unsigned_integral Pixel> static void draw1(
		std::span<const Pixel, 16> palette, Pixel* __restrict buffer,
		byte* __restrict info, size_t p)
	{
		*info = bool(p);
		if (p) *buffer = palette[p];
	}
	static constexpr bool DRAW_BACKDROP = false;
};
struct P2Policy {
	static byte readNameTable(V9990VRAM& vram, unsigned addr) {
		return vram.readVRAMDirect(addr);
	}
	static byte readPatternTable(V9990VRAM& vram, unsigned addr) {
		return vram.readVRAMBx(addr);
	}
	static byte readSpriteAttr(V9990VRAM& vram, unsigned addr) {
		return vram.readVRAMDirect(addr);
	}
	static unsigned spritePatOfst(byte spriteNo, byte spriteY) {
		return (256 * (((spriteNo & 0xE0) >> 1) + spriteY))
		     + (  8 *  (spriteNo & 0x1F));
	}
	template<std::unsigned_integral Pixel> static void draw1(
		std::span<const Pixel, 16> palette, Pixel* __restrict buffer,
		byte* __restrict info, size_t p)
	{
		*info = bool(p);
		*buffer = palette[p];
	}
	static constexpr bool DRAW_BACKDROP = true;
	static constexpr unsigned SCREEN_WIDTH = 512;
	static constexpr unsigned IMAGE_WIDTH = 2 * SCREEN_WIDTH;
	static constexpr unsigned NAME_CHARS = IMAGE_WIDTH / 8;
	static constexpr unsigned PATTERN_CHARS = SCREEN_WIDTH / 8;
};

template<typename Policy, bool ALIGNED>
static unsigned getPatternAddress(
	V9990VRAM& vram, unsigned nameAddr, unsigned patternBase, unsigned x, unsigned y)
{
	assert(!ALIGNED || ((x & 7) == 0));
	unsigned patternNum = (Policy::readNameTable(vram, nameAddr + 0) +
	                       Policy::readNameTable(vram, nameAddr + 1) * 256) & 0x1FFF;
	constexpr auto PATTERN_PITCH = Policy::PATTERN_CHARS * 8 * (8 / 2);
	unsigned x2 = (patternNum % Policy::PATTERN_CHARS) * 4 + (ALIGNED ? 0 : ((x & 7) / 2));
	unsigned y2 = (patternNum / Policy::PATTERN_CHARS) * PATTERN_PITCH + y;
	return patternBase + y2 + x2;
}

template<typename Policy>
static constexpr unsigned nextNameAddr(unsigned addr)
{
	constexpr auto MASK = (2 * Policy::NAME_CHARS) - 1;
	return (addr & ~MASK) | ((addr + 2) & MASK);
}

template<typename Policy, bool CHECK_WIDTH, std::unsigned_integral Pixel>
static void draw2(
	V9990VRAM& vram, std::span<const Pixel, 16> palette, Pixel* __restrict& buffer, byte* __restrict& info,
	unsigned& address, int& width)
{
	byte data = Policy::readPatternTable(vram, address++);
	Policy::draw1(palette, buffer + 0, info + 0, data >> 4);
	if (!CHECK_WIDTH || (width != 1)) {
		Policy::draw1(palette, buffer + 1, info + 1, data & 0x0F);
	}
	width  -= 2;
	buffer += 2;
	info   += 2;
}

template<typename Policy, std::unsigned_integral Pixel>
static void renderPattern(
	V9990VRAM& vram, Pixel* __restrict buffer, std::span<byte> info_,
	Pixel bgCol, unsigned x, unsigned y,
	unsigned nameTable, unsigned patternBase,
	std::span<const Pixel, 16> palette0, std::span<const Pixel, 16> palette1)
{
	assert(x < Policy::IMAGE_WIDTH);
	int width = info_.size();
	if (width == 0) return;
	byte* info = info_.data();

	std::optional<ScopedAssign<Pixel>> col0, col1; // optimized away when not used
	if constexpr (Policy::DRAW_BACKDROP) {
		// Speedup drawing by temporarily replacing palette index 0.
		// OK because palette0 and palette1 never partially overlap, IOW either:
		// - palette0 == palette1           (fully overlap)
		// - abs(palette0 - palette1) >= 16 (no overlap at all)
		col0.emplace(*const_cast<Pixel*>(palette0.data()), bgCol);
		col1.emplace(*const_cast<Pixel*>(palette1.data()), bgCol);
	}

	unsigned nameAddr = nameTable + (((y / 8) * Policy::NAME_CHARS + (x / 8)) * 2);
	y = (y & 7) * Policy::NAME_CHARS * 2;

	if (x & 7) {
		unsigned address = getPatternAddress<Policy, false>(vram, nameAddr, patternBase, x, y);
		if (x & 1) {
			byte data = Policy::readPatternTable(vram, address);
			Policy::draw1((address & 1) ? palette1 : palette0, buffer, info, data & 0x0F);
			++address;
			++x;
			++buffer;
			++info;
			--width;
		}
		while ((x & 7) && (width > 0)) {
			draw2<Policy, true>(vram, (address & 1) ? palette1 : palette0, buffer, info, address, width);
			x += 2;
		}
		nameAddr = nextNameAddr<Policy>(nameAddr);
	}
	assert((x & 7) == 0 || (width <= 0));
	while ((width & ~7) > 0) {
		unsigned address = getPatternAddress<Policy, true>(vram, nameAddr, patternBase, x, y);
		draw2<Policy, false>(vram, palette0, buffer, info, address, width);
		draw2<Policy, false>(vram, palette1, buffer, info, address, width);
		draw2<Policy, false>(vram, palette0, buffer, info, address, width);
		draw2<Policy, false>(vram, palette1, buffer, info, address, width);
		nameAddr = nextNameAddr<Policy>(nameAddr);
	}
	assert(width < 8);
	if (width > 0) {
		unsigned address = getPatternAddress<Policy, true>(vram, nameAddr, patternBase, x, y);
		do {
			draw2<Policy, true>(vram, (address & 1) ? palette1 : palette0, buffer, info, address, width);
		} while (width > 0);
	}
}

template<typename Policy, std::unsigned_integral Pixel> // only used for P1
static void renderPattern2(
	V9990VRAM& vram, Pixel* buffer, std::span<byte, 256> info, Pixel bgCol, unsigned width1, unsigned width2,
	unsigned displayAX, unsigned displayAY, unsigned nameA, unsigned patternA, std::span<const Pixel, 16> palA,
	unsigned displayBX, unsigned displayBY, unsigned nameB, unsigned patternB, std::span<const Pixel, 16> palB)
{
	renderPattern<Policy>(
		vram, buffer, subspan(info, 0, width1), bgCol,
		displayAX, displayAY, nameA, patternA, palA, palA);

	buffer += width1;
	width2 -= width1;
	displayBX = (displayBX + width1) & 511;

	renderPattern<Policy>(
		vram, buffer, subspan(info, width1, width2), bgCol,
		displayBX, displayBY, nameB, patternB, palB, palB);
}

template<typename Policy, std::unsigned_integral Pixel>
static void renderSprites(
	V9990VRAM& vram, unsigned spritePatternTable, std::span<const Pixel, 64> palette64,
	Pixel* __restrict buffer, std::span<byte> info,
	int displayX, int displayEnd, unsigned displayY)
{
	constexpr unsigned spriteTable = 0x3FE00;

	// determine visible sprites
	std::array<int, 16 + 1> visibleSprites;
	int index = 0;
	int index_max = 16;
	for (auto sprite : xrange(125)) {
		unsigned spriteInfo = spriteTable + 4 * sprite;
		byte spriteY = Policy::readSpriteAttr(vram, spriteInfo) + 1;
		byte posY = displayY - spriteY;
		if (posY < 16) {
			byte attr = Policy::readSpriteAttr(vram, spriteInfo + 3);
			if (attr & 0x10) {
				// Invisible sprites do contribute towards the
				// 16-sprites-per-line limit.
				index_max--;
			} else {
				visibleSprites[index++] = sprite;
			}
			if (index == index_max) break;
		}
	}
	visibleSprites[index] = -1;

	// actually draw sprites
	for (unsigned sprite = 0; visibleSprites[sprite] != -1; ++sprite) {
		unsigned addr = spriteTable + 4 * visibleSprites[sprite];
		byte spriteAttr = Policy::readSpriteAttr(vram, addr + 3);
		bool front = (spriteAttr & 0x20) == 0;
		byte level = front ? 2 : 1;
		int spriteX = Policy::readSpriteAttr(vram, addr + 2);
		spriteX += 256 * (spriteAttr & 0x03);
		if (spriteX > 1008) spriteX -= 1024; // hack X coord into -16..1008
		byte spriteY  = Policy::readSpriteAttr(vram, addr + 0);
		byte spriteNo = Policy::readSpriteAttr(vram, addr + 1);
		spriteY = displayY - (spriteY + 1);
		unsigned patAddr = spritePatternTable + Policy::spritePatOfst(spriteNo, spriteY);
		auto palette16 = subspan<16>(palette64, (spriteAttr >> 2) & 0x30);
		for (int x = 0; x < 16; x +=2) {
			auto draw = [&](int xPos, size_t p) {
				if ((displayX <= xPos) && (xPos < displayEnd)) {
					size_t xx = xPos - displayX;
					if (p) {
						if (info[xx] < level) {
							buffer[xx] = palette16[p];
						}
						info[xx] = 2; // also if back-sprite is behind foreground
					}
				}
			};
			byte data = Policy::readPatternTable(vram, patAddr++);
			draw(spriteX + x + 0, data >> 4);
			draw(spriteX + x + 1, data & 0x0F);
		}
	}
}

template<std::unsigned_integral Pixel>
void V9990P1Converter<Pixel>::convertLine(
	std::span<Pixel> buf, unsigned displayX, unsigned displayY,
	unsigned displayYA, unsigned displayYB, bool drawSprites)
{
	Pixel* __restrict linePtr = buf.data();
	unsigned displayWidth = buf.size();

	unsigned prioX = vdp.getPriorityControlX();
	unsigned prioY = vdp.getPriorityControlY();
	if (displayY >= prioY) prioX = 0;

	unsigned displayAX = (displayX + vdp.getScrollAX()) & 511;
	unsigned displayBX = (displayX + vdp.getScrollBX()) & 511;

	// Configurable 'roll' only applies to layer A.
	// Layer B always rolls at 512 lines.
	unsigned rollMask = vdp.getRollMask(0x1FF);
	unsigned scrollAY = vdp.getScrollAY();
	unsigned scrollBY = vdp.getScrollBY();
	unsigned scrollAYBase = scrollAY & ~rollMask & 0x1FF;
	unsigned displayAY = scrollAYBase + ((displayYA + scrollAY) & rollMask);
	unsigned displayBY =                 (displayYB + scrollBY) & 0x1FF;

	unsigned displayEnd = displayX + displayWidth;
	unsigned end1 = std::max<int>(0, std::min<int>(prioX, displayEnd) - displayX);

	std::array<byte, 256> info; // filled in later: 0->background, 1->foreground, 2->sprite (front or back)

	// background + backdrop color
	Pixel bgCol = palette64[vdp.getBackDropColor()];
	byte offset = vdp.getPaletteOffset();
	auto palA = subspan<16>(palette64, (offset & 0x03) << 4);
	auto palB = subspan<16>(palette64, (offset & 0x0C) << 2);
	renderPattern2<P1BackgroundPolicy>( // does not yet fill-in 'info'
		vram, linePtr, /*dummy*/info, bgCol, end1, displayWidth,
		displayBX, displayBY, 0x7E000, 0x40000, palB,
		displayAX, displayAY, 0x7C000, 0x00000, palA);

	// foreground + fill-in 'info'
	assert(displayWidth <= 256);
	renderPattern2<P1ForegroundPolicy>( // fills 'info' with '0' or '1'
		vram, linePtr, info, bgCol, end1, displayWidth,
		displayAX, displayAY, 0x7C000, 0x00000, palA,
		displayBX, displayBY, 0x7E000, 0x40000, palB);

	// combined back+front sprite plane
	if (drawSprites) {
		unsigned spritePatternTable = vdp.getSpritePatternAddress(P1);
		renderSprites<P1Policy>( // uses and updates 'info'
			vram, spritePatternTable, palette64,
			linePtr, info, displayX, displayEnd, displayY);
	}
}

template<std::unsigned_integral Pixel>
void V9990P2Converter<Pixel>::convertLine(
	std::span<Pixel> buf, unsigned displayX, unsigned displayY,
	unsigned displayYA, bool drawSprites)
{
	Pixel* __restrict linePtr = buf.data();
	unsigned displayWidth = buf.size();

	unsigned displayAX = (displayX + vdp.getScrollAX()) & 1023;

	unsigned scrollY = vdp.getScrollAY();
	unsigned rollMask = vdp.getRollMask(0x1FF);
	unsigned scrollYBase = scrollY & ~rollMask & 0x1FF;
	unsigned displayAY = scrollYBase + ((displayYA + scrollY) & rollMask);

	unsigned displayEnd = displayX + displayWidth;

	// image plane + backdrop color + fill-in 'info'
	assert(displayWidth <= 512);
	std::array<byte, 512> info; // filled in later: 0->background, 1->foreground, 2->sprite (front or back)
	Pixel bgCol = palette64[vdp.getBackDropColor()];
	byte offset = vdp.getPaletteOffset();
	auto palette0 = subspan<16>(palette64, (offset & 0x03) << 4);
	auto palette1 = subspan<16>(palette64, (offset & 0x0C) << 2);
	renderPattern<P2Policy>(
		vram, linePtr, subspan(info, 0, displayWidth), bgCol,
		displayAX, displayAY, 0x7C000, 0x00000, palette0, palette1);

	// combined back+front sprite plane
	if (drawSprites) {
		unsigned spritePatternTable = vdp.getSpritePatternAddress(P2);
		renderSprites<P2Policy>(
			vram, spritePatternTable, palette64,
			linePtr, info, displayX, displayEnd, displayY);
	}
}

// Force template instantiation
#if HAVE_16BPP
template class V9990P1Converter<uint16_t>;
template class V9990P2Converter<uint16_t>;
#endif
#if HAVE_32BPP || COMPONENT_GL
template class V9990P1Converter<uint32_t>;
template class V9990P2Converter<uint32_t>;
#endif

} // namespace openmsx
