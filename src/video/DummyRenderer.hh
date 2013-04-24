#ifndef DUMMYRENDERER_HH
#define DUMMYRENDERER_HH

#include "Renderer.hh"
#include "Layer.hh"

namespace openmsx {

/** Dummy Renderer
  */
class DummyRenderer : public Renderer, public Layer
{
public:
	// Renderer interface:
	PostProcessor* getPostProcessor() const;
	void reInit();
	void frameStart(EmuTime::param time);
	void frameEnd(EmuTime::param time);
	void updateTransparency(bool enabled, EmuTime::param time);
	void updateSuperimposing(const RawFrame* videoSource, EmuTime::param time);
	void updateForegroundColor(int color, EmuTime::param time);
	void updateBackgroundColor(int color, EmuTime::param time);
	void updateBlinkForegroundColor(int color, EmuTime::param time);
	void updateBlinkBackgroundColor(int color, EmuTime::param time);
	void updateBlinkState(bool enabled, EmuTime::param time);
	void updatePalette(int index, int grb, EmuTime::param time);
	void updateVerticalScroll(int scroll, EmuTime::param time);
	void updateHorizontalScrollLow(byte scroll, EmuTime::param time);
	void updateHorizontalScrollHigh(byte scroll, EmuTime::param time);
	void updateBorderMask(bool masked, EmuTime::param time);
	void updateMultiPage(bool multiPage, EmuTime::param time);
	void updateHorizontalAdjust(int adjust, EmuTime::param time);
	void updateDisplayEnabled(bool enabled, EmuTime::param time);
	void updateDisplayMode(DisplayMode mode, EmuTime::param time);
	void updateNameBase(int addr, EmuTime::param time);
	void updatePatternBase(int addr, EmuTime::param time);
	void updateColorBase(int addr, EmuTime::param time);
	void updateSpritesEnabled(bool enabled, EmuTime::param time);
	void updateVRAM(unsigned offset, EmuTime::param time);
	void updateWindow(bool enabled, EmuTime::param time);

	// Layer interface:
	virtual void paint(OutputSurface& output);
};

} // namespace openmsx

#endif
