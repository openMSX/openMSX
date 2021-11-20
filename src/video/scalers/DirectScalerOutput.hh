#ifndef DIRECTSCALEROUTPUT_HH
#define DIRECTSCALEROUTPUT_HH

#include "ScalerOutput.hh"
#include "SDLOutputSurface.hh"

namespace openmsx {

template<typename Pixel>
class DirectScalerOutput final : public ScalerOutput<Pixel>
{
public:
	explicit DirectScalerOutput(SDLOutputSurface& output);

	[[nodiscard]] unsigned getWidth()  const override;
	[[nodiscard]] unsigned getHeight() const override;
	[[nodiscard]] Pixel* acquireLine(unsigned y) override;
	void releaseLine(unsigned y, Pixel* buf) override;
	void fillLine   (unsigned y, Pixel color) override;

private:
	SDLOutputSurface& output;
	SDLDirectPixelAccess pixelAccess;
};

} // namespace openmsx

#endif
