// $Id$

#include "SDLGLOffScreenSurface.hh"
#include "SDLGLVisibleSurface.hh"
#include "GLUtil.hh"

namespace openmsx {

SDLGLOffScreenSurface::SDLGLOffScreenSurface(const SDLGLVisibleSurface& output)
	: SDLGLOutputSurface(output.getFrameBufferType())
{
	// only used for width and height
	setSDLDisplaySurface(
		const_cast<SDL_Surface*>(output.getSDLDisplaySurface()));

	fboTex.reset(new Texture());
	fboTex->bind();
	fboTex->setWrapMode(false);
	fboTex->enableInterpolation();
	glTexImage2D(GL_TEXTURE_2D,    // target
	             0,                // level
	             GL_RGB8,          // internal format
	             getWidth(),       // width
	             getHeight(),      // height
	             0,                // border
	             GL_RGB,           // format
	             GL_UNSIGNED_BYTE, // type
	             nullptr);         // data
	fbo.reset(new FrameBufferObject(*fboTex));
	fbo->push();

	SDLGLOutputSurface::init(*this);
}

SDLGLOffScreenSurface::~SDLGLOffScreenSurface()
{
}

void SDLGLOffScreenSurface::flushFrameBuffer()
{
	SDLGLOutputSurface::flushFrameBuffer(getWidth(), getHeight());
}

void SDLGLOffScreenSurface::saveScreenshot(const std::string& filename)
{
	SDLGLOutputSurface::saveScreenshot(filename, getWidth(), getHeight());
}

} // namespace openmsx
