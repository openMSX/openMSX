#ifndef DUMMYVIDEOSYSTEM_HH
#define DUMMYVIDEOSYSTEM_HH

#include "VideoSystem.hh"
#include "components.hh"

namespace openmsx {

class DummyVideoSystem final : public VideoSystem
{
public:
	// VideoSystem interface:
	[[nodiscard]] std::unique_ptr<Rasterizer> createRasterizer(VDP& vdp) override;
	[[nodiscard]] std::unique_ptr<V9990Rasterizer> createV9990Rasterizer(
		V9990& vdp) override;
#if COMPONENT_LASERDISC
	[[nodiscard]] std::unique_ptr<LDRasterizer> createLDRasterizer(
		LaserdiscPlayer& ld) override;
#endif
	void flush() override;
	[[nodiscard]] std::optional<gl::ivec2> getMouseCoord() override;
	[[nodiscard]] OutputSurface* getOutputSurface() override;
	void showCursor(bool show) override;
	[[nodiscard]] bool getCursorEnabled() override;
	[[nodiscard]] std::string getClipboardText() override;
	void setClipboardText(zstring_view text) override;
	[[nodiscard]] std::optional<gl::ivec2> getWindowPosition() override;
	void setWindowPosition(gl::ivec2 pos) override;
	void repaint() override;
};

} // namespace openmsx

#endif
