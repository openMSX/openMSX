#ifndef BITMAPCONVERTER_HH
#define BITMAPCONVERTER_HH

#include "DisplayMode.hh"
#include "openmsx.hh"
#include <cstdint>

namespace openmsx {

template<int N> struct DoublePixel;
template<> struct DoublePixel<2> { using type = uint32_t; };
template<> struct DoublePixel<4> { using type = uint64_t; };

/** Utility class for converting VRAM contents to host pixels.
  */
template<typename Pixel>
class BitmapConverter
{
public:
	/** Create a new bitmap scanline converter.
	  * @param palette16 Pointer to 2*16-entries array that specifies
	  *   VDP color index to host pixel mapping.
	  *   This is kept as a pointer, so any changes to the palette
	  *   are immediately picked up by convertLine. Though to allow
	  *   some internal optimizations, this class should be informed about
	  *   changes in this array (not needed for the next two), see
	  *   palette16Changed().
	  *   Used for display modes Graphic4, Graphic5 and Graphic6.
	  *   First 16 entries are for even pixels, next 16 are for odd pixels
	  * @param palette256 Pointer to 256-entries array that specifies
	  *   VDP color index to host pixel mapping.
	  *   This is kept as a pointer, so any changes to the palette
	  *   are immediately picked up by convertLine.
	  *   Used for display mode Graphic7.
	  * @param palette32768 Pointer to 32768-entries array that specifies
	  *   VDP color index to host pixel mapping.
	  *   This is kept as a pointer, so any changes to the palette
	  *   are immediately picked up by convertLine.
	  *   Used when YJK filter is active.
	  */
	BitmapConverter(const Pixel* palette16,
	                const Pixel* palette256,
	                const Pixel* palette32768);

	/** Convert a line of V9938 VRAM to 512 host pixels.
	  * Call this method in non-planar display modes (Graphic4 and Graphic5).
	  * @param linePtr Pointer to array of host pixels.
	  * @param vramPtr Pointer to VRAM contents.
	  */
	void convertLine(Pixel* linePtr, const byte* vramPtr);

	/** Convert a line of V9938 VRAM to 512 host pixels.
	  * Call this method in planar display modes (Graphic6 and Graphic7).
	  * @param linePtr Pointer to array of host pixels.
	  * @param vramPtr0 Pointer to VRAM contents, first plane.
	  * @param vramPtr1 Pointer to VRAM contents, second plane.
	  */
	void convertLinePlanar(Pixel* linePtr,
	                       const byte* vramPtr0, const byte* vramPtr1);

	/** Select the display mode to use for scanline conversion.
	  * @param mode_ The new display mode.
	  */
	inline void setDisplayMode(DisplayMode mode_)
	{
		mode = mode_;
	}

	/** Inform this class about changes in the palette16 array.
	  */
	inline void palette16Changed()
	{
		dPaletteValid = false;
	}

private:
	void calcDPalette();

	inline void renderGraphic4(Pixel* pixelPtr, const byte* vramPtr0);
	inline void renderGraphic5(Pixel* pixelPtr, const byte* vramPtr0);
	inline void renderGraphic6(
		Pixel* pixelPtr, const byte* vramPtr0, const byte* vramPtr1);
	inline void renderGraphic7(
		Pixel* pixelPtr, const byte* vramPtr0, const byte* vramPtr1);
	inline void renderYJK(
		Pixel* pixelPtr, const byte* vramPtr0, const byte* vramPtr1);
	inline void renderYAE(
		Pixel* pixelPtr, const byte* vramPtr0, const byte* vramPtr1);
	inline void renderBogus(Pixel* pixelPtr);

private:
	const Pixel* const __restrict palette16;
	const Pixel* const __restrict palette256;
	const Pixel* const __restrict palette32768;

	using DPixel = typename DoublePixel<sizeof(Pixel)>::type;
	DPixel dPalette[16 * 16];
	DisplayMode mode;
	bool dPaletteValid;
};

} // namespace openmsx

#endif
