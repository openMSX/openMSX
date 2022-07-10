#ifndef SDLSNOW_HH
#define SDLSNOW_HH

#include "Layer.hh"
#include <concepts>

namespace openmsx {

class OutputSurface;
class Display;

/** Snow effect for background layer.
  */
template<std::unsigned_integral Pixel>
class SDLSnow final : public Layer
{
public:
	SDLSnow(OutputSurface& output, Display& display);

	// Layer interface:
	void paint(OutputSurface& output) override;

private:
	Display& display;

	/** Gray values for noise.
	  */
	Pixel gray[256];
};

} // namespace openmsx

#endif
