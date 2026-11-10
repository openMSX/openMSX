#ifndef V9990DUMMYRENDERER_HH
#define V9990DUMMYRENDERER_HH

#include "V9990Renderer.hh"

namespace openmsx {

class V9990DummyRenderer final : public V9990Renderer
{
public:
	// V9990Renderer interface:
	[[nodiscard]] PostProcessor* getPostProcessor() const override;
	void reset(EmuTime time) override;
	void frameStart(EmuTime time) override;
	void frameEnd(EmuTime time) override;
	void renderUntil(EmuTime time) override;
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
};

} // namespace openmsx

#endif
