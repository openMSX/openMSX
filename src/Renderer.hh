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

	/** Informs the renderer of a VDP foreground colour change.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updateForegroundColour(EmuTime &time) = NULL;

	/** Informs the renderer of a VDP background colour change.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updateBackgroundColour(EmuTime &time) = NULL;

	/** Informs the renderer of a VDP display enabled change.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updateDisplayEnabled(EmuTime &time) = NULL;

	/** Informs the renderer of a VDP display mode change.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updateDisplayMode(EmuTime &time) = NULL;

	/** Informs the renderer of a name table base address change.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updateNameBase(EmuTime &time) = NULL;

	/** Informs the renderer of a pattern table base address change.
	  */
	virtual void updatePatternBase(EmuTime &time) = NULL;

	/** Informs the renderer of a colour table base address change.
	  */
	virtual void updateColourBase(EmuTime &time) = NULL;

	/** Informs the renderer of a sprite attribute table base address change.
	  */
	virtual void updateSpriteAttributeBase(EmuTime &time) = NULL;

	/** Informs the renderer of a sprite pattern table base address change.
	  */
	virtual void updateSpritePatternBase(EmuTime &time) = NULL;

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

