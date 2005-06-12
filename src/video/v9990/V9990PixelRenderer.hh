// $Id$

#ifndef V9990PIXELRENDERER_HH
#define V9990PIXELRENDERER_HH

#include "V9990Renderer.hh"
#include "SettingListener.hh"
#include "RenderSettings.hh"
#include "openmsx.hh"

namespace openmsx {

class V9990;
class V9990Rasterizer;

/** Generic pixel based renderer for the V9990.
  * Uses a rasterizer to plot actual pixels for a specific video system
  *
  * @see PixelRenderer.cc
  */
class V9990PixelRenderer : public V9990Renderer, private SettingListener
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
	void updateDisplayEnabled(bool enabled, const EmuTime& time);
	void setDisplayMode(V9990DisplayMode mode, const EmuTime& time);
	void setColorMode(V9990ColorMode mode, const EmuTime& time);
	void updatePalette(int index, byte r, byte g, byte b, const EmuTime& time);
	void updateBackgroundColor(int index, const EmuTime& time);
	void setImageWidth(int width);
	void updateScrollAX(const EmuTime& time);
	void updateScrollAY(const EmuTime& time);
	void updateScrollBX(const EmuTime& time);
	void updateScrollBY(const EmuTime& time);

private:
	void sync(const EmuTime& time, bool force = false);
	void renderUntil(const EmuTime& time);

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

	/** Accuracy setting for current frame.
	 */
	RenderSettings::Accuracy accuracy;

	/** Is display enabled?
	  * Enabled means the current line is in the display area and
	  * forced blanking is off.
	  */
	bool displayEnabled;

	/** The last sync point's vertical position. In lines, starting
	  * from VSYNC
	  */
	int lastY;

	/** The last sync point's horizontal position in UC ticks, starting
	  * from HSYNC
	  */
	int lastX;

	/** Should current frame be draw or can it be skipped.
	  */
	bool drawFrame;

	/** Frameskip
	  */
	int frameSkipCounter;
	double finishFrameDuration;

	/**
	  */
	void draw(int fromX, int fromY, int toX, int toY, DrawType type);

	/** Subdivide an area specified by two scan positions into a series of
	  * rectangles
	  */
	void subdivide(int fromX, int fromY, int toX, int toY,
	               int clipL, int clipR, DrawType drawType);

	// SettingListener
	virtual void update(const Setting* setting);
};

} // namespace openmsx

#endif
