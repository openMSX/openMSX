#ifndef SDLGLOUTPUTSURFACE_HH
#define SDLGLOUTPUTSURFACE_HH

#include <string>

namespace openmsx {

class OutputSurface;

/** This is a common base class for SDLGLVisibleSurface and
  * SDLGLOffScreenSurface. Its only purpose is to have a place to put common
  * code.
  */
class SDLGLOutputSurface
{
protected:
	SDLGLOutputSurface() = default;
	~SDLGLOutputSurface() = default;

	void init(OutputSurface& output);
	void saveScreenshot(const std::string& filename,
	                    const OutputSurface& output) const;
};

} // namespace openmsx

#endif
