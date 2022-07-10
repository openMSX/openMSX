#ifndef SCALERFACTORY_HH
#define SCALERFACTORY_HH

#include <concepts>
#include <memory>

namespace openmsx {

class RenderSettings;
template<std::unsigned_integral Pixel> class Scaler;
template<std::unsigned_integral Pixel> class PixelOperations;

/** Abstract base class for scalers.
  * A scaler is an algorithm that converts low-res graphics to hi-res graphics.
  */
template<std::unsigned_integral Pixel>
class ScalerFactory
{
public:
	/** Instantiates a Scaler.
	  * @return A Scaler object, owned by the caller.
	  */
	[[nodiscard]] static std::unique_ptr<Scaler<Pixel>> createScaler(
		const PixelOperations<Pixel>& pixelOps,
		RenderSettings& renderSettings);
};

} // namespace openmsx

#endif
