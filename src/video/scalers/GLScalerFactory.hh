#ifndef GLSCALERFACTORY_HH
#define GLSCALERFACTORY_HH

#include <memory>

namespace openmsx {

class GLScaler;
class RenderSettings;

namespace GLScalerFactory
{
	/** Instantiates a Scaler.
	  * @return A Scaler object, owned by the caller.
	  */
	[[nodiscard]] std::unique_ptr<GLScaler> createScaler(RenderSettings& renderSettings);
}

} // namespace openmsx

#endif // GLSCALERFACTORY_HH
