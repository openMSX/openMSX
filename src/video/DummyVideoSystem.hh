#ifndef DUMMYVIDEOSYSTEM_HH
#define DUMMYVIDEOSYSTEM_HH

#include "VideoSystem.hh"
#include "components.hh"

namespace openmsx {

class DummyVideoSystem final : public VideoSystem
{
public:
	// VideoSystem interface:
	std::unique_ptr<Rasterizer> createRasterizer(VDP& vdp) override;
	std::unique_ptr<V9990Rasterizer> createV9990Rasterizer(
		V9990& vdp) override;
#if COMPONENT_LASERDISC
	std::unique_ptr<LDRasterizer> createLDRasterizer(
		LaserdiscPlayer& ld) override;
#endif
	void flush() override;
	OutputSurface* getOutputSurface() override;
	void showCursor(bool show) override;
	void repaint() override;
};

} // namespace openmsx

#endif
