#include "V9990P2Converter.hh"
#include "V9990VRAM.hh"
#include "V9990.hh"
#include "MemoryOps.hh"
#include "build-info.hh"
#include "components.hh"
#include <algorithm>
#include <cstdint>

namespace openmsx {

template <class Pixel>
V9990P2Converter<Pixel>::V9990P2Converter(V9990& vdp_, const Pixel* palette64_)
	: vdp(vdp_), vram(vdp.getVRAM()), palette64(palette64_)
{
}

template <class Pixel>
void V9990P2Converter<Pixel>::convertLine(
	Pixel* linePtr, unsigned displayX, unsigned displayWidth,
	unsigned displayY, unsigned displayYA, bool drawSprites)
{
	unsigned displayAX = (displayX + vdp.getScrollAX()) & 1023;

	unsigned scrollY = vdp.getScrollAY();
	unsigned rollMask = vdp.getRollMask(0x1FF);
	unsigned scrollYBase = scrollY & ~rollMask & 0x1FF;
	unsigned displayAY = scrollYBase + ((displayYA + scrollY) & rollMask);

	unsigned displayEnd = displayX + displayWidth;

	// backdrop color
	Pixel bgcol = palette64[vdp.getBackDropColor()];
	MemoryOps::MemSet<Pixel> memset;
	memset(linePtr, displayWidth, bgcol);

	// back sprite plane
	int visibleSprites[16 + 1];
	if (drawSprites) {
		determineVisibleSprites(visibleSprites, displayY);
		renderSprites(linePtr, displayX, displayEnd, displayY,
		              visibleSprites, false);
	}

	// image plane
	byte pal = (vdp.getPaletteOffset() & 0x03) << 4;
	renderPattern(linePtr, displayWidth, displayAX, displayAY, pal);

	// front sprite plane
	if (drawSprites) {
		renderSprites(linePtr, displayX, displayEnd, displayY,
		              visibleSprites, true);
	}
}

template <class Pixel>
void V9990P2Converter<Pixel>::renderPattern(
	Pixel* __restrict buffer, unsigned width, unsigned x, unsigned y,
	byte pal)
{
	static const unsigned patternTable = 0x00000;
	static const unsigned nameTable    = 0x7C000;

	x &= 1023;
	const Pixel* palette = palette64 + pal;

	unsigned nameAddr = nameTable + (((y / 8) * 128 + (x / 8)) * 2);
	y = (y & 7) * 256;

	if (x & 7) {
		unsigned patternNum = (vram.readVRAMDirect(nameAddr + 0) +
		                       vram.readVRAMDirect(nameAddr + 1) * 256) & 0x1FFF;
		unsigned x2 = (patternNum % 64) * 4 + ((x & 7) / 2);
		unsigned y2 = (patternNum / 64) * 2048 + y;
		unsigned address = patternTable + y2 + x2;
		byte data = vram.readVRAMBx(address);

		while ((x & 7) && width) {
			byte p;
			if (x & 1) {
				p = data & 0x0F;
				++address;
			} else {
				data = vram.readVRAMBx(address);
				p = data >> 4;
			}
			if (p) buffer[0] = palette[p];
			++x;
			++buffer;
			--width;
		}
		nameAddr = (nameAddr & ~255) | ((nameAddr + 2) & 255);
	}
	assert((x & 7) == 0 || (width == 0));
	while (width & ~7) {
		unsigned patternNum = (vram.readVRAMDirect(nameAddr + 0) +
		                       vram.readVRAMDirect(nameAddr + 1) * 256) & 0x1FFF;
		unsigned x2 = (patternNum % 64) * 4;
		unsigned y2 = (patternNum / 64) * 2048 + y;
		unsigned address = patternTable + y2 + x2;

		byte data0 = vram.readVRAMBx(address + 0);
		byte p0 = data0 >> 4;
		if (p0) buffer[0] = palette[p0];
		byte p1 = data0 & 0x0F;
		if (p1) buffer[1] = palette[p1];

		byte data1 = vram.readVRAMBx(address + 1);
		byte p2 = data1 >> 4;
		if (p2) buffer[2] = palette[p2];
		byte p3 = data1 & 0x0F;
		if (p3) buffer[3] = palette[p3];

		byte data2 = vram.readVRAMBx(address + 2);
		byte p4 = data2 >> 4;
		if (p4) buffer[4] = palette[p4];
		byte p5 = data2 & 0x0F;
		if (p5) buffer[5] = palette[p5];

		byte data3 = vram.readVRAMBx(address + 3);
		byte p6 = data3 >> 4;
		if (p6) buffer[6] = palette[p6];
		byte p7 = data3 & 0x0F;
		if (p7) buffer[7] = palette[p7];

		width -= 8;
		buffer += 8;
		nameAddr = (nameAddr & ~255) | ((nameAddr + 2) & 255);
	}
	assert(width < 8);
	if (width) {
		unsigned patternNum = (vram.readVRAMDirect(nameAddr + 0) +
		                       vram.readVRAMDirect(nameAddr + 1) * 256) & 0x1FFF;
		unsigned x2 = (patternNum % 64) * 4;
		unsigned y2 = (patternNum / 64) * 2048 + y;
		unsigned address = patternTable + y2 + x2;
		do {
			byte data = vram.readVRAMBx(address++);
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
void V9990P2Converter<Pixel>::determineVisibleSprites(
	int* __restrict visibleSprites, int displayY)
{
	static const unsigned spriteTable = 0x3FE00;

	int index = 0;
	int index_max = 16;
	for (int sprite = 0; sprite < 125; ++sprite) {
		int spriteInfo = spriteTable + 4 * sprite;
		byte spriteY = vram.readVRAMDirect(spriteInfo) + 1;
		byte posY = displayY - spriteY;
		if (posY < 16) {
			byte attr = vram.readVRAMDirect(spriteInfo + 3);
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
void V9990P2Converter<Pixel>::renderSprites(
	Pixel* __restrict buffer, int displayX, int displayEnd, unsigned displayY,
	const int* __restrict visibleSprites, bool front)
{
	static const unsigned spriteTable = 0x3FE00;
	unsigned spritePatternTable = vdp.getSpritePatternAddress(P1); // TODO P2 ???

	for (unsigned sprite = 0; visibleSprites[sprite] != -1; ++sprite) {
		unsigned addr = spriteTable + 4 * visibleSprites[sprite];
		byte spriteAttr = vram.readVRAMDirect(addr + 3);
		if (front ^ !!(spriteAttr & 0x20)) {
			int spriteX = vram.readVRAMDirect(addr + 2);
			spriteX += 256 * (spriteAttr & 0x03);
			if (spriteX > 1008) spriteX -= 1024; // hack X coord into -16..1008
			byte spriteY  = vram.readVRAMDirect(addr + 0);
			byte spriteNo = vram.readVRAMDirect(addr + 1);
			spriteY = displayY - (spriteY + 1);
			unsigned patAddr = spritePatternTable
			     + (256 * (((spriteNo & 0xE0) >> 1) + spriteY))
			     + (  8 *  (spriteNo & 0x1F));
			const Pixel* palette = palette64 + ((spriteAttr >> 2) & 0x30);
			for (int x = 0; x < 16; x +=2) {
				byte data = vram.readVRAMBx(patAddr++);
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
template class V9990P2Converter<uint16_t>;
#endif
#if HAVE_32BPP || COMPONENT_GL
template class V9990P2Converter<uint32_t>;
#endif

} // namespace openmsx
