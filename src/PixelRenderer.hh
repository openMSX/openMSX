// $Id$

#ifndef __PIXELRENDERER_HH__
#define __PIXELRENDERER_HH__

#include "openmsx.hh"
#include "Renderer.hh"
#include "VDPVRAM.hh"
#include "Command.hh"

class VDP;
class SpriteChecker;


/** Abstract base class for pixel-based Renderers.
  * Provides a framework for implementing a pixel-based Renderer,
  * thereby reducing the amount of code needed to implement one.
  */
class PixelRenderer : public Renderer
{
public:
	// Renderer interface:

	void frameStart(const EmuTime &time);
	void putImage(const EmuTime &time);
	void updateDisplayEnabled(bool enabled, const EmuTime &time);
	/*
	void setFullScreen(bool);
	void updateTransparency(bool enabled, const EmuTime &time);
	void updateForegroundColour(int colour, const EmuTime &time);
	void updateBackgroundColour(int colour, const EmuTime &time);
	void updateBlinkForegroundColour(int colour, const EmuTime &time);
	void updateBlinkBackgroundColour(int colour, const EmuTime &time);
	void updateBlinkState(bool enabled, const EmuTime &time);
	void updatePalette(int index, int grb, const EmuTime &time);
	void updateVerticalScroll(int scroll, const EmuTime &time);
	void updateHorizontalAdjust(int adjust, const EmuTime &time);
	void updateDisplayMode(int mode, const EmuTime &time);
	void updateNameBase(int addr, const EmuTime &time);
	void updatePatternBase(int addr, const EmuTime &time);
	void updateColourBase(int addr, const EmuTime &time);
	*/

	void updateVRAM(int addr, byte data, const EmuTime &time) {
		// TODO: Is it possible to get rid of this method?
		//       One method call is a considerable overhead since VRAM
		//       changes occur pretty often.
		//       For example, register dirty checker at caller.

		// If display is disabled, VRAM changes will not affect the
		// renderer output, therefore sync is not necessary.
		// TODO: Changes in invisible pages do not require sync either.
		//       Maybe this is a task for the dirty checker, because what is
		//       visible is display mode dependant.
		if (vdp->isDisplayEnabled()) renderUntil(time);

		updateVRAMCache(addr, data);
	}

protected:
	/** Constructor.
	  */
	PixelRenderer(VDP *vdp, bool fullScreen, const EmuTime &time);

	/** Destructor.
	  */
	virtual ~PixelRenderer();

	/** Let underlying graphics system finish rendering this frame.
	  */
	virtual void finishFrame() = 0;

	/** Render a rectangle of border pixels on the host screen.
	  * The units are absolute lines (Y) and VDP clockticks (X).
	  * @param fromX X coordinate of render start (inclusive).
	  * @param fromY Y coordinate of render start (inclusive).
	  * @param limitX X coordinate of render end (exclusive).
	  * @param limitY Y coordinate of render end (exclusive).
	  */
	virtual void drawBorder(int fromX, int fromY, int limitX, int limitY) = 0;

	/** Render a rectangle of display pixels on the host screen.
	  * The units are absolute lines (Y) and VDP clockticks (X).
	  * @param fromX X coordinate of render start (inclusive).
	  * @param fromY Y coordinate of render start (inclusive).
	  * @param limitX X coordinate of render end (exclusive).
	  * @param limitY Y coordinate of render end (exclusive).
	  */
	virtual void drawDisplay(int fromX, int fromY, int limitX, int limitY) = 0;

	/** Notifies the VRAM cache of a VRAM write.
	  */
	virtual void updateVRAMCache(int addr, byte data) = 0;

	/** Update renderer state to specified moment in time.
	  * @param time Moment in emulated time to update to.
	  */
	void sync(const EmuTime &time) {
		vram->sync(time);
		renderUntil(time);
	}

	/** Get start of display in ticks since start of line.
	  */
	int getDisplayLeft() {
		return 100 + 102 + 56
			+ (vdp->getHorizontalAdjust() - 7) * 4
			+ (vdp->isTextMode() ? 28 : 0);
	}

	/** The VDP of which the video output is being rendered.
	  */
	VDP *vdp;

	/** The VRAM whose contents are rendered.
	  */
	VDPVRAM *vram;

	/** The sprite checker whose sprites are rendered.
	  */
	SpriteChecker *spriteChecker;

	/** Line to render at top of display.
	  * After all, our screen is 240 lines while display is 262 or 313.
	  */
	int lineRenderTop;

private:
	/** Indicates whether the area to be drawn is border or display. */
	enum DrawType { DRAW_BORDER, DRAW_DISPLAY };

	/** Call the right draw method in the subclass,
	  * depending on passed drawType.
	  */
	inline void draw(
		int startX, int startY, int endX, int endY, DrawType drawType );

	/** Subdivide an area specified by two scan positions into a series of
	  * rectangles.
	  * Clips the rectangles to { (x,y) | clipL <= x < clipR }.
	  * @param drawType
	  *   If DRAW_BORDER, draw rectangles using drawBorder;
	  *   if DRAW_DISPLAY, draw rectangles using drawDisplay.
	  */
	inline void subdivide(
		int startX, int startY, int endX, int endY,
		int clipL, int clipR, DrawType drawType );

	/** Render lines until specified moment in time.
	  * Unlike sync(), this method does not sync with VDPVRAM.
	  * The VRAM should be to be up to date and remain unchanged
	  * from the current time to the specified time.
	  * @param time Moment in emulated time to render lines until.
	  */
	inline void renderUntil(const EmuTime &time);

	/** Is display enabled?
	  * Enabled means the current line is in the display area and
	  * forced blanking is off.
	  */
	bool displayEnabled;

	/** Number of the next position within a line to render.
	  * Expressed in "small pixels" (Text2, Graphics 5/6) from the
	  * left border of the rendered screen.
	  */
	int nextX;

	/** Number of the next line to render.
	  * Expressed in number of lines above lineRenderTop.
	  */
	int nextY;

	/** Absolute line number of first bottom erase line.
	  */
	int lineBottomErase;

	/** Frame skip command
	  */
	class FrameSkipCmd : public Command {
		public:
			FrameSkipCmd(PixelRenderer *rend);
			virtual void execute(const std::vector<std::string> &tokens);
			virtual void help   (const std::vector<std::string> &tokens);
		private:
			PixelRenderer *renderer;
	};
	friend class FrameSkipCmd;
	FrameSkipCmd frameSkipCmd;
	bool autoFrameSkip;
	int frameSkip;
	int curFrameSkip;
	int frameSkipToHigh;
	int frameSkipToLow;
};

#endif //__PIXELRENDERER_HH__

