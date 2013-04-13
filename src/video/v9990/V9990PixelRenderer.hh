// $Id$

#ifndef V9990PIXELRENDERER_HH
#define V9990PIXELRENDERER_HH

#include "V9990Renderer.hh"
#include "Observer.hh"
#include "RenderSettings.hh"
#include "openmsx.hh"
#include "noncopyable.hh"
#include <memory>

namespace openmsx {

class V9990;
class V9990Rasterizer;
class Setting;
class EventDistributor;
class RealTime;
class VideoSourceSetting;

/** Generic pixel based renderer for the V9990.
  * Uses a rasterizer to plot actual pixels for a specific video system
  *
  * @see PixelRenderer.cc
  */
class V9990PixelRenderer : public V9990Renderer, private Observer<Setting>,
                           private noncopyable
{
public:
	explicit V9990PixelRenderer(V9990& vdp);
	virtual ~V9990PixelRenderer();

	// V9990Renderer interface:
	void reset(EmuTime::param time);
	void frameStart(EmuTime::param time);
	void frameEnd(EmuTime::param time);
	void updateDisplayEnabled(bool enabled, EmuTime::param time);
	void setDisplayMode(V9990DisplayMode mode, EmuTime::param time);
	void setColorMode(V9990ColorMode mode, EmuTime::param time);
	void updatePalette(int index, byte r, byte g, byte b, bool ys,
	                   EmuTime::param time);
	void updateSuperimposing(bool enabled, EmuTime::param time);
	void updateBackgroundColor(int index, EmuTime::param time);
	void updateScrollAX(EmuTime::param time);
	void updateScrollBX(EmuTime::param time);
	void updateScrollAYLow(EmuTime::param time);
	void updateScrollBYLow(EmuTime::param time);

private:
	void sync(EmuTime::param time, bool force = false);
	void renderUntil(EmuTime::param time);

	/** Type of drawing to do.
	  */
	enum DrawType {
		DRAW_BORDER,
		DRAW_DISPLAY
	};

	/** The V9990 VDP
	  */
	V9990& vdp;

	EventDistributor& eventDistributor;
	RealTime& realTime;

	/** Settings shared between all renderers
	  */
	RenderSettings& renderSettings;
	VideoSourceSetting& videoSourceSetting;

	/** The Rasterizer
	  */
	const std::unique_ptr<V9990Rasterizer> rasterizer;

	/** Frameskip
	  */
	double finishFrameDuration;
	int frameSkipCounter;

	/** Accuracy setting for current frame.
	 */
	RenderSettings::Accuracy accuracy;

	/** The last sync point's vertical position. In lines, starting
	  * from VSYNC
	  */
	int lastY;

	/** The last sync point's horizontal position in UC ticks, starting
	  * from HSYNC
	  */
	int lastX;

	/** Apparently V9990 keeps an internal counter that indicates which line
	  * to display. That counter is initialized at the top of the screen and
	  * increased after every line. The counter is also reset when the
	  * vertical scroll register is written to (even when same value is
	  * written). Note: this is different from V99x8 behaviour!
	  * TODO we don't actually store the counter but an offset (so that the
	  *      V99x8 drawing algorithm can still be used. Code might become
	  *      simpler if we do store the counter.
	  */
	int verticalOffsetA;
	int verticalOffsetB;

	/** Is display enabled?
	  * Enabled means the current line is in the display area and
	  * forced blanking is off.
	  */
	bool displayEnabled;

	/** Should current frame be draw or can it be skipped.
	  */
	bool drawFrame;
	bool prevDrawFrame;

	/**
	  */
	void draw(int fromX, int fromY, int toX, int toY, DrawType type);

	/** Subdivide an area specified by two scan positions into a series of
	  * rectangles
	  */
	void subdivide(int fromX, int fromY, int toX, int toY,
	               int clipL, int clipR, DrawType drawType);

	// Observer<Setting>
	virtual void update(const Setting& setting);
};

} // namespace openmsx

#endif
