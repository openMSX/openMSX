#ifndef SDLGLOFFSCREENSURFACE_HH
#define SDLGLOFFSCREENSURFACE_HH

#include "OutputSurface.hh"
#include "GLUtil.hh"

namespace openmsx {

/** This class installs a FrameBufferObject (FBO). So as long as this object
  * is live, all openGL draw commands will be redirected to this FBO.
  */
class SDLGLOffScreenSurface final : public OutputSurface
{
public:
	explicit SDLGLOffScreenSurface(const OutputSurface& output);

private:
	// OutputSurface
	void saveScreenshot(const std::string& filename) override;

private:
	gl::Texture fboTex;
	gl::FrameBufferObject fbo;
};

} // namespace openmsx

#endif
