#ifndef RENDERER_HH
#define RENDERER_HH

#include "VRAMObserver.hh"
#include "openmsx.hh"
#include <array>
#include <cstdint>

namespace openmsx {

class PostProcessor;
class DisplayMode;
class RawFrame;

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
	virtual ~Renderer() = default;

	/** See VDP::getPostProcessor. */
	[[nodiscard]] virtual PostProcessor* getPostProcessor() const = 0;

	/** Reinitialize Renderer state.
	  */
	virtual void reInit() = 0;

	/** Signals the start of a new frame.
	  * The Renderer can use this to get fixed-per-frame settings from
	  * the VDP, such as PAL/NTSC timing.
	  * @param time The moment in emulated time the frame starts.
	  */
	virtual void frameStart(EmuTime::param time) = 0;

	/** Signals the end of a frame.
	  * @param time The moment in emulated time the frame ends.
	  *   Note: this is the same time stamp as the start of the next frame.
	  */
	virtual void frameEnd(EmuTime::param time) = 0;

	/** Informs the renderer of a VDP transparency enable/disable change.
	  * @param enabled The new transparency state.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updateTransparency(bool enabled, EmuTime::param time) = 0;

	/** Informs the renderer of a VDP superimposing change.
	  * @param videoSource Video that should be superimposed, nullptr if none.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updateSuperimposing(const RawFrame* videoSource,
	                                 EmuTime::param time) = 0;

	/** Informs the renderer of a VDP foreground color change.
	  * @param color The new foreground color.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updateForegroundColor(int color, EmuTime::param time) = 0;

	/** Informs the renderer of a VDP background color change.
	  * @param color The new background color.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updateBackgroundColor(int color, EmuTime::param time) = 0;

	/** Informs the renderer of a VDP blink foreground color change.
	  * @param color The new blink foreground color.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updateBlinkForegroundColor(int color, EmuTime::param time) = 0;

	/** Informs the renderer of a VDP blink background color change.
	  * @param color The new blink background color.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updateBlinkBackgroundColor(int color, EmuTime::param time) = 0;

	/** Informs the renderer of a VDP blinking state change.
	  * @param enabled The new blink state.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updateBlinkState(bool enabled, EmuTime::param time) = 0;

	/** Informs the renderer of a VDP palette change.
	  * @param index The index [0..15] in the palette that changes.
	  * @param grb The new definition for the changed palette index:
	  *   bit 10..8 is green, bit 6..4 is red and bit 2..0 is blue;
	  *   all other bits are zero.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updatePalette(unsigned index, int grb, EmuTime::param time) = 0;

	/** Informs the renderer of a vertical scroll change.
	  * @param scroll The new scroll value.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updateVerticalScroll(int scroll, EmuTime::param time) = 0;

	/** Informs the renderer of a horizontal scroll change:
	  * the lower scroll value has changed.
	  * @param scroll The new scroll value.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updateHorizontalScrollLow(byte scroll, EmuTime::param time) = 0;

	/** Informs the renderer of a horizontal scroll change:
	  * the higher scroll value has changed.
	  * @param scroll The new scroll value.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updateHorizontalScrollHigh(byte scroll, EmuTime::param time) = 0;

	/** Informs the renderer of a horizontal scroll change:
	  * the border mask has been enabled/disabled.
	  * @param masked true iff enabled.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updateBorderMask(bool masked, EmuTime::param time) = 0;

	/** Informs the renderer of a horizontal scroll change:
	  * the multi page setting has changed.
	  * @param multiPage The new multi page flag.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updateMultiPage(bool multiPage, EmuTime::param time) = 0;

	/** Informs the renderer of a horizontal adjust change.
	  * Note that there is no similar method for vertical adjust updates,
	  * because vertical adjust is calculated at start of frame and
	  * then fixed.
	  * @param adjust The new adjust value.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updateHorizontalAdjust(int adjust, EmuTime::param time) = 0;

	/** Informs the renderer of a VDP display enabled change.
	  * Both the regular border start/end and forced blanking by clearing
	  * the display enable bit are considered display enabled changes.
	  * @param enabled The new display enabled state.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updateDisplayEnabled(bool enabled, EmuTime::param time) = 0;

	/** Informs the renderer of a VDP display mode change.
	  * @param mode The new display mode.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updateDisplayMode(DisplayMode mode, EmuTime::param time) = 0;

	/** Informs the renderer of a name table base address change.
	  * @param addr The new base address.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updateNameBase(unsigned addr, EmuTime::param time) = 0;

	/** Informs the renderer of a pattern table base address change.
	  * @param addr The new base address.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updatePatternBase(unsigned addr, EmuTime::param time) = 0;

	/** Informs the renderer of a color table base address change.
	  * @param addr The new base address.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updateColorBase(unsigned addr, EmuTime::param time) = 0;

	/** Informs the renderer of a VDP sprites enabled change.
	  * @param enabled The new sprites enabled state.
	  * @param time The moment in emulated time this change occurs.
	  */
	virtual void updateSpritesEnabled(bool enabled, EmuTime::param time) = 0;

	/** Sprite palette in Graphic 7 mode.
          * See page 98 of the V9938 data book.
	  * Each palette entry is a word in GRB format:
	  * bit 10..8 is green, bit 6..4 is red and bit 2..0 is blue.
	  */
	static constexpr std::array<uint16_t, 16> GRAPHIC7_SPRITE_PALETTE = {
		0x000, 0x002, 0x030, 0x032, 0x300, 0x302, 0x330, 0x332,
		0x472, 0x007, 0x070, 0x077, 0x700, 0x707, 0x770, 0x777,
	};

protected:
	Renderer() = default;
};

} // namespace openmsx

#endif
