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
	virtual void updateForegroundColour(const EmuTime &time) = NULL;

	/** Informs the renderer of a VDP background colour change.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updateBackgroundColour(const EmuTime &time) = NULL;

	/** Informs the renderer of a VDP display enabled change.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updateDisplayEnabled(const EmuTime &time) = NULL;

	/** Informs the renderer of a VDP display mode change.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updateDisplayMode(const EmuTime &time) = NULL;

	/** Informs the renderer of a name table base address change.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updateNameBase(const EmuTime &time) = NULL;

	/** Informs the renderer of a pattern table base address change.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updatePatternBase(const EmuTime &time) = NULL;

	/** Informs the renderer of a colour table base address change.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updateColourBase(const EmuTime &time) = NULL;

	/** Informs the renderer of a sprite attribute table base address change.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updateSpriteAttributeBase(const EmuTime &time) = NULL;

	/** Informs the renderer of a sprite pattern table base address change.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updateSpritePatternBase(const EmuTime &time) = NULL;

	/** Informs the renderer of a change in VRAM contents.
	  * TODO: Maybe this is a performance problem, if so think of a
	  *   smarter way to update (for example, subscribe to VRAM
	  *   address regions).
	  * @param addr The address that is changed.
	  * @param data The new value.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updateVRAM(int addr, byte data, const EmuTime &time) = NULL;

};

#endif //__RENDERER_HH__

