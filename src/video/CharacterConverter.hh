#ifndef CHARACTERCONVERTER_HH
#define CHARACTERCONVERTER_HH

#include <cstdint>
#include <span>

namespace openmsx {

class VDP;
class VDPVRAM;
class DisplayMode;


/** Utility class for converting VRAM contents to host pixels.
  */
class CharacterConverter
{
public:
	using Pixel = uint32_t;

	/** Create a new bitmap scanline converter.
	  * @param vdp The VDP of which the VRAM will be converted.
	  * @param palFg Pointer to 16-entries array that specifies
	  *   VDP foreground color index to host pixel mapping.
	  *   This is kept as a pointer, so any changes to the palette
	  *   are immediately picked up by convertLine.
	  * @param palBg Pointer to 16-entries array that specifies
	  *   VDP background color index to host pixel mapping.
	  *   This is kept as a pointer, so any changes to the palette
	  *   are immediately picked up by convertLine.
	  */
	CharacterConverter(VDP& vdp, std::span<const Pixel, 16> palFg, std::span<const Pixel, 16> palBg);

	/** Convert a line of V9938 VRAM to 256 or 512 host pixels.
	  * Call this method in non-planar display modes (Graphic4 and Graphic5).
	  * @param buf Buffer where host pixels will be written to.
	  *            Depending on the screen mode, the buffer must contain at
	  *            least 256 or 512 pixels, but more is allowed (the extra
	  *            pixels aren't touched).
	  * @param line Display line number [0..255].
	  */
	void convertLine(std::span<Pixel> buf, int line) const;

	/** Select the display mode to use for scanline conversion.
	  * @param mode The new display mode.
	  */
	void setDisplayMode(DisplayMode mode);

private:
	inline void renderText1   (std::span<Pixel, 256> buf, int line) const;
	inline void renderText1Q  (std::span<Pixel, 256> buf, int line) const;
	inline void renderText2   (std::span<Pixel, 512> buf, int line) const;
	inline void renderGraphic1(std::span<Pixel, 256> buf, int line) const;
	inline void renderGraphic2(std::span<Pixel, 256> buf, int line) const;
	inline void renderMulti   (std::span<Pixel, 256> buf, int line) const;
	inline void renderMultiQ  (std::span<Pixel, 256> buf, int line) const;
	inline void renderBogus   (std::span<Pixel, 256> buf) const;
	inline void renderBlank   (std::span<Pixel, 256> buf) const;
	inline void renderMultiHelper(Pixel* pixelPtr, int line,
	                       unsigned mask, unsigned patternQuarter) const;

	[[nodiscard]] std::span<const uint8_t, 32> getNamePtr(int line, int scroll) const;

private:
	VDP& vdp;
	VDPVRAM& vram;

	std::span<const Pixel, 16> palFg;
	std::span<const Pixel, 16> palBg;

	unsigned modeBase = 0; // not strictly needed, but avoids Coverity warning
};

} // namespace openmsx

#endif
