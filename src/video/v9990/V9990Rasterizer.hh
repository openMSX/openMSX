// $Id$

#ifndef V9990RASTERIZER_HH
#define V9990RASTERIZER_HH

#include "V9990ModeEnum.hh"
#include "openmsx.hh"

namespace openmsx {

/** If this seems awfully familiar, take a look at Rasterizer.hh
  * It's virtually the same class, but for a different video processor.
  */
class V9990Rasterizer
{
public:
	virtual ~V9990Rasterizer() {}

	/** Will the output of this Rasterizer be displayed?
	  * There is no point in producing a frame that will not be displayed.
	  * TODO: Is querying the next pipeline step the best way to solve this,
	  *       or is it better to explicitly disable the first step in the pipeline?
	  */
	virtual bool isActive() = 0;

	/** Resynchronize with VDP - flush caches etc.
	  */
	virtual void reset() = 0;

	/** Indicates the start of a new frame.
	  */
	virtual void frameStart() = 0;

	/** Indicates the end of the current frame.
	  */
	virtual void frameEnd() = 0;

	/** The display mode determines the screens geometry and how V9990
	  * pixels are mapped to pixels on screen.
	  * @param displayMode  The new display mode.
	  */
	virtual void setDisplayMode(enum V9990DisplayMode displayMode) = 0;

	/** The color mode determines how the V9990 VRAM data gets converted
	  * to pixel colors.
	  * @param colorMode  The new color mode.
	  */
	virtual void setColorMode(enum V9990ColorMode colorMode) = 0;

	/** Set RGB values for a palette entry
	  * @param index  Index in palette
	  * @param r      Red component intensity (5 bits)
	  * @param g      Green component intensity (5 bits)
	  * @param b      Blue component intensity (5 bits)
	  */
	virtual void setPalette(int index, byte r, byte g, byte b) = 0;

	/** Render a rectangle of border pixels on the host screen.
	  * The units are absolute lines (Y) and V9990 UC ticks (X).
	  * @param fromX   X coordinate of render start (inclusive).
	  * @param fromY   Y coordinate of render start (inclusive).
	  * @param limitX  X coordinate of render end (exclusive).
	  * @param limitY  Y coordinate of render end (exclusive).
	  */
	virtual void drawBorder(
		int fromX, int fromY,
		int limitX, int limitY) = 0;

	/** Render a rectangle of display pixels on the host screen.
	  * @param fromX    X coordinate of render start in V9990 UC ticks.
	  * @param fromY    Y coordinate of render start in absolute lines.
	  * @param displayX display coordinate of render start: [0..640).
	  * @param displayY display coordinate of render start: [0..480).
	  * @param displayWidth rectangle width in pixels.
	  * @param displayHeight rectangle height in lines.
	  */
	virtual void drawDisplay(
		int fromX, int fromY,
		int displayX, int displayY,
		int displayWidth, int displayHeight) = 0;
};

} // namespace openmsx

#endif
