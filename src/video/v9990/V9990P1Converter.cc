#include "V9990P1Converter.hh"
#include "V9990.hh"
#include "V9990VRAM.hh"
#include "MemoryOps.hh"
#include "ScopedAssign.hh"
#include "optional.hh"
#include "build-info.hh"
#include "components.hh"
#include <cassert>
#include <algorithm>
#include <cstdint>

namespace openmsx {

template<typename Pixel>
V9990P1Converter<Pixel>::V9990P1Converter(V9990& vdp_, const Pixel* palette64_)
	: vdp(vdp_), vram(vdp.getVRAM())
	, palette64(palette64_)
{
}

template<bool ALIGNED>
static unsigned getPatternAddress(
	V9990VRAM& vram, unsigned nameAddr, unsigned patternBase, unsigned x, unsigned y)
{
	assert(!ALIGNED || ((x & 7) == 0));
	unsigned patternNum = (vram.readVRAMP1(nameAddr + 0) +
	                       vram.readVRAMP1(nameAddr + 1) * 256) & 0x1FFF;
	unsigned x2 = (patternNum % 32) * 4 + (ALIGNED ? 0 : ((x & 7) / 2));
	unsigned y2 = (patternNum / 32) * 1024 + y;
	return patternBase + y2 + x2;
}

static unsigned nextNameAddr(unsigned addr)
{
	return (addr & ~127) | ((addr + 2) & 127);
}

template<bool BACKGROUND, typename Pixel>
static void draw1(const Pixel* palette, Pixel* __restrict buffer, byte* __restrict info, size_t p)
{
	if (BACKGROUND) {
		*buffer = palette[p];
	} else {
		*info = bool(p);
		if (p) *buffer = palette[p];
	}
}

template<bool BACKGROUND, bool CHECK_WIDTH, typename Pixel>
static void draw2(
	V9990VRAM& vram, const Pixel* palette, Pixel* __restrict& buffer, byte* __restrict& info,
	unsigned& address, int& width)
{
	byte data = vram.readVRAMP1(address++);
	draw1<BACKGROUND>(palette, buffer + 0, info + 0, data >> 4);
	if (!CHECK_WIDTH || (width != 1)) {
		draw1<BACKGROUND>(palette, buffer + 1, info + 1, data & 0x0F);
	}
	width  -= 2;
	buffer += 2;
	info   += 2;
}

template<bool BACKGROUND, typename Pixel>
static void renderPattern2(
	V9990VRAM& vram, Pixel* __restrict buffer, byte* __restrict info,
	int width, unsigned x, unsigned y,
	unsigned nameTable, unsigned patternBase, const Pixel* palette)
{
	assert(x < 512);
	if (width == 0) return;

	unsigned nameAddr = nameTable + (((y / 8) * 64 + (x / 8)) * 2);
	y = (y & 7) * 128;

	if (x & 7) {
		unsigned address = getPatternAddress<false>(vram, nameAddr, patternBase, x, y);
		if (x & 1) {
			byte data = vram.readVRAMP1(address++);
			draw1<BACKGROUND>(palette, buffer, info, data & 0x0F);
			++x;
			++buffer;
			--width;
		}
		while ((x & 7) && (width > 0)) {
			draw2<BACKGROUND, true>(vram, palette, buffer, info, address, width);
			x += 2;
		}
		nameAddr = nextNameAddr(nameAddr);
	}
	assert((x & 7) == 0 || (width <= 0));
	while (width & ~7) {
		unsigned address = getPatternAddress<true>(vram, nameAddr, patternBase, x, y);
		draw2<BACKGROUND, false>(vram, palette, buffer, info, address, width);
		draw2<BACKGROUND, false>(vram, palette, buffer, info, address, width);
		draw2<BACKGROUND, false>(vram, palette, buffer, info, address, width);
		draw2<BACKGROUND, false>(vram, palette, buffer, info, address, width);
		nameAddr = nextNameAddr(nameAddr);
	}
	assert(width < 8);
	if (width > 0) {
		unsigned address = getPatternAddress<true>(vram, nameAddr, patternBase, x, y);
		do {
			draw2<BACKGROUND, true>(vram, palette, buffer, info, address, width);
		} while (width > 0);
	}
}

template<bool BACKGROUND, typename Pixel>
static void renderPattern(
	V9990VRAM& vram, Pixel* buffer, byte* info, Pixel bgcol, unsigned width1, unsigned width2,
	unsigned displayAX, unsigned displayAY, unsigned nameA, unsigned patternA, const Pixel* palA,
	unsigned displayBX, unsigned displayBY, unsigned nameB, unsigned patternB, const Pixel* palB)
{
	optional<ScopedAssign<Pixel>> col0A, col0B; // optimized away for '!BACKGROUND'
	if (BACKGROUND) {
		// Speedup drawing by temporarily replacing palette index 0.
		// Only allowed when 'palA' and 'palB' either fully overlap or are disjuct.
		col0A.emplace(const_cast<Pixel*>(palA)[0], bgcol);
		col0B.emplace(const_cast<Pixel*>(palB)[0], bgcol);
	}

	renderPattern2<BACKGROUND>(
		vram, buffer, info, width1,
		displayAX, displayAY, nameA, patternA, palA);

	buffer += width1;
	info   += width1;
	width2 -= width1;
	displayBX = (displayBX + width1) & 511;

	renderPattern2<BACKGROUND>(
		vram, buffer, info, width2,
		displayBX, displayBY, nameB, patternB, palB);
}

static void determineVisibleSprites(
	V9990VRAM& vram, int* __restrict visibleSprites, unsigned displayY)
{
	constexpr unsigned spriteTable = 0x3FE00;

	int index = 0;
	int index_max = 16;
	for (unsigned sprite = 0; sprite < 125; ++sprite) {
		unsigned spriteInfo = spriteTable + 4 * sprite;
		byte spriteY = vram.readVRAMP1(spriteInfo) + 1;
		byte posY = displayY - spriteY;
		if (posY < 16) {
			byte attr = vram.readVRAMP1(spriteInfo + 3);
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
}

template<typename Pixel>
static void renderSprites(
	V9990VRAM& vram, unsigned spritePatternTable, const Pixel* palette64,
	Pixel* __restrict buffer, byte* __restrict info,
	int displayX, int displayEnd, unsigned displayY,
	const int* __restrict visibleSprites)
{
	static const unsigned spriteTable = 0x3FE00;

	for (unsigned sprite = 0; visibleSprites[sprite] != -1; ++sprite) {
		unsigned addr = spriteTable + 4 * visibleSprites[sprite];
		byte spriteAttr = vram.readVRAMP1(addr + 3);
		bool front = (spriteAttr & 0x20) == 0;
		byte level = front ? 2 : 1;
		int spriteX = vram.readVRAMP1(addr + 2);
		spriteX += 256 * (spriteAttr & 0x03);
		if (spriteX > 1008) spriteX -= 1024; // hack X coord into -16..1008
		byte spriteY  = vram.readVRAMP1(addr + 0);
		byte spriteNo = vram.readVRAMP1(addr + 1);
		spriteY = displayY - (spriteY + 1);
		unsigned patAddr = spritePatternTable
		     + (128 * ((spriteNo & 0xF0) + spriteY))
		     + (  8 *  (spriteNo & 0x0F));
		const Pixel* palette = palette64 + ((spriteAttr >> 2) & 0x30);
		for (int x = 0; x < 16; x +=2) {
			auto draw = [&](int xPos, size_t p) {
				if ((displayX <= xPos) && (xPos < displayEnd)) {
					size_t xx = xPos - displayX;
					if (p) {
						if (info[xx] < level) {
							buffer[xx] = palette[p];
						}
						info[xx] = 2; // also if back-sprite is behind foreground
					}
				}
			};
			byte data = vram.readVRAMP1(patAddr++);
			draw(spriteX + x + 0, data >> 4);
			draw(spriteX + x + 1, data & 0x0F);
		}
	}
}

template<typename Pixel>
void V9990P1Converter<Pixel>::convertLine(
	Pixel* linePtr, unsigned displayX, unsigned displayWidth,
	unsigned displayY, unsigned displayYA, unsigned displayYB,
	bool drawSprites)
{
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

	// background + backdrop color
	Pixel bgcol = palette64[vdp.getBackDropColor()];
	byte offset = vdp.getPaletteOffset();
	const Pixel* palA = palette64 + ((offset & 0x03) << 4);
	const Pixel* palB = palette64 + ((offset & 0x0C) << 2);
	renderPattern<true>(vram, linePtr, nullptr, bgcol, end1, displayWidth,
	                    displayBX, displayBY, 0x7E000, 0x40000, palB,
	                    displayAX, displayAY, 0x7C000, 0x00000, palA);

	// foreground + fill-in 'info'
	assert(displayWidth <= 256);
	byte info[256]; // left uninitialized
	                // 0->background, 1->foreground, 2->sprite (front or back)
	renderPattern<false>(vram, linePtr, info, bgcol, end1, displayWidth,
	                     displayAX, displayAY, 0x7C000, 0x00000, palA,
	                     displayBX, displayBY, 0x7E000, 0x40000, palB);

	// combined back+front sprite plane
	if (drawSprites) {
		int visibleSprites[16 + 1];
		determineVisibleSprites(vram, visibleSprites, displayY);
		unsigned spritePatternTable = vdp.getSpritePatternAddress(P1);
		renderSprites(vram, spritePatternTable, palette64,
		              linePtr, info, displayX, displayEnd, displayY,
		              visibleSprites);
	}
}

// Force template instantiation
#if HAVE_16BPP
template class V9990P1Converter<uint16_t>;
#endif
#if HAVE_32BPP || COMPONENT_GL
template class V9990P1Converter<uint32_t>;
#endif

} // namespace openmsx
