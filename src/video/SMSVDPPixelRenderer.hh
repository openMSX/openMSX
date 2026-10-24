#ifndef SMSVDPPIXELRENDERER_HH
#define SMSVDPPIXELRENDERER_HH

#include "SMSVDPRenderer.hh"

#include <memory>

namespace openmsx {

class EventDistributor;
class MSXMotherBoard;
class SMSVDP;
class SMSVDPRasterizer;
class VideoSourceSetting;

/** Generic pixel based renderer for the Sega VDP core.
  * Uses a rasterizer to plot actual pixels for a specific video system.
  */
class SMSVDPPixelRenderer final : public SMSVDPRenderer
{
public:
	explicit SMSVDPPixelRenderer(SMSVDP& vdp);
	~SMSVDPPixelRenderer() override;

	// SMSVDPRenderer interface:
	[[nodiscard]] PostProcessor* getPostProcessor() const override;
	[[nodiscard]] bool isActive() const override;
	void frameEnd(EmuTime time) override;
	void drawLine(unsigned y, std::span<const uint8_t, 320> paletteIndices) override;

private:
	MSXMotherBoard& motherboard;
	EventDistributor& eventDistributor;
	VideoSourceSetting& videoSourceSetting;
	const std::unique_ptr<SMSVDPRasterizer> rasterizer;
};

} // namespace openmsx

#endif
