#ifndef V9990PIXELRENDERER_HH
#define V9990PIXELRENDERER_HH

#include "V9990Renderer.hh"

#include "Observer.hh"
#include "RenderSettings.hh"

#include <cstdint>
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
class V9990PixelRenderer final : public V9990Renderer
                               , private Observer<Setting>
{
public:
	explicit V9990PixelRenderer(V9990& vdp);
	~V9990PixelRenderer() override;

	// V9990Renderer interface:
	[[nodiscard]] PostProcessor* getPostProcessor() const override;
	void reset(EmuTime time) override;
	void frameStart(EmuTime time) override;
	void frameEnd(EmuTime time) override;
	void updateDisplayEnabled(bool enabled, EmuTime time) override;
	void setDisplayMode(V9990DisplayMode mode, EmuTime time) override;
	void setColorMode(V9990ColorMode mode, EmuTime time) override;
	void updatePalette(int index, uint8_t r, uint8_t g, uint8_t b, bool ys,
	                   EmuTime time) override;
	void updateSuperimposing(bool enabled, EmuTime time) override;
	void updateBackgroundColor(int index, EmuTime time) override;
	void updateScrollAX(EmuTime time) override;
	void updateScrollBX(EmuTime time) override;
	void updateScrollAYLow(EmuTime time) override;
	void updateScrollBYLow(EmuTime time) override;

private:
	void sync(EmuTime time, bool force = false);
	void renderUntil(EmuTime time) override;

	/** Type of drawing to do.
	  */
	enum DrawType : uint8_t {
		DRAW_BORDER,
		DRAW_DISPLAY
	};

	/**
	  */
	void draw(int fromX, int fromY, int toX, int toY, DrawType type);

	/** Subdivide an area specified by two scan positions into a series of
	  * rectangles
	  */
	void subdivide(int fromX, int fromY, int toX, int toY,
	               int clipL, int clipR, DrawType drawType);

	// Observer<Setting>
	void update(const Setting& setting) noexcept override;

private:
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
	float finishFrameDuration{0.0f};
	int frameSkipCounter{999}; // force drawing of frame;

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
	bool drawFrame{false}; // don't draw before frameStart is called
	bool prevDrawFrame{false};
};

} // namespace openmsx

#endif
