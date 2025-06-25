#ifndef LDRENDERER_HH
#define LDRENDERER_HH

#include "EmuTime.hh"

namespace openmsx {

class RawFrame;

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
	virtual ~LDRenderer() = default;

	/** Signals the start of a new frame.
	  * The LDRenderer can use this to get fixed-per-frame settings from
	  * the VDP, such as PAL/NTSC timing.
	  * @param time The moment in emulated time the frame starts.
	  */
	virtual void frameStart(EmuTime time) = 0;

	/** Signals the end of a frame.
	  */
	virtual void frameEnd() = 0;

	virtual void drawBlank(int r, int g, int b) = 0;

	[[nodiscard]] virtual RawFrame* getRawFrame() = 0;

protected:
	LDRenderer() = default;
};

} // namespace openmsx

#endif
