/*
TODO:
- Implement sprite pixels in Graphic 5.
*/

#ifndef SPRITECONVERTER_HH
#define SPRITECONVERTER_HH

#include "SpriteChecker.hh"
#include "DisplayMode.hh"
#include "view.hh"
#include "narrow.hh"
#include <cstdint>
#include <span>

namespace openmsx {

/** Utility class for converting VRAM contents to host pixels.
  */
class SpriteConverter
{
public:
	using Pixel = uint32_t;

	// TODO: Move some methods to .cc?

	/** Constructor.
	  * After construction, also call the various set methods to complete
	  * initialisation.
	  * @param spriteChecker_ Delivers the sprite data to be rendered.
	  * @param pal The initial palette. Can later be changed via setPallette().
	  */
	explicit SpriteConverter(SpriteChecker& spriteChecker_,
	                         std::span<const Pixel, 16> pal)
		: spriteChecker(spriteChecker_)
		, palette(pal)
	{
	}

	/** Update the transparency setting.
	  * @param enabled The new value.
	  */
	void setTransparency(bool enabled)
	{
		transparency = enabled;
	}

	/** Notify SpriteConverter of a display mode change.
	  * @param newMode The new display mode.
	  */
	void setDisplayMode(DisplayMode newMode)
	{
		mode = newMode;
	}

	/** Set palette to use for converting sprites.
	  * This palette is stored by reference, so any modifications to it
	  * will be used while drawing.
	  * @param newPalette 16-entry array containing the sprite palette.
	  */
	void setPalette(std::span<const Pixel, 16> newPalette)
	{
		palette = newPalette;
	}

	static bool clipPattern(int& x, SpriteChecker::SpritePattern& pattern,
	                        int minX, int maxX)
	{
		int before = minX - x;
		if (before > 0) {
			if (before >= 32) {
				// 32 pixels before minX -> not visible
				return false;
			}
			pattern <<= before;
			x = minX;
		}
		int after = maxX - x;
		if (after < 32) {
			// close to maxX (or past)
			if (after <= 0) {
				// past maxX -> not visible
				return false;
			}
			auto mask = narrow_cast<int>(0x80000000);
			pattern &= (mask >> (after - 1));
		}
		return true; // visible
	}

	/** Draw sprites in sprite mode 1.
	  * @param absLine Absolute line number.
	  *     Range is [0..262) for NTSC and [0..313) for PAL.
	  * @param minX Minimum X coordinate to draw (inclusive).
	  * @param maxX Maximum X coordinate to draw (exclusive).
	  * @param pixelPtr Pointer to memory to draw to.
	  */
	void drawMode1(int absLine, int minX, int maxX, std::span<Pixel> pixelPtr)
	{
		// Determine sprites visible on this line.
		auto visibleSprites = spriteChecker.getSprites(absLine);
		// Optimisation: return at once if no sprites on this line.
		// Lines without any sprites are very common in most programs.
		if (visibleSprites.empty()) return;

		// Render using overdraw.
		for (const auto& si : view::reverse(visibleSprites)) {
			// Get sprite info.
			Pixel colIndex = si.colorAttrib & 0x0F;
			// Don't draw transparent sprites in sprite mode 1.
			// Verified on real V9958: TP bit also has effect in
			// sprite mode 1.
			if (colIndex == 0 && transparency) continue;
			Pixel color = palette[colIndex];
			SpriteChecker::SpritePattern pattern = si.pattern;
			int x = si.x;
			// Clip sprite pattern to render range.
			if (!clipPattern(x, pattern, minX, maxX)) continue;
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
	template<unsigned MODE>
	void drawMode2(int absLine, int minX, int maxX, std::span<Pixel> pixelPtr)
	{
		// Determine sprites visible on this line.
		auto visibleSprites = spriteChecker.getSprites(absLine);
		// Optimisation: return at once if no sprites on this line.
		// Lines without any sprites are very common in most programs.
		if (visibleSprites.empty()) return;
		std::span visibleSpritesWithSentinel{visibleSprites.data(),
		                                     visibleSprites.size() +1};

		// Sprites with CC=1 are only visible if preceded by a sprite
		// with CC=0. Therefor search for first sprite with CC=0.
		int first = 0;
		do {
			if ((visibleSprites[first].colorAttrib & 0x40) == 0) [[likely]] {
				break;
			}
			++first;
		} while (first < int(visibleSprites.size()));
		for (int i = narrow<int>(visibleSprites.size() - 1); i >= first; --i) {
			const SpriteChecker::SpriteInfo& info = visibleSprites[i];
			int x = info.x;
			SpriteChecker::SpritePattern pattern = info.pattern;
			// Clip sprite pattern to render range.
			if (!clipPattern(x, pattern, minX, maxX)) continue;
			uint8_t c = info.colorAttrib & 0x0F;
			if (c == 0 && transparency) continue;
			while (pattern) {
				if (pattern & 0x80000000) {
					uint8_t color = c;
					// Merge in any following CC=1 sprites.
					for (int j = i + 1; /*sentinel*/; ++j) {
						const SpriteChecker::SpriteInfo& info2 =
							visibleSpritesWithSentinel[j];
						if (!(info2.colorAttrib & 0x40)) break;
						unsigned shift2 = x - info2.x;
						if ((shift2 < 32) &&
						   ((info2.pattern << shift2) & 0x80000000)) {
							color |= info2.colorAttrib & 0x0F;
						}
					}
					if constexpr (MODE == DisplayMode::GRAPHIC5) {
						Pixel pixL = palette[color >> 2];
						Pixel pixR = palette[color & 3];
						pixelPtr[x * 2 + 0] = pixL;
						pixelPtr[x * 2 + 1] = pixR;
					} else {
						Pixel pix = palette[color];
						if constexpr (MODE == DisplayMode::GRAPHIC6) {
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
	std::span<const Pixel, 16> palette;

	/** VDP transparency setting (R#8, bit5).
	  */
	bool transparency;

	/** The current display mode.
	  */
	DisplayMode mode;
};

} // namespace openmsx

#endif
