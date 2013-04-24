/*
TODO:
- Implement sprite pixels in Graphic 5.
*/

#ifndef SPRITECONVERTER_HH
#define SPRITECONVERTER_HH

#include "SpriteChecker.hh"
#include "DisplayMode.hh"
#include "openmsx.hh"

namespace openmsx {

/** Utility class for converting VRAM contents to host pixels.
  */
template <class Pixel>
class SpriteConverter
{
public:
	// TODO: Move some methods to .cc?

	/** Constructor.
	  * After construction, also call the various set methods to complete
	  * initialisation.
	  * @param spriteChecker_ Delivers the sprite data to be rendered.
	  */
	explicit SpriteConverter(SpriteChecker& spriteChecker_)
		: spriteChecker(spriteChecker_)
	{
	}

	/** Update the transparency setting.
	  * @param enabled The new value.
	  */
	void setTransparency(bool enabled)
	{
		this->transparency = enabled;
	}

	/** Notify SpriteConverter of a display mode change.
	  * @param mode The new display mode.
	  */
	void setDisplayMode(DisplayMode mode)
	{
		this->mode = mode;
	}

	/** Set palette to use for converting sprites.
	  * This palette is stored by reference, so any modifications to it
	  * will be used while drawing.
	  * @param palette 16-entry array containing the sprite palette.
	  */
	void setPalette(const Pixel* palette)
	{
		this->palette = palette;
	}

	/** Draw sprites in sprite mode 1.
	  * @param absLine Absolute line number.
	  *     Range is [0..262) for NTSC and [0..313) for PAL.
	  * @param minX Minimum X coordinate to draw (inclusive).
	  * @param maxX Maximum X coordinate to draw (exclusive).
	  * @param pixelPtr Pointer to memory to draw to.
	  */
	void drawMode1(int absLine, int minX, int maxX,
	               Pixel* __restrict pixelPtr) __restrict
	{
		// Determine sprites visible on this line.
		const SpriteChecker::SpriteInfo* visibleSprites;
		int visibleIndex =
			spriteChecker.getSprites(absLine, visibleSprites);
		// Optimisation: return at once if no sprites on this line.
		// Lines without any sprites are very common in most programs.
		if (visibleIndex == 0) return;

		// Render using overdraw.
		while (visibleIndex--) {
			// Get sprite info.
			const SpriteChecker::SpriteInfo* sip =
				&visibleSprites[visibleIndex];
			Pixel colIndex = sip->colorAttrib & 0x0F;
			// Don't draw transparent sprites in sprite mode 1.
			// TODO: Verify on real V9938 that sprite mode 1 indeed
			//       ignores the transparency bit.
			if (colIndex == 0) continue;
			Pixel color = palette[colIndex];
			SpriteChecker::SpritePattern pattern = sip->pattern;
			int x = sip->x;
			// Clip sprite pattern to render range.
			if (x < minX) {
				if (x <= minX - 32) continue;
				pattern <<= minX - x;
				x = minX;
			} else if (x > maxX - 32) {
				if (x >= maxX) continue;
				pattern &= -1 << (32 - (maxX - x));
			}
			// Convert pattern to pixels.
			Pixel* p = &pixelPtr[x];
			while (pattern) {
				// Draw pixel if sprite has a dot.
				if (pattern & 0x80000000) {
					*p = color;
				}
				// Advancing behaviour.
				pattern <<= 1;
				p++;
			}
		}
	}

	/** Draw sprites in sprite mode 2.
	  * Make sure the pixel pointers point to a large enough memory area:
	  * 256 pixels for ZOOM_256 and ZOOM_REAL in 256-pixel wide modes;
	  * 512 pixels for ZOOM_REAL in 512-pixel wide modes.
	  * @param absLine Absolute line number.
	  *     Range is [0..262) for NTSC and [0..313) for PAL.
	  * @param minX Minimum X coordinate to draw (inclusive).
	  * @param maxX Maximum X coordinate to draw (exclusive).
	  * @param pixelPtr Pointer to memory to draw to.
	  */
	template <unsigned MODE>
	void drawMode2(int absLine, int minX, int maxX,
	               Pixel* __restrict pixelPtr) __restrict
	{
		// Determine sprites visible on this line.
		const SpriteChecker::SpriteInfo* visibleSprites;
		int visibleIndex =
			spriteChecker.getSprites(absLine, visibleSprites);
		// Optimisation: return at once if no sprites on this line.
		// Lines without any sprites are very common in most programs.
		if (visibleIndex == 0) return;

		for (int i = visibleIndex - 1; i >= 0; --i) {
			const SpriteChecker::SpriteInfo& info = visibleSprites[i];
			int x = info.x;
			SpriteChecker::SpritePattern pattern = info.pattern;
			int d = minX - x;
			if (d > 0) {
				if (d >= 32) continue;
				pattern <<= d;
				x = minX;
			}
			d = maxX - x;
			if (d < 32) {
				if (d <= 0) continue;
				int mask = 0x80000000;
				pattern &= (mask >> (d - 1));
			}
			byte c = info.colorAttrib & 0x0F;
			if (c == 0 && transparency) continue;
			while (pattern) {
				if (pattern & 0x80000000) {
					byte color = c;
					// Merge in any following CC=1 sprites.
					for (int j = i + 1; /*sentinel*/; ++j) {
						const SpriteChecker::SpriteInfo& info2 =
							visibleSprites[j];
						if (!(info2.colorAttrib & 0x40)) break;
						unsigned shift2 = x - info2.x;
						if ((shift2 < 32) &&
						   ((info2.pattern << shift2) & 0x80000000)) {
							color |= info2.colorAttrib & 0x0F;
						}
					}
					if (MODE == DisplayMode::GRAPHIC5) {
						Pixel pixL = palette[color >> 2];
						Pixel pixR = palette[color & 3];
						pixelPtr[x * 2 + 0] = pixL;
						pixelPtr[x * 2 + 1] = pixR;
					} else {
						Pixel pix = palette[color];
						if (MODE == DisplayMode::GRAPHIC6) {
							pixelPtr[x * 2 + 0] = pix;
							pixelPtr[x * 2 + 1] = pix;
						} else {
							pixelPtr[x] = pix;
						}
					}
				}
				++x;
				pattern <<= 1;
			}
		}
	}

private:
	SpriteChecker& spriteChecker;

	/** The current sprite palette.
	  */
	const Pixel* palette;

	/** VDP transparency setting (R#8, bit5).
	  */
	bool transparency;

	/** The current display mode.
	  */
	DisplayMode mode;
};

} // namespace openmsx

#endif
