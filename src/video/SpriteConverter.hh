// $Id$

#ifndef __SPRITECONVERTER_HH__
#define __SPRITECONVERTER_HH__

#include "openmsx.hh"
#include "Renderer.hh"
#include "SpriteChecker.hh"
#include <cassert>

/** Utility class for converting VRAM contents to host pixels.
  */
template <class Pixel, Renderer::Zoom zoom>
class SpriteConverter
{
public:

	// TODO: Move some methods to .cc?

	/** Constructor.
	  * After construction, also call the various set methods to complete
	  * initialisation.
	  */
	SpriteConverter(SpriteChecker *spriteChecker) {
		this->spriteChecker = spriteChecker;
	}

	/** Update the transparency setting.
	  * @param enabled The new value.
	  */
	void setTransparency(bool enabled) {
		this->transparency = enabled;
	}

	/** Set palette to use for converting sprites.
	  * This palette is stored by reference, so any modifications to it
	  * will be used while drawing.
	  * @param palette 16-entry array containing the sprite palette.
	  */
	void setPalette(Pixel *palette) {
		this->palette = palette;
	}

	/** Draw sprites in sprite mode 1.
	  * @param absLine Absolute line number.
	  * 	Range is [0..262) for NTSC and [0..313) for PAL.
	  * @param minX Minimum X coordinate to draw (inclusive).
	  * @param maxX Maximum X coordinate to draw (exclusive).
	  * @param pixelPtr0 Pointer to memory to draw to.
	  * @param pixelPtr1 Pointer to memory to draw to as well,
	  * 	only specify if needed.
	  */
	void drawMode1(
		int absLine, int minX, int maxX,
		Pixel *pixelPtr0, Pixel *pixelPtr1 = NULL
	) {
		// Determine sprites visible on this line.
		SpriteChecker::SpriteInfo *visibleSprites;
		int visibleIndex =
			spriteChecker->getSprites(absLine, visibleSprites);
		// Optimisation: return at once if no sprites on this line.
		// Lines without any sprites are very common in most programs.
		if (visibleIndex == 0) return;

		// Render using overdraw.
		while (visibleIndex--) {
			// Get sprite info.
			SpriteChecker::SpriteInfo *sip = &visibleSprites[visibleIndex];
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
			if (zoom == Renderer::ZOOM_512) {
				Pixel *p0 = &pixelPtr0[x * 2];
				Pixel *p1 = &pixelPtr1[x * 2];
				while (pattern) {
					// Draw pixel if sprite has a dot.
					if (pattern & 0x80000000) {
						p0[0] = p0[1] = p1[0] = p1[1] = colour;
					}
					// Advancing behaviour.
					pattern <<= 1;
					p0 += 2;
					p1 += 2;
				}
			} else {
				Pixel *p = &pixelPtr0[x];
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
	}

	/** Draw sprites in sprite mode 2.
	  * @param absLine Absolute line number.
	  * 	Range is [0..262) for NTSC and [0..313) for PAL.
	  * @param minX Minimum X coordinate to draw (inclusive).
	  * @param maxX Maximum X coordinate to draw (exclusive).
	  * @param pixelPtr0 Pointer to memory to draw to.
	  * @param pixelPtr1 Pointer to memory to draw to as well,
	  * 	only specify if needed.
	  */
	void drawMode2(
		int absLine, int minX, int maxX,
		Pixel *pixelPtr0, Pixel *pixelPtr1 = NULL
	) {
		// TODO: SDLGL is different here.
		assert((zoom == Renderer::ZOOM_512) != (pixelPtr1 == NULL));

		// Determine sprites visible on this line.
		SpriteChecker::SpriteInfo *visibleSprites;
		int visibleIndex =
			spriteChecker->getSprites(absLine, visibleSprites);
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
				if (pixelDone >= 256) break;
			}
			// Calculate colour of pixel to be plotted.
			byte colour = 0xFF;
			for (int i = 0; i < visibleIndex; i++) {
				SpriteChecker::SpriteInfo *sip = &visibleSprites[i];
				int shift = pixelDone - sip->x;
				if ((0 <= shift && shift < maxSize)
				&& ((sip->pattern << shift) & 0x80000000)) {
					byte c = sip->colourAttrib & 0x0F;
					if (c == 0 && transparency) continue;
					colour = c;
					// Merge in any following CC=1 sprites.
					for (i++ ; i < visibleIndex; i++) {
						sip = &visibleSprites[i];
						if (!(sip->colourAttrib & 0x40)) break;
						int shift = pixelDone - sip->x;
						if ((0 <= shift && shift < maxSize)
						&& ((sip->pattern << shift) & 0x80000000)) {
							colour |= sip->colourAttrib & 0x0F;
						}
					}
					break;
				}
			}
			// Plot it.
			if (colour != 0xFF) {
				if (zoom == Renderer::ZOOM_512) {
					int i = pixelDone * 2;
					pixelPtr0[i] = pixelPtr0[i + 1] =
					pixelPtr1[i] = pixelPtr1[i + 1] =
						palette[colour];
				} else {
					pixelPtr0[pixelDone] = palette[colour];
				}
			}
		}
	}

private:

	SpriteChecker *spriteChecker;

	/** VDP transparency setting (R#8, bit5).
	  */
	bool transparency;
	
	/** The current sprite palette.
	  */
	Pixel *palette;

};

#endif // __SPRITECONVERTER_HH__
