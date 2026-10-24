#ifndef SMSVDPSDLRASTERIZER_HH
#define SMSVDPSDLRASTERIZER_HH

#include "SMSVDPRasterizer.hh"

#include "Observer.hh"

#include <array>
#include <cstdint>
#include <memory>

namespace openmsx {

class Display;
class PostProcessor;
class RawFrame;
class RenderSettings;
class SMSVDP;
class Setting;

/** Rasterizer for the SDLGL-PP video system.
  * Owns the PostProcessor (the actual visible layer) and a work frame.
  * The chip's palette is converted to native pixels (with the video
  * settings applied) and recalculated when those settings change.
  */
class SMSVDPSDLRasterizer final : public SMSVDPRasterizer
                               , private Observer<Setting>
{
public:
	using Pixel = uint32_t;

	SMSVDPSDLRasterizer(SMSVDP& vdp, Display& display, std::unique_ptr<PostProcessor> postProcessor);
	~SMSVDPSDLRasterizer() override;

	// SMSVDPRasterizer interface:
	[[nodiscard]] PostProcessor* getPostProcessor() const override;
	void frameEnd(EmuTime time) override;
	void drawLine(unsigned y, std::span<const uint8_t, 320> paletteIndices) override;

private:
	// Observer<Setting>
	void update(const Setting& setting) noexcept override;

	void precalcPalette();

	SMSVDP& vdp;
	RenderSettings& renderSettings;
	std::array<Pixel, 80> palette{};
	std::unique_ptr<RawFrame> workFrame;
	std::unique_ptr<PostProcessor> postProcessor;
};

} // namespace openmsx

#endif
