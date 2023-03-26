#include "SDLGLOffScreenSurface.hh"
#include "VisibleSurface.hh"
#include "GLUtil.hh"

namespace openmsx {

SDLGLOffScreenSurface::SDLGLOffScreenSurface(const OutputSurface& output)
	: fboTex(true) // enable interpolation   TODO why?
{
	calculateViewPort(output.getLogicalSize(), output.getPhysicalSize());
	auto [w, h] = getPhysicalSize();
	fboTex.bind();
	glTexImage2D(GL_TEXTURE_2D,    // target
	             0,                // level
	             GL_RGB,           // internal format
	             w,                // width
	             h,                // height
	             0,                // border
	             GL_RGB,           // format
	             GL_UNSIGNED_BYTE, // type
	             nullptr);         // data
	fbo = gl::FrameBufferObject(fboTex);
	fbo.push();
}

void SDLGLOffScreenSurface::saveScreenshot(const std::string& filename)
{
	VisibleSurface::saveScreenshotGL(*this, filename);
}

} // namespace openmsx
