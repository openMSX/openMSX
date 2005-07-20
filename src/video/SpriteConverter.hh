// $Id$

/*
TODO:
- Implement sprite pixels in Graphic 5.
*/

#ifndef SPRITECONVERTER_HH
#define SPRITECONVERTER_HH

#include "openmsx.hh"
#include "Renderer.hh"
#include "SpriteChecker.hh"
#include "DisplayMode.hh"

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
	SpriteConverter(SpriteChecker& spriteChecker_)
		: spriteChecker(spriteChecker_)
	{
	}

	/** Update the transparency setting.
	  * @param enabled The new value.
	  */
	void setTransparency(bool enabled) {
		this->transparency = enabled;
	}

	/** Notify SpriteConverter of a display mode change.
	  * @param mode The new display mode.
	  */
	void setDisplayMode(DisplayMode mode) {
		this->mode = mode;
	}

	/** Set palette to use for converting sprites.
	  * This palette is stored by reference, so any modifications to it
	  * will be used while drawing.
	  * @param palette 16-entry array containing the sprite palette.
	  */
	void setPalette(Pixel* palette) {
		this->palette = palette;
	}

	/** Draw sprites in sprite mode 1.
	  * @param absLine Absolute line number.
	  * 	Range is [0..262) for NTSC and [0..313) for PAL.
	  * @param minX Minimum X coordinate to draw (inclusive).
	  * @param maxX Maximum X coordinate to draw (exclusive).
	  * @param pixelPtr Pointer to memory to draw to.
	  */
	void drawMode1(
		int absLine, int minX, int maxX, Pixel* pixelPtr
	) {
		// Determine sprites visible on this line.
		SpriteChecker::SpriteInfo* visibleSprites;
		int visibleIndex =
			spriteChecker.getSprites(absLine, visibleSprites);
		// Optimisation: return at once if no sprites on this line.
		// Lines without any sprites are very common in most programs.
		if (visibleIndex == 0) return;

		// Render using overdraw.
		while (visibleIndex--) {
			// Get sprite info.
			SpriteChecker::SpriteInfo* sip = &visibleSprites[visibleIndex];
			Pixel colIndex = sip->colourAttrib & 0x0F;
			// Don't draw transparent sprites in sprite mode 1.
			// TODO: Verify on real V9938 that sprite mode 1 indeed
			//       ignores the transparency bit.
			if (colIndex == 0) continue;
			Pixel colour = palette[colIndex];
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
					*p = colour;
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
	  * 	Range is [0..262) for NTSC and [0..313) for PAL.
	  * @param minX Minimum X coordinate to draw (inclusive).
	  * @param maxX Maximum X coordinate to draw (exclusive).
	  * @param pixelPtr Pointer to memory to draw to.
	  */
	void drawMode2(
		int absLine, int minX, int maxX, Pixel* pixelPtr
	) {
		// Determine sprites visible on this line.
		SpriteChecker::SpriteInfo* visibleSprites;
		int visibleIndex =
			spriteChecker.getSprites(absLine, visibleSprites);
		// Optimisation: return at once if no sprites on this line.
		// Lines without any sprites are very common in most programs.
		if (visibleIndex == 0) return;

		// Determine width of sprites.
		SpriteChecker::SpritePattern combined = 0;
		for (int i = 0; i < visibleIndex; i++) {
			combined |= visibleSprites[i].pattern;
		}
		int maxSize = SpriteChecker::patternWidth(combined);
		// Left-to-right scan.
		for (int pixelDone = minX; pixelDone < maxX; pixelDone++) {
			// Skip pixels if possible.
			int minStart = pixelDone - maxSize;
			int leftMost = 0xFFFF;
			for (int i = 0; i < visibleIndex; i++) {
				int x = visibleSprites[i].x;
				if (minStart < x && x < leftMost) leftMost = x;
			}
			if (leftMost > pixelDone) {
				pixelDone = leftMost;
				if (pixelDone >= maxX) break;
			}
			// Calculate colour of pixel to be plotted.
			byte colour = 0xFF;
			for (int i = 0; i < visibleIndex; i++) {
				SpriteChecker::SpriteInfo& info = visibleSprites[i];
				int shift = pixelDone - info.x;
				if ((0 <= shift && shift < maxSize)
				&& ((info.pattern << shift) & 0x80000000)) {
					byte c = info.colourAttrib & 0x0F;
					if (c == 0 && transparency) continue;
					colour = c;
					// Merge in any following CC=1 sprites.
					for (int j = i + 1; j < visibleIndex; j++) {
						SpriteChecker::SpriteInfo& info2 = visibleSprites[j];
						if (!(info2.colourAttrib & 0x40)) break;
						int shift2 = pixelDone - info2.x;
						if ((0 <= shift2 && shift2 < maxSize)
						&& ((info2.pattern << shift2) & 0x80000000)) {
							colour |= info2.colourAttrib & 0x0F;
						}
					}
					break;
				}
			}
			// Plot it.
			if (colour != 0xFF) {
				if (mode.getByte() == DisplayMode::GRAPHIC5) {
					Pixel pixL = palette[colour >> 2];
					Pixel pixR = palette[colour & 3];
					pixelPtr[pixelDone * 2 + 0] = pixL;
					pixelPtr[pixelDone * 2 + 1] = pixR;
				} else {
					Pixel pix = palette[colour];
					if (mode.getByte() == DisplayMode::GRAPHIC6) {
						pixelPtr[pixelDone * 2 + 0] = pix;
						pixelPtr[pixelDone * 2 + 1] = pix;
					} else {
						pixelPtr[pixelDone] = pix;
					}
				}
			}
		}
	}

private:
	SpriteChecker& spriteChecker;

	/** VDP transparency setting (R#8, bit5).
	  */
	bool transparency;

	/** The current display mode.
	  */
	DisplayMode mode;

	/** The current sprite palette.
	  */
	Pixel* palette;
};

} // namespace openmsx

#endif
