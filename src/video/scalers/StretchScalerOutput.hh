#ifndef STRETCHSCALEROUTPUT_HH
#define STRETCHSCALEROUTPUT_HH

#include <concepts>
#include <memory>

namespace openmsx {

class SDLOutputSurface;
template<std::unsigned_integral Pixel> class PixelOperations;
template<std::unsigned_integral Pixel> class ScalerOutput;

template<std::unsigned_integral Pixel>
struct StretchScalerOutputFactory
{
	[[nodiscard]] static std::unique_ptr<ScalerOutput<Pixel>> create(
		SDLOutputSurface& output,
		PixelOperations<Pixel> pixelOps,
		unsigned inWidth);
};

} // namespace openmsx

#endif
