#ifndef V9990DUMMYRENDERER_HH
#define V9990DUMMYRENDERER_HH

#include "V9990Renderer.hh"

namespace openmsx {

class V9990DummyRenderer final : public V9990Renderer
{
public:
	// V9990Renderer interface:
	[[nodiscard]] PostProcessor* getPostProcessor() const override;
	void reset(EmuTime::param time) override;
	void frameStart(EmuTime::param time) override;
	void frameEnd(EmuTime::param time) override;
	void renderUntil(EmuTime::param time) override;
	void updateDisplayEnabled(bool enabled, EmuTime::param time) override;
	void setDisplayMode(V9990DisplayMode mode, EmuTime::param time) override;
	void setColorMode(V9990ColorMode mode, EmuTime::param time) override;
	void updatePalette(int index, byte r, byte g, byte b, bool ys,
	                   EmuTime::param time) override;
	void updateSuperimposing(bool enabled, EmuTime::param time) override;
	void updateBackgroundColor(int index, EmuTime::param time) override;
	void updateScrollAX(EmuTime::param time) override;
	void updateScrollBX(EmuTime::param time) override;
	void updateScrollAYLow(EmuTime::param time) override;
	void updateScrollBYLow(EmuTime::param time) override;
};

} // namespace openmsx

#endif
