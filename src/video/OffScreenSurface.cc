#include "OffScreenSurface.hh"

#include "GLUtil.hh"
#include "OutputDimensions.hh"

namespace openmsx {

OffScreenSurface::OffScreenSurface(const OutputDimensions& output)
	: fboTex(true) // enable interpolation   TODO why?
{
	prevFbo = gl::FrameBufferObject::getCurrent();

	auto [w, h] = output.getPhysicalSize();
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

} // namespace openmsx
