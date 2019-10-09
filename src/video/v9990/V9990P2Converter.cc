#include "V9990P2Converter.hh"
#include "V9990VRAM.hh"
#include "V9990.hh"
#include "ScopedAssign.hh"
#include "build-info.hh"
#include "components.hh"
#include <algorithm>
#include <cassert>
#include <cstdint>

namespace openmsx {

template<typename Pixel>
V9990P2Converter<Pixel>::V9990P2Converter(V9990& vdp_, const Pixel* palette64_)
	: vdp(vdp_), vram(vdp.getVRAM()), palette64(palette64_)
{
}

template<bool ALIGNED>
static unsigned getPatternAddress(
	V9990VRAM& vram, unsigned nameAddr, unsigned patternBase, unsigned x, unsigned y)
{
	assert(!ALIGNED || ((x & 7) == 0));
	unsigned patternNum = (vram.readVRAMDirect(nameAddr + 0) +
	                       vram.readVRAMDirect(nameAddr + 1) * 256) & 0x1FFF;
	unsigned x2 = (patternNum % 64) * 4 + (ALIGNED ? 0 : ((x & 7) / 2));
	unsigned y2 = (patternNum / 64) * 2048 + y;
	return patternBase + y2 + x2;
}

static unsigned nextNameAddr(unsigned addr)
{
	return (addr & ~255) | ((addr + 2) & 255);
}

template<typename Pixel>
static void draw1(const Pixel* palette, Pixel* __restrict buffer, byte* __restrict info, size_t p)
{
	*info = bool(p);
	*buffer = palette[p];
}

template<bool CHECK_WIDTH, typename Pixel>
static void draw2(
	V9990VRAM& vram, const Pixel* palette, Pixel* __restrict& buffer, byte* __restrict& info,
	unsigned& address, int& width)
{
	byte data = vram.readVRAMBx(address++);
	draw1(palette, buffer + 0, info + 0, data >> 4);
	if (!CHECK_WIDTH || (width != 1)) {
		draw1(palette, buffer + 1, info + 1, data & 0x0F);
	}
	width  -= 2;
	buffer += 2;
	info   += 2;
}

template<typename Pixel>
static void renderPattern2(
	V9990VRAM& vram, Pixel* __restrict buffer, byte* __restrict info,
	int width, unsigned x, unsigned y,
	unsigned nameTable, unsigned patternBase, const Pixel* palette)
{
	assert(x < 1024);
	if (width == 0) return;

	unsigned nameAddr = nameTable + (((y / 8) * 128 + (x / 8)) * 2);
	y = (y & 7) * 256;

	if (x & 7) {
		unsigned address = getPatternAddress<false>(vram, nameAddr, patternBase, x, y);
		if (x & 1) {
			byte data = vram.readVRAMBx(address++);
			draw1(palette, buffer, info, data & 0x0F);
			++x;
			++buffer;
			++info;
			--width;
		}
		while ((x & 7) && (width > 0)) {
			draw2<true>(vram, palette, buffer, info, address, width);
			x += 2;
		}
		nameAddr = nextNameAddr(nameAddr);
	}
	assert((x & 7) == 0 || (width <= 0));
	while (width & ~7) {
		unsigned address = getPatternAddress<true>(vram, nameAddr, patternBase, x, y);
		draw2<false>(vram, palette, buffer, info, address, width);
		draw2<false>(vram, palette, buffer, info, address, width);
		draw2<false>(vram, palette, buffer, info, address, width);
		draw2<false>(vram, palette, buffer, info, address, width);
		nameAddr = nextNameAddr(nameAddr);
	}
	assert(width < 8);
	if (width > 0) {
		unsigned address = getPatternAddress<true>(vram, nameAddr, patternBase, x, y);
		do {
			draw2<true>(vram, palette, buffer, info, address, width);
		} while (width > 0);
	}
}

template<typename Pixel>
static void renderPattern(
	V9990VRAM& vram, Pixel* buffer, byte* info, Pixel bgcol, unsigned width,
	unsigned displayX, unsigned displayY, unsigned name, unsigned pattern, const Pixel* pal)
{
	// Speedup drawing by temporarily replacing palette index 0.
	ScopedAssign<Pixel> col0(const_cast<Pixel*>(pal)[0], bgcol);

	renderPattern2(vram, buffer, info, width,
	               displayX, displayY, name, pattern, pal);
}
static void determineVisibleSprites(
	V9990VRAM& vram, int* __restrict visibleSprites, unsigned displayY)
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
		byte spriteAttr = vram.readVRAMDirect(addr + 3);
		bool front = (spriteAttr & 0x20) == 0;
		byte level = front ? 2 : 1;
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
			auto draw = [&](int xPos, size_t p) {
				if ((displayX <= xPos) && (xPos < displayEnd)) {
					size_t xx = xPos - displayX;
					if (p) {
						if (info[xx] < level) {
							buffer[xx] = palette[p];
						}
						info[xx] = 2; // also if back-sprite os behind foreground
					}
				}
			};
			byte data = vram.readVRAMBx(patAddr++);
			draw(spriteX + x + 0, data >> 4);
			draw(spriteX + x + 1, data & 0x0F);
		}
	}
}

template<typename Pixel>
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

	// image plane + backdrop color
	assert(displayWidth <= 512);
	byte info[512]; // left uninitialized
	                // 0->background, 1->foreground, 2->sprite (front or back)
	Pixel bgcol = palette64[vdp.getBackDropColor()];
	byte offset = vdp.getPaletteOffset();
	const Pixel* pal = palette64 + ((offset & 0x03) << 4);
	renderPattern(vram, linePtr, info, bgcol, displayWidth,
	              displayAX, displayAY, 0x7C000, 0x00000, pal);

	// combined back+front sprite plane
	if (drawSprites) {
		int visibleSprites[16 + 1];
		determineVisibleSprites(vram, visibleSprites, displayY);
		unsigned spritePatternTable = vdp.getSpritePatternAddress(P1); // TODO P2 ???
		renderSprites(vram, spritePatternTable, palette64,
		              linePtr, info, displayX, displayEnd, displayY,
		              visibleSprites);
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
