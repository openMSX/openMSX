// $Id$

#ifndef RENDERER_HH
#define RENDERER_HH

#include "openmsx.hh"
#include "VRAMObserver.hh"

namespace openmsx {

class EmuTime;
class RenderSettings;
class DisplayMode;

/** Abstract base class for Renderers.
  * A Renderer is a class that converts VDP state to visual
  * information (for example, pixels on a screen).
  *
  * The update methods are called exactly before the change occurs in
  * the VDP, so that the renderer can update itself to the specified
  * time using the old settings.
  */
class Renderer : public VRAMObserver
{
public:
	/** Creates a new Renderer.
	  */
	Renderer();

	/** Destroy this Renderer.
	  */
	virtual ~Renderer();

	/** Reinitialise Renderer state.
	  * @param time The moment in time this reset occurs.
	  */
	virtual void reset(const EmuTime& time) = 0;

	/** Signals the start of a new frame.
	  * The Renderer can use this to get fixed-per-frame settings from
	  * the VDP, such as PAL/NTSC timing.
	  * @param time The moment in emulated time the frame starts.
	  */
	virtual void frameStart(const EmuTime& time) = 0;

	/** Signals the end of a frame.
	  * @param time The moment in emulated time the frame ends.
	  *   Note: this is the same time stamp as the start of the next frame.
	  */
	virtual void frameEnd(const EmuTime& time) = 0;

	/** Informs the renderer of a VDP transparency enable/disable change.
	  * @param enabled The new transparency state.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updateTransparency(bool enabled, const EmuTime& time) = 0;

	/** Informs the renderer of a VDP foreground colour change.
	  * @param colour The new foreground colour.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updateForegroundColour(int colour, const EmuTime& time) = 0;

	/** Informs the renderer of a VDP background colour change.
	  * @param colour The new background colour.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updateBackgroundColour(int colour, const EmuTime& time) = 0;

	/** Informs the renderer of a VDP blink foreground colour change.
	  * @param colour The new blink foreground colour.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updateBlinkForegroundColour(int colour, const EmuTime& time) = 0;

	/** Informs the renderer of a VDP blink background colour change.
	  * @param colour The new blink background colour.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updateBlinkBackgroundColour(int colour, const EmuTime& time) = 0;

	/** Informs the renderer of a VDP blinking state change.
	  * @param enabled The new blink state.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updateBlinkState(bool enabled, const EmuTime& time) = 0;

	/** Informs the renderer of a VDP palette change.
	  * @param index The index [0..15] in the palette that changes.
	  * @param grb The new definition for the changed palette index:
	  *   bit 10..8 is green, bit 6..4 is red and bit 2..0 is blue;
	  *   all other bits are zero.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updatePalette(int index, int grb, const EmuTime& time) = 0;

	/** Informs the renderer of a vertical scroll change.
	  * @param scroll The new scroll value.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updateVerticalScroll(int scroll, const EmuTime& time) = 0;

	/** Informs the renderer of a horizontal scroll change:
	  * the lower scroll value has changed.
	  * @param scroll The new scroll value.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updateHorizontalScrollLow(byte scroll, const EmuTime& time) = 0;

	/** Informs the renderer of a horizontal scroll change:
	  * the higher scroll value has changed.
	  * @param scroll The new scroll value.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updateHorizontalScrollHigh(byte scroll, const EmuTime& time) = 0;

	/** Informs the renderer of a horizontal scroll change:
	  * the border mask has been enabled/disabled.
	  * @param masked true iff enabled.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updateBorderMask(bool masked, const EmuTime& time) = 0;

	/** Informs the renderer of a horizontal scroll change:
	  * the multi page setting has changed.
	  * @param multiPage The new multi page flag.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updateMultiPage(bool multiPage, const EmuTime& time) = 0;

	/** Informs the renderer of a horizontal adjust change.
	  * Note that there is no similar method for vertical adjust updates,
	  * because vertical adjust is calculated at start of frame and
	  * then fixed.
	  * @param adjust The new adjust value.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updateHorizontalAdjust(int adjust, const EmuTime& time) = 0;

	/** Informs the renderer of a VDP display enabled change.
	  * Both the regular border start/end and forced blanking by clearing
	  * the display enable bit are considered display enabled changes.
	  * @param enabled The new display enabled state.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updateDisplayEnabled(bool enabled, const EmuTime& time) = 0;

	/** Informs the renderer of a VDP display mode change.
	  * @param mode The new display mode.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updateDisplayMode(DisplayMode mode, const EmuTime& time) = 0;

	/** Informs the renderer of a name table base address change.
	  * @param addr The new base address.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updateNameBase(int addr, const EmuTime& time) = 0;

	/** Informs the renderer of a pattern table base address change.
	  * @param addr The new base address.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updatePatternBase(int addr, const EmuTime& time) = 0;

	/** Informs the renderer of a colour table base address change.
	  * @param addr The new base address.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updateColourBase(int addr, const EmuTime& time) = 0;

	/** Informs the renderer of a VDP sprites enabled change.
	  * @param enabled The new sprites enabled state.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updateSpritesEnabled(bool enabled, const EmuTime& time) = 0;

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

protected:
	RenderSettings& settings;
};

} // namespace openmsx

#endif
