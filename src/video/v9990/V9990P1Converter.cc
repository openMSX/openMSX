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

template <class Pixel>
V9990P1Converter<Pixel>::V9990P1Converter(V9990& vdp_, const Pixel* palette64_)
	: vdp(vdp_), vram(vdp.getVRAM())
	, palette64(palette64_)
{
}

template <class Pixel>
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
	byte palA = (offset & 0x03) << 4;
	byte palB = (offset & 0x0C) << 2;
	renderPattern(linePtr, end1, displayWidth,
	              displayBX, displayBY, 0x7E000, 0x40000, palB,
	              displayAX, displayAY, 0x7C000, 0x00000, palA);

	// back sprite plane
	int visibleSprites[16 + 1];
	if (drawSprites) {
		determineVisibleSprites(visibleSprites, displayY);
		renderSprites(linePtr, displayX, displayEnd, displayY,
		              visibleSprites, false);
	}

	// foreground
	renderPattern(linePtr, end1, displayWidth,
	              displayAX, displayAY, 0x7C000, 0x00000, palA,
	              displayBX, displayBY, 0x7E000, 0x40000, palB);

	// front sprite plane
	if (drawSprites) {
		renderSprites(linePtr, displayX, displayEnd, displayY,
		              visibleSprites, true);
	}
}

template <class Pixel>
void V9990P1Converter<Pixel>::renderPattern(
	Pixel* buffer, unsigned width1, unsigned width2,
	unsigned displayAX, unsigned displayAY, unsigned nameA,
	unsigned patternA, byte palA,
	unsigned displayBX, unsigned displayBY, unsigned nameB,
	unsigned patternB, byte palB)
{
	renderPattern2(&buffer[0],      width1, displayAX, displayAY,
	               nameA, patternA, palA);
	renderPattern2(&buffer[width1], width2 - width1, displayBX + width1, displayBY,
	               nameB, patternB, palB);
}

template <class Pixel>
void V9990P1Converter<Pixel>::renderPattern2(
	Pixel* __restrict buffer, unsigned width, unsigned x, unsigned y,
	unsigned nameTable, unsigned patternTable, byte pal)
{
	x &= 511;
	const Pixel* palette = palette64 + pal;

	unsigned nameAddr = nameTable + (((y / 8) * 64 + (x / 8)) * 2);
	y = (y & 7) * 128;

	if (x & 7) {
		unsigned patternNum = (vram.readVRAMP1(nameAddr + 0) +
		                       vram.readVRAMP1(nameAddr + 1) * 256) & 0x1FFF;
		unsigned x2 = (patternNum % 32) * 4 + ((x & 7) / 2);
		unsigned y2 = (patternNum / 32) * 1024 + y;
		unsigned address = patternTable + y2 + x2;
		byte data = vram.readVRAMP1(address);

		while ((x & 7) && width) {
			byte p;
			if (x & 1) {
				p = data & 0x0F;
				++address;
			} else {
				data = vram.readVRAMP1(address);
				p = data >> 4;
			}
			if (p) buffer[0] = palette[p];
			++x;
			++buffer;
			--width;
		}
		nameAddr = (nameAddr & ~127) | ((nameAddr + 2) & 127);
	}
	assert((x & 7) == 0 || (width == 0));
	while (width & ~7) {
		unsigned patternNum = (vram.readVRAMP1(nameAddr + 0) +
		                       vram.readVRAMP1(nameAddr + 1) * 256) & 0x1FFF;
		unsigned x2 = (patternNum % 32) * 4;
		unsigned y2 = (patternNum / 32) * 1024 + y;
		unsigned address = patternTable + y2 + x2;

		byte data0 = vram.readVRAMP1(address + 0);
		byte p0 = data0 >> 4;
		if (p0) buffer[0] = palette[p0];
		byte p1 = data0 & 0x0F;
		if (p1) buffer[1] = palette[p1];

		byte data1 = vram.readVRAMP1(address + 1);
		byte p2 = data1 >> 4;
		if (p2) buffer[2] = palette[p2];
		byte p3 = data1 & 0x0F;
		if (p3) buffer[3] = palette[p3];

		byte data2 = vram.readVRAMP1(address + 2);
		byte p4 = data2 >> 4;
		if (p4) buffer[4] = palette[p4];
		byte p5 = data2 & 0x0F;
		if (p5) buffer[5] = palette[p5];

		byte data3 = vram.readVRAMP1(address + 3);
		byte p6 = data3 >> 4;
		if (p6) buffer[6] = palette[p6];
		byte p7 = data3 & 0x0F;
		if (p7) buffer[7] = palette[p7];

		width -= 8;
		buffer += 8;
		nameAddr = (nameAddr & ~127) | ((nameAddr + 2) & 127);
	}
	assert(width < 8);
	if (width) {
		unsigned patternNum = (vram.readVRAMP1(nameAddr + 0) +
		                       vram.readVRAMP1(nameAddr + 1) * 256) & 0x1FFF;
		unsigned x2 = (patternNum % 32) * 4;
		unsigned y2 = (patternNum / 32) * 1024 + y;
		unsigned address = patternTable + y2 + x2;
		do {
			byte data = vram.readVRAMP1(address++);
			byte p0 = data >> 4;
			if (p0) buffer[0] = palette[p0];
			if (width != 1) {
				byte p1 = data & 0x0F;
				if (p1) buffer[1] = palette[p1];
			}
			width -= 2;
			buffer += 2;
		} while (int(width) > 0);
	}
}

template <class Pixel>
void V9990P1Converter<Pixel>::determineVisibleSprites(
	int* __restrict visibleSprites, unsigned displayY)
{
	static const unsigned spriteTable = 0x3FE00;

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

template <class Pixel>
void V9990P1Converter<Pixel>::renderSprites(
	Pixel* __restrict buffer, int displayX, int displayEnd, unsigned displayY,
	const int* __restrict visibleSprites, bool front)
{
	static const unsigned spriteTable = 0x3FE00;
	unsigned spritePatternTable = vdp.getSpritePatternAddress(P1);

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
				byte data = vram.readVRAMP1(patAddr++);
				int xPos = spriteX + x;
				if ((displayX <= xPos) && (xPos < displayEnd)) {
					byte p = data >> 4;
					if (p) buffer[xPos - displayX] = palette[p];
				}
				++xPos;
				if ((displayX <= xPos) && (xPos < displayEnd)) {
					byte p = data & 0x0F;
					if (p) buffer[xPos - displayX] = palette[p];
				}
			}
		}
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
