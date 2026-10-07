#ifndef SMSVDPDUMMYRENDERER_HH
#define SMSVDPDUMMYRENDERER_HH

#include "SMSVDPRenderer.hh"

namespace openmsx {

class SMSVDPDummyRenderer final : public SMSVDPRenderer
{
public:
	// SMSVDPRenderer interface:
	[[nodiscard]] PostProcessor* getPostProcessor() const override;
	[[nodiscard]] bool isActive() const override;
	void reset(EmuTime time) override;
	void frameStart(EmuTime time) override;
	void frameEnd(EmuTime time) override;
	void drawLine(unsigned y, std::span<const uint32_t, 320> pixels) override;
};

} // namespace openmsx

#endif
