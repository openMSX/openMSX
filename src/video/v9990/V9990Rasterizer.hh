// $Id$

#ifndef __V9990RASTERIZER_HH__
#define __V9990RASTERIZER_HH__

#include "Display.hh"
#include "V9990.hh"

namespace openmsx {

/*
 * If this seems awfully familiar, take a look at Rasterizer.hh
 * It's virtually the same class, but for a different video processor.
 */
		
class V9990Rasterizer : public Layer
{
public:

	/** Destructor
	  */
	virtual ~Rasterizer() {};

	/** Resynchronize with VDP - flush caches etc
	  */
	virtual void reset() = 0;

	/** Indicates the start of a new frame.
	  */
	virtual void frameStart() = 0;

	/** Indicates the end of the current frame.
	  */ 
	virtual void frameEnd() = 0;

	/** The display mode determines the screens geometry and how V9990
	  * pixels are mapped to pixels on screen 
	  * @param mode  The new display mode
	  */
	virtual void setDisplayMode(V9990DisplayMode display_mode) = 0;

	/** The color mode determines how the V9990 VRAM data gets converted
	  * to pixel colors.
	  * @param mode  The new color mode
	  */
	virtual void setColorMode(V9990ColorMode color_mode) = 0;
	
	/** Render a rectangle of border pixels on the host screen.
	  * The units are absolute lines (Y) and VDP clockticks (X).
	  * @param fromX   X coordinate of render start (inclusive).
	  * @param fromY   Y coordinate of render start (inclusive).
	  * @param limitX  X coordinate of render end (exclusive).
	  * @param limitY  Y coordinate of render end (exclusive).
	  */
	virtual void drawBorder(
		int fromX, int fromY,
		int limitX, int limitY) = 0;

	/** Render a rectangle of display pixels on the host screen.
	  * @param fromX    X coordinate of render start in VDP ticks.
	  * @param fromY    Y coordinate of render start in absolute lines.
	  * @param displayX display coordinate of render start: [0..640)
	  * @param displayY display coordinate of render start: [0..480).
	  * @param displayWidth rectangle width in pixels (512 per line).
	  * @param displayHeight rectangle height in lines.
	  */
	virtual void drawDisplay(
		int fromX, int fromY,
		int displayX, int displayY,
		int displayWidth, int displayHeight
		) = 0;
};

} // V9990Rasterizer

} // namespace openmsx
#endif
