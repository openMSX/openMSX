// $Id$

#ifndef GLSCALERFACTORY_HH
#define GLSCALERFACTORY_HH

#include <memory>

namespace openmsx {

class GLScaler;
class RenderSettings;

class GLScalerFactory
{
public:
	/** Instantiates a Scaler.
	  * @return A Scaler object, owned by the caller.
	  */
	static std::auto_ptr<GLScaler> createScaler(
		RenderSettings& renderSettings
		);
};

} // namespace openmsx

#endif // GLSCALERFACTORY_HH
