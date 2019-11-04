#include "SDLGLOffScreenSurface.hh"
#include "SDLGLVisibleSurface.hh"
#include "GLUtil.hh"

namespace openmsx {

SDLGLOffScreenSurface::SDLGLOffScreenSurface(const SDLGLVisibleSurface& output)
	: SDLOutputSurface(32,
		OPENMSX_BIGENDIAN ? 0xFF000000 : 0x00FF0000, OPENMSX_BIGENDIAN ? 24 : 16, 0,
		OPENMSX_BIGENDIAN ? 0x00FF0000 : 0x0000FF00, OPENMSX_BIGENDIAN ? 16 :  8, 0,
		OPENMSX_BIGENDIAN ? 0x0000FF00 : 0x000000FF, OPENMSX_BIGENDIAN ?  8 :  0, 0,
		OPENMSX_BIGENDIAN ? 0x000000FF : 0xFF000000, OPENMSX_BIGENDIAN ?  0 : 24, 0)
	, fboTex(true) // enable interpolation   TODO why?
{
	// only used for width and height
	setSDLSurface(const_cast<SDL_Surface*>(output.getSDLSurface()));
	calculateViewPort(output.getPhysicalSize());

	auto [w, h] = getPhysicalSize();
	fboTex.bind();
	glTexImage2D(GL_TEXTURE_2D,    // target
	             0,                // level
	             GL_RGB8,          // internal format
	             w,                // width
	             h,                // height
	             0,                // border
	             GL_RGB,           // format
	             GL_UNSIGNED_BYTE, // type
	             nullptr);         // data
	fbo = gl::FrameBufferObject(fboTex);
	fbo.push();

	setBufferPtr(nullptr, 0); // direct access not allowed
}

void SDLGLOffScreenSurface::saveScreenshot(const std::string& filename)
{
	SDLGLVisibleSurface::saveScreenshotGL(*this, filename);
}

} // namespace openmsx
