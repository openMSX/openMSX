#ifndef V9990RENDERER_HH
#define V9990RENDERER_HH

#include "V9990ModeEnum.hh"
#include "EmuTime.hh"

#include <cstdint>

namespace openmsx {

class PostProcessor;

/** Abstract base class for V9990 renderers.
  * A V9990Renderer is a class that converts the V9990 state into
  * visual information (e.g. pixels on a screen).
  *
  * @see Renderer.hh
  */
class V9990Renderer
{
public:
	virtual ~V9990Renderer() = default;

	/** See V9990::getPostProcessor. */
	[[nodiscard]] virtual PostProcessor* getPostProcessor() const = 0;

	/** Re-initialise the V9990Renderer's state.
	  * @param time The moment in emulated time this reset occurs.
	  */
	virtual void reset(EmuTime::param time) = 0;

	/** Signal the start of a new frame.
	  * The V9990Renderer can use this to get fixed-per-frame
	  * settings from the V9990 VDP. Typical settings include:
	  * - PAL/NTSC timing
	  * - MCLK/XTAL selection
	  * @param time The moment in emulated time the frame starts.
	  */
	virtual void frameStart(EmuTime::param time) = 0;

	/** Signal the end of the current frame.
	  * @param time The moment in emulated time the frame ends.
	  */
	virtual void frameEnd(EmuTime::param time) = 0;

	/** Render until the given point in emulated time
	  * @param time The moment in emulated time the frame ends.
	  */
	virtual void renderUntil(EmuTime::param time) = 0;

	/** Informs the renderer of a VDP display enabled change.
	 *  Both the regular border start/end and forced blanking by clearing
	 *  the display enable bit are considered display enabled changes.
	 *  @param enabled The new display enabled state.
	 *  @param time The moment in emulated time this change occurs.
	 */
	virtual void updateDisplayEnabled(bool enabled, EmuTime::param time) = 0;

	/** Set screen mode. */
	virtual void setDisplayMode(V9990DisplayMode mode,
	                            EmuTime::param time) = 0;

	/** Set color mode. */
	virtual void setColorMode(V9990ColorMode mode,
	                          EmuTime::param time) = 0;

	/** Set a palette entry. */
	virtual void updatePalette(int index, uint8_t r, uint8_t g, uint8_t b, bool ys,
	                        EmuTime::param time) = 0;

	/** Change superimpose status. */
	virtual void updateSuperimposing(bool enabled, EmuTime::param time) = 0;

	/** Set background color. */
	virtual void updateBackgroundColor(int index, EmuTime::param time) = 0;

	/** Set scroll register. */
	virtual void updateScrollAX(EmuTime::param time) = 0;
	virtual void updateScrollBX(EmuTime::param time) = 0;
	virtual void updateScrollAYLow(EmuTime::param time) = 0;
	virtual void updateScrollBYLow(EmuTime::param time) = 0;

protected:
	V9990Renderer() = default;
};

} // namespace openmsx

#endif
