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
	~LDPixelRenderer();

	// Renderer interface:
	void frameStart(EmuTime::param time) override;
	void frameEnd() override;
	void drawBlank(int r, int g, int b) override;
	RawFrame* getRawFrame() override;

private:
	bool isActive() const;

	MSXMotherBoard& motherboard;
	EventDistributor& eventDistributor;
	const std::unique_ptr<LDRasterizer> rasterizer;
};

} // namespace openmsx

#endif
