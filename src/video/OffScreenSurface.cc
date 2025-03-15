#include "OffScreenSurface.hh"
#include "VisibleSurface.hh"
#include "GLUtil.hh"

namespace openmsx {

OffScreenSurface::OffScreenSurface(const OutputSurface& output)
	: fboTex(true) // enable interpolation   TODO why?
{
	prevFbo = gl::FrameBufferObject::getCurrent();

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

	fbo.activate();
}

OffScreenSurface::~OffScreenSurface()
{
	gl::FrameBufferObject::activate(prevFbo);
}

void OffScreenSurface::saveScreenshot(const std::string& filename)
{
	VisibleSurface::saveScreenshotGL(*this, filename);
}

} // namespace openmsx
