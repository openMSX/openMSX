// $Id$

#ifndef __RENDERER_HH__
#define __RENDERER_HH__

#include "openmsx.hh"

class EmuTime;

/** Abstract base class for Renderers.
  * A Renderer is a class that converts VDP state to visual
  * information (for example, pixels on a screen).
  */
class Renderer
{
public:

	/** NTSC version of the MSX1 palette.
	  * An array of 16 RGB triples.
	  * Each component ranges from 0 (off) to 255 (full intensity).
	  */
	static const byte TMS99X8A_PALETTE[16][3];

	/** TODO: Soon to be replaced by pixel-based update method.
	  *   Not sure yet how to handle PAL/NTSC differences.
	  *   Renderer should care as little as possible, but how
	  *   little is that?
	  */
	//virtual void fullScreenRefresh() = 0;

	/** Puts the generated image on the screen.
	  * May wait for a suitable moment to do so (vsync).
	  */
	virtual void putImage() = 0;

	/** Render full screen or windowed?
	  * This is a hint to the renderer, not all renderers will
	  * support both modes.
	  */
	virtual void setFullScreen(bool) = 0;

	/** Informs the renderer of a VDP foreground colour change.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updateForegroundColour(const EmuTime &time) = 0;

	/** Informs the renderer of a VDP background colour change.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updateBackgroundColour(const EmuTime &time) = 0;

	/** Informs the renderer of a VDP blinking state change.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updateBlinkState(const EmuTime &time) = 0;

	/** Informs the renderer of a VDP palette change.
	  * @param index The index [0..15] in the palette that changed.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updatePalette(int index, const EmuTime &time) = 0;

	/** Informs the renderer of a VDP display enabled change.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updateDisplayEnabled(const EmuTime &time) = 0;

	/** Informs the renderer of a VDP display mode change.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updateDisplayMode(const EmuTime &time) = 0;

	/** Informs the renderer of a name table base address change.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updateNameBase(const EmuTime &time) = 0;

	/** Informs the renderer of a pattern table base address change.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updatePatternBase(const EmuTime &time) = 0;

	/** Informs the renderer of a colour table base address change.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updateColourBase(const EmuTime &time) = 0;

	/** Informs the renderer of a sprite attribute table base address change.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updateSpriteAttributeBase(const EmuTime &time) = 0;

	/** Informs the renderer of a sprite pattern table base address change.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updateSpritePatternBase(const EmuTime &time) = 0;

	/** Informs the renderer of a change in VRAM contents.
	  * TODO: Maybe this is a performance problem, if so think of a
	  *   smarter way to update (for example, subscribe to VRAM
	  *   address regions).
	  * @param addr The address that is changed.
	  * @param data The new value.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updateVRAM(int addr, byte data, const EmuTime &time) = 0;

};

#endif //__RENDERER_HH__

