#ifndef RASTERIZER_HH
#define RASTERIZER_HH

#include "DisplayMode.hh"

#include "EmuTime.hh"

namespace openmsx {

class PostProcessor;
class RawFrame;

class Rasterizer
{
public:
	virtual ~Rasterizer() = default;

	/** See VDP::getPostProcessor(). */
	[[nodiscard]] virtual PostProcessor* getPostProcessor() const = 0;

	/** Will the output of this Rasterizer be displayed?
	  * There is no point in producing a frame that will not be displayed.
	  * TODO: Is querying the next pipeline step the best way to solve this,
	  *       or is it better to explicitly disable the first step in the pipeline?
	  */
	virtual bool isActive() = 0;

	/** Resynchronize with VDP: all cached states are flushed.
	  */
	virtual void reset() = 0;

	/** Indicates the start of a new frame.
	  * The rasterizer can fetch per-frame settings from the VDP.
	  */
	virtual void frameStart(EmuTime time) = 0;

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
	virtual void setPalette(unsigned index, int grb) = 0;

	/** Changes the background color.
	  * @param index Palette index of the new background color.
	  */
	virtual void setBackgroundColor(uint8_t index) = 0;

	virtual void setHorizontalAdjust(int adjust) = 0;
	virtual void setHorizontalScrollLow(uint8_t scroll) = 0;
	virtual void setBorderMask(bool masked) = 0;
	virtual void setTransparency(bool enabled) = 0;
	virtual void setSuperimposeVideoFrame(const RawFrame* videoSource) = 0;

	/** Render a rectangle of border pixels on the host screen.
	  * The units are absolute lines (Y) and VDP clock ticks (X).
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
		int displayWidth, int displayHeight) = 0;

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
		int displayWidth, int displayHeight) = 0;

	/** Is video recording active?
	  */
	[[nodiscard]] virtual bool isRecording() const = 0;

protected:
	Rasterizer() = default;
};

} // namespace openmsx

#endif
