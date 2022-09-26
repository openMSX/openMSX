#ifndef CHARACTERCONVERTER_HH
#define CHARACTERCONVERTER_HH

#include "openmsx.hh"
#include <concepts>
#include <span>

namespace openmsx {

class VDP;
class VDPVRAM;
class DisplayMode;


/** Utility class for converting VRAM contents to host pixels.
  */
template<std::unsigned_integral Pixel>
class CharacterConverter
{
public:
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

	/** Convert a line of V9938 VRAM to 512 host pixels.
	  * Call this method in non-planar display modes (Graphic4 and Graphic5).
	  * @param linePtr Pointer to array where host pixels will be written to.
	  * @param line Display line number [0..255].
	  */
	void convertLine(Pixel* linePtr, int line);

	/** Select the display mode to use for scanline conversion.
	  * @param mode The new display mode.
	  */
	void setDisplayMode(DisplayMode mode);

private:
	inline void renderText1   (Pixel* pixelPtr, int line);
	inline void renderText1Q  (Pixel* pixelPtr, int line);
	inline void renderText2   (Pixel* pixelPtr, int line);
	inline void renderGraphic1(Pixel* pixelPtr, int line);
	inline void renderGraphic2(Pixel* pixelPtr, int line);
	inline void renderMulti   (Pixel* pixelPtr, int line);
	inline void renderMultiQ  (Pixel* pixelPtr, int line);
	inline void renderBogus   (Pixel* pixelPtr);
	inline void renderBlank   (Pixel* pixelPtr);
	inline void renderMultiHelper(Pixel* pixelPtr, int line,
	                       int mask, int patternQuarter);

	[[nodiscard]] const byte* getNamePtr(int line, int scroll);

private:
	VDP& vdp;
	VDPVRAM& vram;

	std::span<const Pixel, 16> palFg;
	std::span<const Pixel, 16> palBg;

	unsigned modeBase;
};

} // namespace openmsx

#endif
