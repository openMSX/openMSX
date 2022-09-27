#ifndef V9990BITMAPCONVERTER_HH
#define V9990BITMAPCONVERTER_HH

#include "V9990ModeEnum.hh"
#include "one_of.hh"
#include <concepts>
#include <cstdint>
#include <span>

namespace openmsx {

class V9990;
class V9990VRAM;

/** Utility class to convert VRAM content to host pixels.
  */
template<std::unsigned_integral Pixel>
class V9990BitmapConverter
{
public:
	V9990BitmapConverter(
		V9990& vdp,
		std::span<const Pixel,    64> palette64,  std::span<const int16_t,  64> palette64_32768,
		std::span<const Pixel,   256> palette256, std::span<const int16_t, 256> palette256_32768,
		std::span<const Pixel, 32768> palette32768);

	/** Convert a line of VRAM into host pixels.
	  */
	void convertLine(Pixel* linePtr, unsigned x, unsigned y, int nrPixels,
		         int cursorY, bool drawCursors);

	/** Set a different rendering mode.
	  */
	void setColorMode(V9990ColorMode colorMode_, V9990DisplayMode display) {
		colorMode = colorMode_;
		highRes = isHighRes(display);
	}

private:
	[[nodiscard]] static bool isHighRes(V9990DisplayMode display) {
		return display == one_of(B4, B5, B6, B7);
	}

private:
	/** Reference to VDP
	  */
	V9990& vdp;

	/** Reference to VDP VRAM
	  */
	V9990VRAM& vram;

	/** The 64 color palette for P1, P2 and BP* modes
	  * This is the palette manipulated through the palette port and register
	  */
	std::span<const Pixel, 64> const palette64;
	std::span<const int16_t, 64> const palette64_32768;

	/** The 256 color palette for BD8 mode
	  * A fixed palette; sub-color space within the 32768 color palette
	  */
	std::span<const Pixel, 256> const palette256;
	std::span<const int16_t, 256> const palette256_32768;

	/** The 15-bits color palette for BD16, BYJK* and BYUV modes
	  * This is the complete color space for the V9990
	  */
	std::span<const Pixel, 32768> const palette32768;

	/** Remember last color and display mode. This is always the same as
	  * vdp.getColorMode() and vdp.getDisplayMode() because these two only
	  * change at the end of a display line.
	  */
	V9990ColorMode colorMode;
	bool highRes;
};

} // namespace openmsx

#endif
