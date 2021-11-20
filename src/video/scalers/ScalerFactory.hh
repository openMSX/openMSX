#ifndef SCALERFACTORY_HH
#define SCALERFACTORY_HH

#include <memory>

namespace openmsx {

class RenderSettings;
template<typename Pixel> class Scaler;
template<typename Pixel> class PixelOperations;

/** Abstract base class for scalers.
  * A scaler is an algorithm that converts low-res graphics to hi-res graphics.
  */
template<typename Pixel>
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
