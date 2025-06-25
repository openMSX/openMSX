#ifndef LDPIXELRENDERER_HH
#define LDPIXELRENDERER_HH

#include "LDRenderer.hh"
#include <memory>

namespace openmsx {

class MSXMotherBoard;
class EventDistributor;
class Display;
class LDRasterizer;
class LaserdiscPlayer;

/** Generic implementation of a pixel-based Renderer.
  * Uses a Rasterizer to plot actual pixels for a specific video system.
  */
class LDPixelRenderer final : public LDRenderer
{
public:
	LDPixelRenderer(LaserdiscPlayer& ld, Display& display);
	~LDPixelRenderer() override;

	// Renderer interface:
	void frameStart(EmuTime time) override;
	void frameEnd() override;
	void drawBlank(int r, int g, int b) override;
	[[nodiscard]] RawFrame* getRawFrame() override;

private:
	[[nodiscard]] bool isActive() const;

private:
	MSXMotherBoard& motherboard;
	EventDistributor& eventDistributor;
	const std::unique_ptr<LDRasterizer> rasterizer;
};

} // namespace openmsx

#endif
