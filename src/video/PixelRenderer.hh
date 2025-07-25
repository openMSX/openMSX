#ifndef PIXELRENDERER_HH
#define PIXELRENDERER_HH

#include "Renderer.hh"
#include "Observer.hh"
#include "RenderSettings.hh"

#include <cstdint>
#include <memory>

namespace openmsx {

class EventDistributor;
class RealTime;
class SpeedManager;
class ThrottleManager;
class Display;
class Rasterizer;
class VDP;
class VDPVRAM;
class SpriteChecker;
class DisplayMode;
class Setting;
class VideoSourceSetting;

/** Generic implementation of a pixel-based Renderer.
  * Uses a Rasterizer to plot actual pixels for a specific video system.
  */
class PixelRenderer final : public Renderer, private Observer<Setting>
{
public:
	PixelRenderer(VDP& vdp, Display& display);
	~PixelRenderer() override;

	// Renderer interface:
	[[nodiscard]] PostProcessor* getPostProcessor() const override;
	void reInit() override;
	void frameStart(EmuTime time) override;
	void frameEnd(EmuTime time) override;
	void updateHorizontalScrollLow(uint8_t scroll, EmuTime time) override;
	void updateHorizontalScrollHigh(uint8_t scroll, EmuTime time) override;
	void updateBorderMask(bool masked, EmuTime time) override;
	void updateMultiPage(bool multiPage, EmuTime time) override;
	void updateTransparency(bool enabled, EmuTime time) override;
	void updateSuperimposing(const RawFrame* videoSource, EmuTime time) override;
	void updateForegroundColor(uint8_t color, EmuTime time) override;
	void updateBackgroundColor(uint8_t color, EmuTime time) override;
	void updateBlinkForegroundColor(uint8_t color, EmuTime time) override;
	void updateBlinkBackgroundColor(uint8_t color, EmuTime time) override;
	void updateBlinkState(bool enabled, EmuTime time) override;
	void updatePalette(unsigned index, int grb, EmuTime time) override;
	void updateVerticalScroll(int scroll, EmuTime time) override;
	void updateHorizontalAdjust(int adjust, EmuTime time) override;
	void updateDisplayEnabled(bool enabled, EmuTime time) override;
	void updateDisplayMode(DisplayMode mode, EmuTime time) override;
	void updateNameBase(unsigned addr, EmuTime time) override;
	void updatePatternBase(unsigned addr, EmuTime time) override;
	void updateColorBase(unsigned addr, EmuTime time) override;
	void updateSpritesEnabled(bool enabled, EmuTime time) override;
	void updateVRAM(unsigned offset, EmuTime time) override;
	void updateWindow(bool enabled, EmuTime time) override;

private:
	/** Indicates whether the area to be drawn is border or display. */
	enum DrawType : uint8_t { DRAW_BORDER, DRAW_DISPLAY };

	// Observer<Setting> interface:
	void update(const Setting& setting) noexcept override;

	/** Call the right draw method in the subclass,
	  * depending on passed drawType.
	  */
	void draw(
		int startX, int startY, int endX, int endY, DrawType drawType,
		bool atEnd);

	/** Subdivide an area specified by two scan positions into a series of
	  * rectangles.
	  * Clips the rectangles to { (x,y) | clipL <= x < clipR }.
	  * @param drawType
	  *   If DRAW_BORDER, draw rectangles using drawBorder;
	  *   if DRAW_DISPLAY, draw rectangles using drawDisplay.
	  */
	void subdivide(
		int startX, int startY, int endX, int endY,
		int clipL, int clipR, DrawType drawType);

	[[nodiscard]] bool checkSync(unsigned offset, EmuTime time) const;

	/** Update renderer state to specified moment in time.
	  * @param time Moment in emulated time to update to.
	  * @param force When screen accuracy is used,
	  *     rendering is only performed if this parameter is true.
	  */
	void sync(EmuTime time, bool force = false);

	/** Render lines until specified moment in time.
	  * Unlike sync(), this method does not sync with VDPVRAM.
	  * The VRAM should be to be up to date and remain unchanged
	  * from the current time to the specified time.
	  * @param time Moment in emulated time to render lines until.
	  */
	void renderUntil(EmuTime time);

private:
	/** The VDP of which the video output is being rendered.
	  */
	VDP& vdp;

	/** The VRAM whose contents are rendered.
	  */
	VDPVRAM& vram;

	EventDistributor& eventDistributor;
	RealTime& realTime;
	SpeedManager& speedManager;
	ThrottleManager& throttleManager;
	RenderSettings& renderSettings;
	VideoSourceSetting& videoSourceSetting;

	/** The sprite checker whose sprites are rendered.
	  */
	SpriteChecker& spriteChecker;

	const std::unique_ptr<Rasterizer> rasterizer;

	float finishFrameDuration = 0.0f;
	float frameSkipCounter = 999.0f; // force drawing of frame

	/** Number of the next position within a line to render.
	  * Expressed in VDP clock ticks since start of line.
	  */
	int nextX;

	/** Number of the next line to render.
	  * Expressed in number of lines since start of frame.
	  */
	int nextY;

	// internal VDP counter, actually belongs in VDP
	int textModeCounter;

	/** Accuracy setting for current frame.
	  */
	RenderSettings::Accuracy accuracy;

	/** Is display enabled?
	  * Enabled means the current line is in the display area and
	  * forced blanking is off.
	  */
	bool displayEnabled;

	/** Should current frame be draw or can it be skipped.
	  */
	bool renderFrame;
	bool prevRenderFrame = false;

	/** Should a rendered frame be painted to the window?
	  * When renderFrame is false, paintFrame must be false as well.
	  * But when recording, renderFrame will be true for every frame,
	  * while paintFrame may not.
	  */
	bool paintFrame;

	/** Timestamp (us, like Timer::getTime()) at which we last painted a frame.
	  * Used to force a minimal paint rate when throttle is off.
	  */
	uint64_t lastPaintTime = 0;
};

} // namespace openmsx

#endif
