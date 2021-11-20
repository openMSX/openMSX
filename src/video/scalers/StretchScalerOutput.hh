#ifndef STRETCHSCALEROUTPUT_HH
#define STRETCHSCALEROUTPUT_HH

#include <memory>

namespace openmsx {

class SDLOutputSurface;
template<typename Pixel> class PixelOperations;
template<typename Pixel> class ScalerOutput;

template<typename Pixel>
struct StretchScalerOutputFactory
{
	[[nodiscard]] static std::unique_ptr<ScalerOutput<Pixel>> create(
		SDLOutputSurface& output,
		PixelOperations<Pixel> pixelOps,
		unsigned inWidth);
};

} // namespace openmsx

#endif
