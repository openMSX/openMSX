#ifndef SDLGLOFFSCREENSURFACE_HH
#define SDLGLOFFSCREENSURFACE_HH

#include "OutputSurface.hh"
#include "SDLGLOutputSurface.hh"
#include "GLUtil.hh"

namespace openmsx {

class SDLGLVisibleSurface;

/** This class installs a FrameBufferObject (FBO). So as long as this object
  * is live, all openGL draw commands will be redirected to this FBO.
  */
class SDLGLOffScreenSurface : public OutputSurface, private SDLGLOutputSurface
{
public:
	explicit SDLGLOffScreenSurface(const SDLGLVisibleSurface& output);
	virtual ~SDLGLOffScreenSurface();

private:
	// OutputSurface
	virtual void saveScreenshot(const std::string& filename);
	virtual void flushFrameBuffer();
	virtual void clearScreen();

	Texture fboTex;
	FrameBufferObject fbo;
};

} // namespace openmsx

#endif
