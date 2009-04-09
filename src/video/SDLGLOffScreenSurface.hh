// $Id$

#ifndef SDLGLOFFSCREENSURFACE_HH
#define SDLGLOFFSCREENSURFACE_HH

#include "OutputSurface.hh"
#include "SDLGLOutputSurface.hh"
#include <memory>

namespace openmsx {

class SDLGLVisibleSurface;
class Texture;
class FrameBufferObject;

/** This class installs a FrameBufferObject (FBO). So as long as this object
  * is live, all openGL draw commands will be redirected to this FBO.
  */
class SDLGLOffScreenSurface : public OutputSurface, private SDLGLOutputSurface
{
public:
	SDLGLOffScreenSurface(const SDLGLVisibleSurface& output);
	virtual ~SDLGLOffScreenSurface();

private:
	// OutputSurface
	virtual void saveScreenshot(const std::string& filename);
	virtual void flushFrameBuffer();

	std::auto_ptr<Texture> fboTex;
	std::auto_ptr<FrameBufferObject> fbo;
};

} // namespace openmsx

#endif
