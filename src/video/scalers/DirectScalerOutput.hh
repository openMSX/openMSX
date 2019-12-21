#ifndef DIRECTSCALEROUTPUT_HH
#define DIRECTSCALEROUTPUT_HH

#include "ScalerOutput.hh"

namespace openmsx {

class SDLOutputSurface;

template<typename Pixel>
class DirectScalerOutput final : public ScalerOutput<Pixel>
{
public:
	explicit DirectScalerOutput(SDLOutputSurface& output);

	unsigned getWidth()  const override;
	unsigned getHeight() const override;
	Pixel* acquireLine(unsigned y) override;
	void   releaseLine(unsigned y, Pixel* buf) override;
	void   fillLine   (unsigned y, Pixel color) override;

private:
	SDLOutputSurface& output;
};

} // namespace openmsx

#endif
