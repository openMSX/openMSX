#include "V9990P1Converter.hh"
#include "V9990.hh"
#include "V9990VRAM.hh"
#include "MemoryOps.hh"
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

template<typename Pixel>
static void draw1(const Pixel* palette, Pixel* __restrict buffer, size_t p)
{
	if (p) *buffer = palette[p];
}

template<bool CHECK_WIDTH, typename Pixel>
static void draw2(
	V9990VRAM& vram, const Pixel* palette, Pixel* __restrict& buffer,
	unsigned& address, int& width)
{
	byte data = vram.readVRAMP1(address++);
	draw1(palette, buffer + 0, data >> 4);
	if (!CHECK_WIDTH || (width != 1)) {
		draw1(palette, buffer + 1, data & 0x0F);
	}
	width  -= 2;
	buffer += 2;
}

template<typename Pixel>
static void renderPattern2(
	V9990VRAM& vram, Pixel* __restrict buffer, int width, unsigned x, unsigned y,
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
			draw1(palette, buffer, data & 0x0F);
			++x;
			++buffer;
			--width;
		}
		while ((x & 7) && (width > 0)) {
			draw2<true>(vram, palette, buffer, address, width);
			x += 2;
		}
		nameAddr = nextNameAddr(nameAddr);
	}
	assert((x & 7) == 0 || (width <= 0));
	while (width & ~7) {
		unsigned address = getPatternAddress<true>(vram, nameAddr, patternBase, x, y);
		draw2<false>(vram, palette, buffer, address, width);
		draw2<false>(vram, palette, buffer, address, width);
		draw2<false>(vram, palette, buffer, address, width);
		draw2<false>(vram, palette, buffer, address, width);
		nameAddr = nextNameAddr(nameAddr);
	}
	assert(width < 8);
	if (width) {
		unsigned address = getPatternAddress<true>(vram, nameAddr, patternBase, x, y);
		do {
			draw2<true>(vram, palette, buffer, address, width);
		} while (width > 0);
	}
}

template<typename Pixel>
static void renderPattern(
	V9990VRAM& vram, Pixel* buffer, unsigned width1, unsigned width2,
	unsigned displayAX, unsigned displayAY, unsigned nameA, unsigned patternA, const Pixel* palA,
	unsigned displayBX, unsigned displayBY, unsigned nameB, unsigned patternB, const Pixel* palB)
{
	renderPattern2(vram, buffer, width1, displayAX, displayAY,
	               nameA, patternA, palA);

	buffer += width1;
	width2 -= width1;
	displayBX = (displayBX + width1) & 511;

	renderPattern2(vram, buffer, width2, displayBX, displayBY,
	               nameB, patternB, palB);
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
	// draw sprites in reverse order
	std::reverse(visibleSprites, visibleSprites + index);
	visibleSprites[index] = -1;
}

template<typename Pixel>
static void renderSprites(
	V9990VRAM& vram, unsigned spritePatternTable, const Pixel* palette64,
	Pixel* __restrict buffer, int displayX, int displayEnd, unsigned displayY,
	const int* __restrict visibleSprites, bool front)
{
	static const unsigned spriteTable = 0x3FE00;

	for (unsigned sprite = 0; visibleSprites[sprite] != -1; ++sprite) {
		unsigned addr = spriteTable + 4 * visibleSprites[sprite];
		byte spriteAttr = vram.readVRAMP1(addr + 3);
		if (front ^ !!(spriteAttr & 0x20)) {
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
						if (p) buffer[xx] = palette[p];
					}
				};
				byte data = vram.readVRAMP1(patAddr++);
				draw(spriteX + x + 0, data >> 4);
				draw(spriteX + x + 1, data & 0x0F);
			}
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

	// back drop color
	Pixel bgcol = palette64[vdp.getBackDropColor()];
	MemoryOps::MemSet<Pixel> memset;
	memset(linePtr, displayWidth, bgcol);

	// background
	byte offset = vdp.getPaletteOffset();
	const Pixel* palA = palette64 + ((offset & 0x03) << 4);
	const Pixel* palB = palette64 + ((offset & 0x0C) << 2);
	renderPattern(vram, linePtr, end1, displayWidth,
	              displayBX, displayBY, 0x7E000, 0x40000, palB,
	              displayAX, displayAY, 0x7C000, 0x00000, palA);

	// back sprite plane
	int visibleSprites[16 + 1];
	unsigned spritePatternTable = vdp.getSpritePatternAddress(P1);
	if (drawSprites) {
		determineVisibleSprites(vram, visibleSprites, displayY);
		renderSprites(vram, spritePatternTable, palette64,
		              linePtr, displayX, displayEnd, displayY,
		              visibleSprites, false);
	}

	// foreground
	renderPattern(vram, linePtr, end1, displayWidth,
	              displayAX, displayAY, 0x7C000, 0x00000, palA,
	              displayBX, displayBY, 0x7E000, 0x40000, palB);

	// front sprite plane
	if (drawSprites) {
		renderSprites(vram, spritePatternTable, palette64,
		              linePtr, displayX, displayEnd, displayY,
		              visibleSprites, true);
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
