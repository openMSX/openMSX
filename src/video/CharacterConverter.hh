#ifndef CHARACTERCONVERTER_HH
#define CHARACTERCONVERTER_HH

#include "DisplayMode.hh"
#include "openmsx.hh"

namespace openmsx {

class VDP;
class VDPVRAM;


/** Utility class for converting VRAM contents to host pixels.
  */
template <class Pixel>
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
	CharacterConverter(VDP& vdp, const Pixel* palFg, const Pixel* palBg);

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
	inline void renderText1   (Pixel* pixelPtr, int line) __restrict;
	inline void renderText1Q  (Pixel* pixelPtr, int line) __restrict;
	inline void renderText2   (Pixel* pixelPtr, int line) __restrict;
	inline void renderGraphic1(Pixel* pixelPtr, int line) __restrict;
	inline void renderGraphic2(Pixel* pixelPtr, int line) __restrict;
	inline void renderMulti   (Pixel* pixelPtr, int line) __restrict;
	inline void renderMultiQ  (Pixel* pixelPtr, int line) __restrict;
	inline void renderBogus   (Pixel* pixelPtr) __restrict;
	inline void renderMultiHelper(Pixel* pixelPtr, int line,
	                       int mask, int patternQuarter) __restrict;

	const byte* getNamePtr(int line, int scroll) __restrict;

	VDP& vdp;
	VDPVRAM& vram;

	const Pixel* const palFg;
	const Pixel* const palBg;

	unsigned modeBase;
};

} // namespace openmsx

#endif
