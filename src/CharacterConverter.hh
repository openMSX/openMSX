// $Id$

#ifndef __CHARACTERCONVERTER_HH__
#define __CHARACTERCONVERTER_HH__

class VDP;
class VDPVRAM;

#include "openmsx.hh"
#include "Renderer.hh"
#include <cassert>

/** Utility class for converting VRAM contents to host pixels.
  */
template <class Pixel, Renderer::Zoom zoom>
class CharacterConverter
{
public:
	/** Create a new bitmap scanline converter.
	  * @param palFg Pointer to 16-entries array that specifies
	  *   VDP foreground colour index to host pixel mapping.
	  *   This is kept as a pointer, so any changes to the palette
	  *   are immediately picked up by convertLine.
	  * @param palBg Pointer to 16-entries array that specifies
	  *   VDP background colour index to host pixel mapping.
	  *   This is kept as a pointer, so any changes to the palette
	  *   are immediately picked up by convertLine.
	  */
	CharacterConverter(VDP *vdp, const Pixel *palFg, const Pixel *palBg);

	/** Convert a line of V9938 VRAM to 512 host pixels.
	  * Call this method in non-planar display modes (Graphic4 and Graphic5).
	  * @param linePtr Pointer to array of host pixels.
	  * @param namePtr Pointer to name table in VRAM.
	  */
	inline void convertLine(
		Pixel *linePtr, int line)
	{
		(this->*renderMethod)(linePtr, line);
	}

	/** Select the display mode to use for scanline conversion.
	  * @param mode The new display mode: M5..M1.
	  * TODO: Should this be inlined? It's not used that frequently.
	  */
	inline void setDisplayMode(int mode)
	{
		assert(mode < 0x0C);
		renderMethod = modeToRenderMethod[mode];
	}

private:
	typedef void (CharacterConverter::*RenderMethod)
		(Pixel *pixelPtr, int line);

	/** RenderMethods for each display mode.
	  */
	static RenderMethod modeToRenderMethod[];

	void renderText1(Pixel *pixelPtr, int line);
	void renderText1Q(Pixel *pixelPtr, int line);
	void renderText2(Pixel *pixelPtr, int line);
	void renderGraphic1(Pixel *pixelPtr, int line);
	void renderGraphic2(Pixel *pixelPtr, int line);
	void renderMulti(Pixel *pixelPtr, int line);
	void renderMultiQ(Pixel *pixelPtr, int line);
	void renderBogus(Pixel *pixelPtr, int line);

	/** Rendering method for the current display mode.
	  */
	RenderMethod renderMethod;

	const Pixel *palFg;
	const Pixel *palBg;

	VDP *vdp;
	VDPVRAM *vram;

	/** Dirty tables indicate which character blocks must be repainted.
	  * The anyDirty variables are true when there is at least one
	  * element in the dirty table that is true.
	  * TODO:
	  * Change tracking is currently disabled, so every change variable
	  * is set to true. In the future change tracking will be restored.
	  */
	bool anyDirtyColour, dirtyColour[1 << 10];
	bool anyDirtyPattern, dirtyPattern[1 << 10];
	bool anyDirtyName, dirtyName[1 << 12];
	bool dirtyForeground, dirtyBackground;
};

#endif // __CHARACTERCONVERTER_HH__
