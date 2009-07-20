// $Id$

#ifndef LDRENDERER_HH
#define LDRENDERER_HH

#include "openmsx.hh"
#include "EmuTime.hh"

namespace openmsx {

/** Abstract base class for LDRenderers.
  * A LDRenderer is a class that converts VDP state to visual
  * information (for example, pixels on a screen).
  *
  * The update methods are called exactly before the change occurs in
  * the VDP, so that the renderer can update itself to the specified
  * time using the old settings.
  */
class LDRenderer
{
public:
	virtual ~LDRenderer();

	/** Reinitialise LDRenderer state.
	  * @param time The moment in time this reset occurs.
	  */
	virtual void reset(EmuTime::param time) = 0;

	/** Signals the start of a new frame.
	  * The LDRenderer can use this to get fixed-per-frame settings from
	  * the VDP, such as PAL/NTSC timing.
	  * @param time The moment in emulated time the frame starts.
	  */
	virtual void frameStart(EmuTime::param time) = 0;

	/** Signals the end of a frame.
	  * @param time The moment in emulated time the frame ends.
	  *   Note: this is the same time stamp as the start of the next frame.
	  */
	virtual void frameEnd(EmuTime::param time) = 0;

	virtual void drawBlank(int r, int g, int b) = 0;

	virtual void drawBitmap(const byte* frame) = 0;

protected:
	LDRenderer();
};

} // namespace openmsx

#endif
