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
	DummyRenderer() : Layer(Layer::Coverage::NONE, Layer::ZIndex::BACKGROUND) {}

	// Renderer interface:
	[[nodiscard]] PostProcessor* getPostProcessor() const override;
	void reInit() override;
	void frameStart(EmuTime time) override;
	void frameEnd(EmuTime time) override;
	void updateTransparency(bool enabled, EmuTime time) override;
	void updateSuperimposing(const RawFrame* videoSource, EmuTime time) override;
	void updateForegroundColor(uint8_t color, EmuTime time) override;
	void updateBackgroundColor(uint8_t color, EmuTime time) override;
	void updateBlinkForegroundColor(uint8_t color, EmuTime time) override;
	void updateBlinkBackgroundColor(uint8_t color, EmuTime time) override;
	void updateBlinkState(bool enabled, EmuTime time) override;
	void updatePalette(unsigned index, int grb, EmuTime time) override;
	void updateVerticalScroll(int scroll, EmuTime time) override;
	void updateHorizontalScrollLow(uint8_t scroll, EmuTime time) override;
	void updateHorizontalScrollHigh(uint8_t scroll, EmuTime time) override;
	void updateBorderMask(bool masked, EmuTime time) override;
	void updateMultiPage(bool multiPage, EmuTime time) override;
	void updateHorizontalAdjust(int adjust, EmuTime time) override;
	void updateDisplayEnabled(bool enabled, EmuTime time) override;
	void updateDisplayMode(DisplayMode mode, EmuTime time) override;
	void updateNameBase(unsigned addr, EmuTime time) override;
	void updatePatternBase(unsigned addr, EmuTime time) override;
	void updateColorBase(unsigned addr, EmuTime time) override;
	void updateSpritesEnabled(bool enabled, EmuTime time) override;
	void updateVRAM(unsigned offset, EmuTime time) override;
	void updateWindow(bool enabled, EmuTime time) override;

	// Layer interface:
	void paint(OutputSurface& output) override;
};

} // namespace openmsx

#endif
