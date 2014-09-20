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
class SDLGLOffScreenSurface final : public OutputSurface, private SDLGLOutputSurface
{
public:
	explicit SDLGLOffScreenSurface(const SDLGLVisibleSurface& output);
	~SDLGLOffScreenSurface();

private:
	// OutputSurface
	void saveScreenshot(const std::string& filename) override;
	void flushFrameBuffer() override;
	void clearScreen() override;

	gl::Texture fboTex;
	gl::FrameBufferObject fbo;
};

} // namespace openmsx

#endif
