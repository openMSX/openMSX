// $Id$

#ifndef V9990RENDERER_HH
#define V9990RENDERER_HH

#include "openmsx.hh"
#include "V9990ModeEnum.hh"

namespace openmsx {

class EmuTime;
class RenderSettings;

/** Abstract base class for V9990 renderers.
  * A V9990Renderer is a class that covnerts the V9990 state into
  * visual information (e.g. pixels on a screen).
  *
  * @see Renderer.cc
  */
class V9990Renderer
{
public:
	virtual ~V9990Renderer();

	/** Re-initialise the V9990Renderer's state.
	  * @param time The moment in emulated time this reset occurs.
	  */
	virtual void reset(const EmuTime& time) = 0;

	/** Signal the start of a new frame.
	  * The V9990Renderer can use this to get fixed-per-frame
	  * settings from the V9990 VDP. Typical settings include:
	  * - PAL/NTSC timing
	  * - MCLK/XTAL selection
	  * @param time The moment in emulated time the frame starts.
	  */
	virtual void frameStart(const EmuTime& time) = 0;

	/** Signal the end of the current frame.
	  * @param time The moment in emulated time the frame ends.
	  */
	virtual void frameEnd(const EmuTime& time) = 0;

	/** Render until the given point in emulated time
	  * @param time The moment in emulated time the frame ends.
	  */
	virtual void renderUntil(const EmuTime& time) = 0;

	/** Informs the renderer of a VDP display enabled change.
	 *  Both the regular border start/end and forced blanking by clearing
	 *  the display enable bit are considered display enabled changes.
	 *  @param enabled The new display enabled state.
	 *  @param time The moment in emulated time this change occurs.
	 */
	virtual void updateDisplayEnabled(bool enabled, const EmuTime& time) = 0;

	/** Set screen mode
	  */
	virtual void setDisplayMode(V9990DisplayMode mode,
	                            const EmuTime& time) = 0;

	/** Set color mode
	  */
	virtual void setColorMode(V9990ColorMode mode,
	                          const EmuTime& time) = 0;

	/** Set a palette entry
	  */
	virtual void updatePalette(int index, byte r, byte g, byte b,
	                        const EmuTime& time) = 0;

	/** Set background color
	  */
	virtual void updateBackgroundColor(int index, const EmuTime& time) = 0;

	/** Set scroll register
	 */
	virtual void updateScrollAX(const EmuTime& time) = 0;
	virtual void updateScrollAY(const EmuTime& time) = 0;
	virtual void updateScrollBX(const EmuTime& time) = 0;
	virtual void updateScrollBY(const EmuTime& time) = 0;

protected:
	V9990Renderer();
};

} // namespace openmsx

#endif
