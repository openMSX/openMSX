#ifndef SMSVDPSDLRASTERIZER_HH
#define SMSVDPSDLRASTERIZER_HH

#include "SMSVDPRasterizer.hh"

#include <memory>

namespace openmsx {

class PostProcessor;
class RawFrame;

/** Rasterizer for the SDLGL-PP video system.
  * Owns the PostProcessor (the actual visible layer) and a work frame.
  */
class SMSVDPSDLRasterizer final : public SMSVDPRasterizer
{
public:
	SMSVDPSDLRasterizer(std::unique_ptr<PostProcessor> postProcessor);
	~SMSVDPSDLRasterizer() override;

	// SMSVDPRasterizer interface:
	[[nodiscard]] PostProcessor* getPostProcessor() const override;
	void reset() override;
	void frameStart() override;
	void frameEnd(EmuTime time) override;
	void drawLine(unsigned y, std::span<const uint32_t, 320> pixels) override;

private:
	std::unique_ptr<RawFrame> workFrame;
	std::unique_ptr<PostProcessor> postProcessor;
};

} // namespace openmsx

#endif
