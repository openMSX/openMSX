// $Id$

#ifndef __BITMAPCONVERTER_HH__
#define __BITMAPCONVERTER_HH__

#include "openmsx.hh"
#include "Renderer.hh"

/** Utility class for converting VRAM contents to host pixels.
  */
template <class Pixel, Renderer::Zoom zoom>
class BitmapConverter
{
public:
	/** Create a new bitmap scanline converter.
	  * @param palette16 Pointer to 16-entries array that specifies
	  *   VDP colour index to host pixel mapping.
	  *   This is kept as a pointer, so any changes to the palette
	  *   are immediately picked up by convertLine.
	  *   Used for display modes Graphic4, Graphic5 and Graphic6.
	  * @param palette256 Pointer to 256-entries array that specifies
	  *   VDP colour index to host pixel mapping.
	  *   This is kept as a pointer, so any changes to the palette
	  *   are immediately picked up by convertLine.
	  *   Used for display mode Graphic7.
	  */
	BitmapConverter(const Pixel *palette16, const Pixel *palette256);

	/** Convert a line of V9938 VRAM to 512 host pixels.
	  * Call this method in non-planar display modes (Graphic4 and Graphic5).
	  * @param linePtr Pointer to array of host pixels.
	  * @param vramPtr Pointer to VRAM contents.
	  */
	inline void convertLine(
		Pixel *linePtr, const byte *vramPtr0)
	{
		(this->*renderMethod)(linePtr, vramPtr0, 0);
	}

	/** Convert a line of V9938 VRAM to 512 host pixels.
	  * Call this method in planar display modes (Graphic6 and Graphic7).
	  * @param linePtr Pointer to array of host pixels.
	  * @param vramPtr0 Pointer to VRAM contents, first plane.
	  * @param vramPtr1 Pointer to VRAM contents, second plane.
	  */
	inline void convertLinePlanar(
		Pixel *linePtr, const byte *vramPtr0, const byte *vramPtr1)
	{
		(this->*renderMethod)(linePtr, vramPtr0, vramPtr1);
	}

	/** Select the display mode to use for scanline conversion.
	  * @param mode The new display mode: M5..M1.
	  * TODO: Should this be inlined? It's not used that frequently.
	  */
	inline void setDisplayMode(int mode)
	{
		switch (mode) {
		case 0x0C:
			renderMethod = &BitmapConverter::renderGraphic4;
			break;
		case 0x10:
			renderMethod = &BitmapConverter::renderGraphic5;
			break;
		case 0x14:
			renderMethod = &BitmapConverter::renderGraphic6;
			break;
		case 0x1C:
			renderMethod = &BitmapConverter::renderGraphic7;
			break;
		default:
			renderMethod = &BitmapConverter::renderBogus;
			break;
		}
	}

	void setBlendMask(int blendMask);
	
private:
	inline Pixel blend(byte colour1, byte colour2);

	typedef void (BitmapConverter::*RenderMethod)
		(Pixel *pixelPtr, const byte *vramPtr0, const byte *vramPtr1);

	void renderGraphic4(
		Pixel *pixelPtr, const byte *vramPtr0, const byte *vramPtr1);
	void renderGraphic5(
		Pixel *pixelPtr, const byte *vramPtr0, const byte *vramPtr1);
	void renderGraphic6(
		Pixel *pixelPtr, const byte *vramPtr0, const byte *vramPtr1);
	void renderGraphic7(
		Pixel *pixelPtr, const byte *vramPtr0, const byte *vramPtr1);
	void renderBogus(
		Pixel *pixelPtr, const byte *vramPtr0, const byte *vramPtr1);

	/** Rendering method for the current display mode.
	  */
	RenderMethod renderMethod;

	const Pixel *palette16;
	const Pixel *palette256;

	int blendMask;
};

#endif // __BITMAPCONVERTER_HH__
