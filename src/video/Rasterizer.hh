// $Id$

#ifndef __RASTERIZER_HH__
#define __RASTERIZER_HH__

#include "VideoLayer.hh"
#include "DisplayMode.hh"


namespace openmsx {

class Rasterizer: public VideoLayer
{
public:
	/** Destructor.
	  */
	virtual ~Rasterizer() {}

	/** Resynchronize with VDP: all cached states are flushed.
	  */
	virtual void reset() = 0;

	/** Indicates the start of a new frame.
	  * The rasterizer can fetch per-frame settings from the VDP.
	  */
	virtual void frameStart() = 0;

	/** Indicates the end of the current frame.
	  * The rasterizer can perform image post processing.
	  */
	virtual void frameEnd() = 0;

	/** Precalc several values that depend on the display mode.
	  * @param mode The new display mode.
	  */
	virtual void setDisplayMode(DisplayMode mode) = 0;

	/** Change an entry in the palette.
	  * @param index The index [0..15] in the palette that changes.
	  * @param grb The new definition for the changed palette index:
	  *   bit 10..8 is green, bit 6..4 is red and bit 2..0 is blue;
	  *   all other bits are zero.
	  */
	virtual void setPalette(int index, int grb) = 0;

	/** Changes the background colour.
	  * @param index Palette index of the new background colour.
	  */
	virtual void setBackgroundColour(int index) = 0;

	virtual void setTransparency(bool enabled) = 0;

	/** Notifies the VRAM cache of a VRAM write.
	  * @param address The address that changed.
	  */
	virtual void updateVRAMCache(int address) = 0;

	/** Render a rectangle of border pixels on the host screen.
	  * The units are absolute lines (Y) and VDP clockticks (X).
	  * @param fromX X coordinate of render start (inclusive).
	  * @param fromY Y coordinate of render start (inclusive).
	  * @param limitX X coordinate of render end (exclusive).
	  * @param limitY Y coordinate of render end (exclusive).
	  */
	virtual void drawBorder(int fromX, int fromY, int limitX, int limitY) = 0;

	/** Render a rectangle of display pixels on the host screen.
	  * @param fromX X coordinate of render start in VDP ticks.
	  * @param fromY Y coordinate of render start in absolute lines.
	  * @param displayX display coordinate of render start: [0..512).
	  * @param displayY display coordinate of render start: [0..256).
	  * @param displayWidth rectangle width in pixels (512 per line).
	  * @param displayHeight rectangle height in lines.
	  */
	virtual void drawDisplay(
		int fromX, int fromY,
		int displayX, int displayY,
		int displayWidth, int displayHeight
		) = 0;

	/** Render a rectangle of sprite pixels on the host screen.
	  * Although the parameters are very similar to drawDisplay,
	  * the displayX and displayWidth use range [0..256) instead of
	  * [0..512) because VDP sprite coordinates work that way.
	  * @param fromX X coordinate of render start in VDP ticks.
	  * @param fromY Y coordinate of render start in absolute lines.
	  * @param displayX display coordinate of render start: [0..256).
	  * @param displayY display coordinate of render start: [0..256).
	  * @param displayWidth rectangle width in pixels (256 per line).
	  * @param displayHeight rectangle height in lines.
	  */
	virtual void drawSprites(
		int fromX, int fromY,
		int displayX, int displayY,
		int displayWidth, int displayHeight
		) = 0;

protected:
	Rasterizer() : VideoLayer(RenderSettings::VIDEO_MSX) {}

};

} // namespace openmsx

#endif //__RASTERIZER_HH__
