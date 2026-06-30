#ifndef OFFSCREENSURFACE_HH
#define OFFSCREENSURFACE_HH

#include "GLUtil.hh"

namespace openmsx {

class OutputDimensions;

/** This class installs a FrameBufferObject (FBO). So as long as this object
  * is live, all openGL draw commands will be redirected to this FBO.
  */
class OffScreenSurface
{
public:
	explicit OffScreenSurface(const OutputDimensions& output);
	~OffScreenSurface();

private:
	gl::Texture fboTex;
	gl::FrameBufferObject fbo;
	GLuint prevFbo = 0;
};

} // namespace openmsx

#endif
