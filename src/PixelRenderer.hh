// $Id$

#ifndef __PIXELRENDERER_HH__
#define __PIXELRENDERER_HH__

#include "openmsx.hh"
#include "Renderer.hh"
#include "VDPVRAM.hh"

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
	void updateDisplayEnabled(bool enabled, const EmuTime &time);
	void updateDisplayMode(int mode, const EmuTime &time);
	void updateNameBase(int addr, const EmuTime &time);
	void updatePatternBase(int addr, const EmuTime &time);
	void updateColourBase(int addr, const EmuTime &time);
	void updateVRAM(int addr, byte data, const EmuTime &time);
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

	/** TODO: Temporary way to access PhaseHandler.
	  */
	virtual void drawArea(int fromX, int fromY, int limitX, int limitY) = 0;

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
	/** Render lines until specified moment in time.
	  * Unlike sync(), this method does not sync with VDPVRAM.
	  * The VRAM should be to be up to date and remain unchanged
	  * from the current time to the specified time.
	  * @param time Moment in emulated time to render lines until.
	  */
	inline void renderUntil(const EmuTime &time);

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

};

#endif //__PIXELRENDERER_HH__

