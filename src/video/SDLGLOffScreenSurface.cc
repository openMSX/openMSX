#include "SDLGLOffScreenSurface.hh"
#include "SDLGLVisibleSurface.hh"
#include "GLUtil.hh"

namespace openmsx {

SDLGLOffScreenSurface::SDLGLOffScreenSurface(const SDLGLVisibleSurface& output)
	: SDLGLOutputSurface(output.getFrameBufferType())
	, fboTex(true) // enable interpolation   TODO why?
{
	// only used for width and height
	setSDLSurface(const_cast<SDL_Surface*>(output.getSDLSurface()));

	fboTex.bind();
	glTexImage2D(GL_TEXTURE_2D,    // target
	             0,                // level
	             GL_RGB8,          // internal format
	             getWidth(),       // width
	             getHeight(),      // height
	             0,                // border
	             GL_RGB,           // format
	             GL_UNSIGNED_BYTE, // type
	             nullptr);         // data
	fbo = gl::FrameBufferObject(fboTex);
	fbo.push();

	SDLGLOutputSurface::init(*this);
}

SDLGLOffScreenSurface::~SDLGLOffScreenSurface()
{
}

void SDLGLOffScreenSurface::flushFrameBuffer()
{
	SDLGLOutputSurface::flushFrameBuffer(getWidth(), getHeight());
}

void SDLGLOffScreenSurface::clearScreen()
{
	SDLGLOutputSurface::clearScreen();
}

void SDLGLOffScreenSurface::saveScreenshot(const std::string& filename)
{
	SDLGLOutputSurface::saveScreenshot(filename, getWidth(), getHeight());
}

} // namespace openmsx
