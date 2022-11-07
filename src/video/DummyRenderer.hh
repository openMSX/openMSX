#ifndef DUMMYRENDERER_HH
#define DUMMYRENDERER_HH

#include "Renderer.hh"
#include "Layer.hh"

namespace openmsx {

/** Dummy Renderer
  */
class DummyRenderer final : public Renderer, public Layer
{
public:
	// Renderer interface:
	[[nodiscard]] PostProcessor* getPostProcessor() const override;
	void reInit() override;
	void frameStart(EmuTime::param time) override;
	void frameEnd(EmuTime::param time) override;
	void updateTransparency(bool enabled, EmuTime::param time) override;
	void updateSuperimposing(const RawFrame* videoSource, EmuTime::param time) override;
	void updateForegroundColor(int color, EmuTime::param time) override;
	void updateBackgroundColor(int color, EmuTime::param time) override;
	void updateBlinkForegroundColor(int color, EmuTime::param time) override;
	void updateBlinkBackgroundColor(int color, EmuTime::param time) override;
	void updateBlinkState(bool enabled, EmuTime::param time) override;
	void updatePalette(int index, int grb, EmuTime::param time) override;
	void updateVerticalScroll(int scroll, EmuTime::param time) override;
	void updateHorizontalScrollLow(byte scroll, EmuTime::param time) override;
	void updateHorizontalScrollHigh(byte scroll, EmuTime::param time) override;
	void updateBorderMask(bool masked, EmuTime::param time) override;
	void updateMultiPage(bool multiPage, EmuTime::param time) override;
	void updateHorizontalAdjust(int adjust, EmuTime::param time) override;
	void updateDisplayEnabled(bool enabled, EmuTime::param time) override;
	void updateDisplayMode(DisplayMode mode, EmuTime::param time) override;
	void updateNameBase(unsigned addr, EmuTime::param time) override;
	void updatePatternBase(unsigned addr, EmuTime::param time) override;
	void updateColorBase(unsigned addr, EmuTime::param time) override;
	void updateSpritesEnabled(bool enabled, EmuTime::param time) override;
	void updateVRAM(unsigned offset, EmuTime::param time) override;
	void updateWindow(bool enabled, EmuTime::param time) override;

	// Layer interface:
	void paint(OutputSurface& output) override;
};

} // namespace openmsx

#endif
