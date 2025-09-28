#ifndef OFFSCREENSURFACE_HH
#define OFFSCREENSURFACE_HH

#include "GLUtil.hh"
#include "OutputSurface.hh"

namespace openmsx {

/** This class installs a FrameBufferObject (FBO). So as long as this object
  * is live, all openGL draw commands will be redirected to this FBO.
  */
class OffScreenSurface final : public OutputSurface
{
public:
	explicit OffScreenSurface(const OutputSurface& output);

private:
	// OutputSurface
	void saveScreenshot(const std::string& filename) override;

private:
	gl::Texture fboTex;
	gl::FrameBufferObject fbo;
};

} // namespace openmsx

#endif
