// $Id$

#ifndef SCALERFACTORY_HH
#define SCALERFACTORY_HH

#include <memory>

namespace openmsx {

class Scaler;
class RenderSettings;
template <typename Pixel> class PixelOperations;

/** Abstract base class for scalers.
  * A scaler is an algorithm that converts low-res graphics to hi-res graphics.
  */
template <class Pixel>
class ScalerFactory
{
public:
	/** Instantiates a Scaler.
	  * @param format Pixel format of the surfaces the scaler will be used on.
	  * @return A Scaler object, owned by the caller.
	  */
	static std::auto_ptr<Scaler> createScaler(
		const PixelOperations<Pixel>& pixelOps, RenderSettings& renderSettings
		);
};

} // namespace openmsx

#endif
