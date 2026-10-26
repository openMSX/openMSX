#ifndef SMSVDPPIXELRENDERER_HH
#define SMSVDPPIXELRENDERER_HH

#include "SMSVDPRenderer.hh"

#include "Observer.hh"

#include <cstdint>
#include <memory>

namespace openmsx {

class Display;
class EventDistributor;
class MSXMotherBoard;
class RealTime;
class RenderSettings;
class Setting;
class SMSVDP;
class SMSVDPRasterizer;
class SpeedManager;
class ThrottleManager;
class VideoSourceSetting;

/** Generic pixel based renderer for the Sega VDP core.
  * Uses a rasterizer to plot actual pixels for a specific video system.
  */
class SMSVDPPixelRenderer final : public SMSVDPRenderer
                               , private Observer<Setting>
{
public:
	explicit SMSVDPPixelRenderer(SMSVDP& vdp);
	~SMSVDPPixelRenderer() override;

	// SMSVDPRenderer interface:
	[[nodiscard]] PostProcessor* getPostProcessor() const override;
	[[nodiscard]] bool isActive() const override;
	void frameStart(EmuTime time) override;
	void frameEnd(EmuTime time) override;
	void reInit() override;
	void drawLine(unsigned y, std::span<const uint8_t, 320> paletteIndices) override;

private:
	// Observer<Setting> interface:
	void update(const Setting& setting) noexcept override;

	MSXMotherBoard& motherboard;
	EventDistributor& eventDistributor;
	VideoSourceSetting& videoSourceSetting;
	RealTime& realTime;
	SpeedManager& speedManager;
	ThrottleManager& throttleManager;
	RenderSettings& renderSettings;
	Display& display;
	const std::unique_ptr<SMSVDPRasterizer> rasterizer;

	float finishFrameDuration = 0.0f;
	float frameSkipCounter = 999.0f; // force drawing of the first frame

	/** Whether the current frame is drawn (see frameStart()). When false,
	  * the VDP core skips the scanline rendering work.
	  */
	bool renderFrame = false;

	/** Whether the current frame is painted (see frameStart()). When
	  * renderFrame is false, paintFrame must be false as well. But when
	  * recording, renderFrame will be true for every frame, while paintFrame
	  * may not.
	  */
	bool paintFrame = false;

	/** Time of the last painted frame, used when the emulation is not
	  * throttled (see frameStart()).
	  */
	uint64_t lastPaintTime = 0;
};

} // namespace openmsx

#endif
