// $Id$

#ifndef __V9990PIXELRENDERER_HH__
#define __V9990PIXELRENDERER_HH__

#include "V9990.hh"
#include "V9990VRAM.hh"
#include "V9990Rasterizer.hh"
#include "openmsx.hh"

namespace openmsx {

class V9990Rasterizer;
class V9990;
class V9990VRAM;

/** Generic pixel based renderer for the V9990.
  * Uses a rasterizer to plot actual pixels for a specific video system
  *
  * @see PixelRenderer.cc
  */
class V9990PixelRenderer : public V9990Renderer
{
public:
	/** Constructor.
	  */
	V9990PixelRenderer(V9990* vdp_);

	/** Destructor.
	  */
	virtual ~V9990PixelRenderer();

	// V9990Renderer interface:
	void reset(const EmuTime& time);
	void frameStart(const EmuTime& time);
	void frameEnd(const EmuTime& time);
	void renderUntil(const EmuTime& time);
	void setDisplayMode(V9990DisplayMode mode, const EmuTime& time);
	void setColorMode(V9990ColorMode mode, const EmuTime& time);
	void setPalette(int index, byte r, byte g, byte b, const EmuTime& time);
	void setBackgroundColor(int index);
	void setImageWidth(int width);

private:
	/** Type of drawing to do.
	  */
	enum DrawType {
		DRAW_BORDER,
		DRAW_DISPLAY
	};

	/** The V9990 VDP
	  */ 
	V9990* vdp;

	/** The Rasterizer
	  */ 
	V9990Rasterizer* rasterizer;

	/** The last sync point's vertical position. In lines, starting
	  * from VSYNC 
	  */
	int lastY;

	/** The last sync point's horizontal position in UC ticks, starting
	  * from HSYNC
	  */
	int lastX;

	/** Horizontal timing
	  */
	const V9990DisplayPeriod* horTiming;

	/** Vertical timing
	  */
	const V9990DisplayPeriod* verTiming;

	/**
	  */
	void draw(int fromX, int fromY, int toX, int toY, DrawType type);

	/** Render a part of the screen.  Horizontal positions given in UC ticks
	  * starting at HSYNC.  Vertical positions given in lines starting from
	  * VSYNC.  FromX/Y define the rendering start point, toX/Y the end point.
	  * The area to be rendered is limited to the borders set by the clip*
	  * variables.
	  * 
	  * @param fromX    X position on line
	  * @param fromY    Y position on line
	  * @param toX      X position on line
	  * @param toY      Y position on line
	  * @param clipL    Left clipping position
	  * @param clipT    Top clipping position
	  * @param clipR    Right clipping position
	  * @param clipB    Bottom clipping position
	  * @param drawType Draw image or border
	  */
	void render(int fromX, int fromY, int toX, int toY,
			    int clipL, int clipT, int clipR, int clipB,
				DrawType drawType);
}; // class V9990Renderer
} // namespace openmsx
#endif
