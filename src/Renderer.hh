// $Id$

#ifndef __RENDERER_HH__
#define __RENDERER_HH__

#include "openmsx.hh"
#include "Command.hh"

class EmuTime;
class RenderSettings;


/** Abstract base class for Renderers.
  * A Renderer is a class that converts VDP state to visual
  * information (for example, pixels on a screen).
  *
  * The update methods are called exactly before the change occurs in
  * the VDP, so that the renderer can update itself to the specified
  * time using the old settings.
  */
class Renderer
{
public:

	/** Zoom style used by MSX VRAM to host pixel converters.
	  * TODO: I'm not sure Renderer is the right class for this,
	  *       but I couldn't find a better place.
	  */
	enum Zoom {
		/** Do not zoom: one MSX pixel is converted to one host pixel.
		  * Line width depends on display mode.
		  */
		ZOOM_REAL,
		/** One MSX pixel line is converted to 256 host pixels.
		  */
		ZOOM_256,
		/** One MSX pixel line is converted to 512 host pixels.
		  */
		ZOOM_512
	};

	/** Creates a new Renderer.
	  * @param fullScreen Start in full screen or windowed;
	  *   true iff full screen.
	  */
	Renderer(bool fullScreen);

	/** Destroy this Renderer.
	  */
	virtual ~Renderer();

	/** Reset
	  * @param time The moment in time this reset occurs.
	  */
	virtual void reset(const EmuTime &time) = 0;

	/** Signals the start of a new frame.
	  * The Renderer can use this to get fixed-per-frame settings from
	  * the VDP, such as PAL/NTSC timing.
	  * @param time The moment in emulated time the frame starts.
	  */
	virtual void frameStart(const EmuTime &time) = 0;

	/** Puts the generated image on the screen.
	  * @param time The moment in emulated time the frame is finished.
	  */
	virtual void putImage(const EmuTime &time) = 0;

	/** Informs the renderer of a VDP transparency enable/disable change.
	  * @param enabled The new transparency state.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updateTransparency(bool enabled, const EmuTime &time) = 0;

	/** Informs the renderer of a VDP foreground colour change.
	  * @param colour The new foreground colour.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updateForegroundColour(int colour, const EmuTime &time) = 0;

	/** Informs the renderer of a VDP background colour change.
	  * @param colour The new background colour.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updateBackgroundColour(int colour, const EmuTime &time) = 0;

	/** Informs the renderer of a VDP blink foreground colour change.
	  * @param colour The new blink foreground colour.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updateBlinkForegroundColour(int colour, const EmuTime &time) = 0;

	/** Informs the renderer of a VDP blink background colour change.
	  * @param colour The new blink background colour.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updateBlinkBackgroundColour(int colour, const EmuTime &time) = 0;

	/** Informs the renderer of a VDP blinking state change.
	  * @param enabled The new blink state.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updateBlinkState(bool enabled, const EmuTime &time) = 0;

	/** Informs the renderer of a VDP palette change.
	  * @param index The index [0..15] in the palette that changes.
	  * @param grb The new definition for the changed palette index:
	  *   bit 10..8 is green, bit 6..4 is red and bit 2..0 is blue;
	  *   all other bits are zero.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updatePalette(int index, int grb, const EmuTime &time) = 0;

	/** Informs the renderer of a vertical scroll change.
	  * @param scroll The new scroll value.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updateVerticalScroll(int scroll, const EmuTime &time) = 0;

	/** Informs the renderer of a horizontal adjust change.
	  * Note that there is no similar method for vertical adjust updates,
	  * because vertical adjust is calculated at start of frame and
	  * then fixed.
	  * @param adjust The new adjust value.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updateHorizontalAdjust(int adjust, const EmuTime &time) = 0;

	/** Informs the renderer of a VDP display enabled change.
	  * Both the regular border start/end and forced blanking by clearing
	  * the display enable bit are considered display enabled changes.
	  * @param enabled The new display enabled state.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updateDisplayEnabled(bool enabled, const EmuTime &time) = 0;

	/** Informs the renderer of a VDP display mode change.
	  * @param mode The new display mode: M5..M1.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updateDisplayMode(int mode, const EmuTime &time) = 0;

	/** Informs the renderer of a name table base address change.
	  * @param addr The new base address.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updateNameBase(int addr, const EmuTime &time) = 0;

	/** Informs the renderer of a pattern table base address change.
	  * @param addr The new base address.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updatePatternBase(int addr, const EmuTime &time) = 0;

	/** Informs the renderer of a colour table base address change.
	  * @param addr The new base address.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updateColourBase(int addr, const EmuTime &time) = 0;

	/** Informs the renderer of a change in VRAM contents.
	  * TODO: Maybe this is a performance problem, if so think of a
	  *   smarter way to update (for example, subscribe to VRAM
	  *   address regions).
	  * @param addr The address that will change.
	  * @param data The new value.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updateVRAM(int addr, byte data, const EmuTime &time) = 0;

	/** Check whether the full screen RendererSetting changed and adjust
	  * the display mode if necessary.
	  * TODO: Make this into a generic renderer change check.
	  * TODO: On Windows, changing the full screen setting requires
	  *       creation of a new Renderer instance.
	  * TODO: Full screen should not be part of the Renderer interface.
	  */
	void checkFullScreen();

protected:
	/** NTSC version of the MSX1 palette.
	  * An array of 16 RGB triples.
	  * Each component ranges from 0 (off) to 255 (full intensity).
	  */
	static const byte TMS99X8A_PALETTE[16][3];

	/** Sprite palette in Graphic 7 mode.
	  * Each palette entry is a word in GRB format:
	  * bit 10..8 is green, bit 6..4 is red and bit 2..0 is blue.
	  */
	static const word GRAPHIC7_SPRITE_PALETTE[16];

	/** Render full screen or windowed?
	  * This is a hint to the renderer, not all renderers will
	  * support both modes.
	  * A renderer that does should override this method
	  * and call the superclass implementation.
	  * TODO: This is a platform specific issue and should therefore
	  *       not be in the Renderer interface.
	  *       For example a Renderer on a PDA may not have a full screen
	  *       mode, but would have a rotate setting.
	  *       Also, on some platforms switching from windowed to full
	  *       screen would require a different Renderer to be hooked
	  *       up to the VDP.
	  *       However, until a proper location is found, keeping it here
	  *       is at least better than in the VDP, where it was before.
	  * @param enabled true iff full screen.
	  */
	virtual void setFullScreen(bool enabled);

	RenderSettings *settings;

private:

	/** Render full screen or windowed?
	  * This is separated from the full screen user preference in
	  * RenderSettings: the setting is what the user want, this field
	  * stores the current situation and the checkFullScreen method
	  * synchronizes between them.
	  * TODO:
	  * This does not really belong here, see comments for checkFullScreen.
	  */
	bool fullScreen;

};

#endif //__RENDERER_HH__

