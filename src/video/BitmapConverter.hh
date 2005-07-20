// $Id$

#ifndef BITMAPCONVERTER_HH
#define BITMAPCONVERTER_HH

#include "openmsx.hh"
#include "Renderer.hh"
#include "DisplayMode.hh"

namespace openmsx {

/** Utility class for converting VRAM contents to host pixels.
  */
template <class Pixel>
class BitmapConverter
{
public:
	/** Create a new bitmap scanline converter.
	  * @param palette16 Pointer to 2*16-entries array that specifies
	  *   VDP colour index to host pixel mapping.
	  *   This is kept as a pointer, so any changes to the palette
	  *   are immediately picked up by convertLine.
	  *   Used for display modes Graphic4, Graphic5 and Graphic6.
	  *   First 16 entries are for even pixels, next 16 are for odd pixels
	  * @param palette256 Pointer to 256-entries array that specifies
	  *   VDP colour index to host pixel mapping.
	  *   This is kept as a pointer, so any changes to the palette
	  *   are immediately picked up by convertLine.
	  *   Used for display mode Graphic7.
	  * @param palette32768 Pointer to 32768-entries array that specifies
	  *   VDP colour index to host pixel mapping.
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
	inline void convertLine(
		Pixel* linePtr, const byte* vramPtr)
	{
		(this->*renderMethod)(linePtr, vramPtr, 0);
	}

	/** Convert a line of V9938 VRAM to 512 host pixels.
	  * Call this method in planar display modes (Graphic6 and Graphic7).
	  * @param linePtr Pointer to array of host pixels.
	  * @param vramPtr0 Pointer to VRAM contents, first plane.
	  * @param vramPtr1 Pointer to VRAM contents, second plane.
	  */
	inline void convertLinePlanar(
		Pixel* linePtr, const byte* vramPtr0, const byte* vramPtr1)
	{
		(this->*renderMethod)(linePtr, vramPtr0, vramPtr1);
	}

	/** Select the display mode to use for scanline conversion.
	  * @param mode The new display mode.
	  * TODO: Should this be inlined? It's not used that frequently.
	  */
	inline void setDisplayMode(DisplayMode mode)
	{
		// TODO: Support YJK on modes other than Graphic 6/7.
		switch (mode.getByte() & ~DisplayMode::YAE) {
		case DisplayMode::GRAPHIC4:
			renderMethod = &BitmapConverter::renderGraphic4;
			break;
		case DisplayMode::GRAPHIC5:
			renderMethod = &BitmapConverter::renderGraphic5;
			break;
		case DisplayMode::GRAPHIC6:
			renderMethod = &BitmapConverter::renderGraphic6;
			break;
		case DisplayMode::GRAPHIC7:
			renderMethod = &BitmapConverter::renderGraphic7;
			break;
		case DisplayMode::GRAPHIC6 | DisplayMode::YJK:
		case DisplayMode::GRAPHIC7 | DisplayMode::YJK:
			if (mode.getByte() & DisplayMode::YAE) {
				renderMethod = &BitmapConverter::renderYAE;
			} else {
				renderMethod = &BitmapConverter::renderYJK;
			}
			break;
		default:
			renderMethod = &BitmapConverter::renderBogus;
			break;
		}
	}

private:

	typedef void (BitmapConverter::*RenderMethod)
		(Pixel* pixelPtr, const byte* vramPtr0, const byte* vramPtr1);

	void renderGraphic4(
		Pixel* pixelPtr, const byte* vramPtr0, const byte* vramPtr1);
	void renderGraphic5(
		Pixel* pixelPtr, const byte* vramPtr0, const byte* vramPtr1);
	void renderGraphic6(
		Pixel* pixelPtr, const byte* vramPtr0, const byte* vramPtr1);
	void renderGraphic7(
		Pixel* pixelPtr, const byte* vramPtr0, const byte* vramPtr1);
	void renderYJK(
		Pixel* pixelPtr, const byte* vramPtr0, const byte* vramPtr1);
	void renderYAE(
		Pixel* pixelPtr, const byte* vramPtr0, const byte* vramPtr1);
	void renderBogus(
		Pixel* pixelPtr, const byte* vramPtr0, const byte* vramPtr1);

	/** Rendering method for the current display mode.
	  */
	RenderMethod renderMethod;

	const Pixel* palette16;
	const Pixel* palette256;
	const Pixel* palette32768;
};

} // namespace openmsx

#endif
