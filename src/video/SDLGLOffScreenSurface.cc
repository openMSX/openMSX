#include "SDLGLOffScreenSurface.hh"
#include "SDLGLVisibleSurface.hh"
#include "GLUtil.hh"

namespace openmsx {

SDLGLOffScreenSurface::SDLGLOffScreenSurface(const SDLGLVisibleSurface& output)
	: fboTex(true) // enable interpolation   TODO why?
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

	SDLAllocFormatPtr frmt(SDL_AllocFormat(
		        OPENMSX_BIGENDIAN ? SDL_PIXELFORMAT_RGBA8888 :
		                            SDL_PIXELFORMAT_ARGB8888));
	setSDLFormat(*frmt);
	setBufferPtr(nullptr, 0); // direct access not allowed
}

void SDLGLOffScreenSurface::saveScreenshot(const std::string& filename)
{
	SDLGLVisibleSurface::saveScreenshotGL(*this, filename);
}

} // namespace openmsx
