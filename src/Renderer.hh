// $Id$

#ifndef __RENDERER_HH__
#define __RENDERER_HH__

class EmuTime;

class Renderer
{
public:

	/** TODO: Soon to be replaced by pixel-based update method.
	  *   Not sure yet how to handle PAL/NTSC differences.
	  *   Renderer should care as little as possible, but how
	  *   little is that?
	  */
	//virtual void fullScreenRefresh() = NULL;

	/** Puts the generated image on the screen.
	  * May wait for a suitable moment to do so (vsync).
	  */
	virtual void putImage() = NULL;

	/** Render full screen or windowed?
	  * This is a hint to the renderer, not all renderers will
	  * support both modes.
	  */
	virtual void setFullScreen(bool) = NULL;

	/** Informs the renderer of a VDP background colour change.
	  * @param colour VDP palette index (0..15) of the new background
	  *    colour.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updateBackgroundColour(int colour, EmuTime &time) = NULL;

	/** Informs the renderer of a VDP blanking change.
	  * @param enabled Is blanking enabled?
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updateBlanking(bool enabled, EmuTime &time) = NULL;

	/** Informs the renderer of a VDP display mode change.
	  * @param mode The new display mode (M2..M0).
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updateDisplayMode(int mode, EmuTime &time) = NULL;

	/** Informs the renderer of a name table base address change.
	  * @param addr The new base address.
	  */
	virtual void updateNameBase(int addr, EmuTime &time) = NULL;

	/** Informs the renderer of a pattern table base address change.
	  * @param addr The new base address.
	  * @param mask Pattern table mask, or -1 for N/A.
	  *   Note that only the undocumented display modes have masks.
	  */
	virtual void updatePatternBase(int addr, int mask, EmuTime &time) = NULL;

	/** Informs the renderer of a colour table base address change.
	  * @param addr The new base address.
	  * @param mask Colour table mask, or -1 for N/A.
	  *   Note that only the undocumented display modes have masks.
	  */
	virtual void updateColourBase(int addr, int mask, EmuTime &time) = NULL;

	/** Informs the renderer of a sprite attribute table base address change.
	  * @param addr The new base address.
	  */
	virtual void updateSpriteAttributeBase(int addr, EmuTime &time) = NULL;

	/** Informs the renderer of a sprite pattern table base address change.
	  * @param addr The new base address.
	  */
	virtual void updateSpritePatternBase(int addr, EmuTime &time) = NULL;

	/** Informs the renderer of a change in VRAM contents.
	  * @param addr The address that is changed.
	  * @param data The new value.
	  * @param time The moment in emulated time this change occurs.
	  * TODO: Maybe this is a performance problem, if so think of a
	  *   smarter way to update (for example, subscribe to VRAM
	  *   address regions).
	  */
	virtual void updateVRAM(int addr, byte data, EmuTime &time) = NULL;

};

#endif //__RENDERER_HH__

