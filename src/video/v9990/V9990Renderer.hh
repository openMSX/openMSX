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
	/** Create a new V9990Renderer.
	  */ 
	V9990Renderer();

	/** Destroy this V9990Renderer.
	  */
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

	/** Set image width
	  */
	virtual void setImageWidth(int width) = 0;

protected:
	RenderSettings& settings;
};

} // namespace openmsx

#endif
