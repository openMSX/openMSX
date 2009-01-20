// $Id$

#include "SDLGLVisibleSurface.hh"
#include "ScreenShotSaver.hh"
#include "GLSnow.hh"
#include "OSDConsoleRenderer.hh"
#include "OSDGUILayer.hh"
#include "InitException.hh"
#include "build-info.hh"
#include "Math.hh"
#include "vla.hh"
#include <vector>

namespace openmsx {

SDLGLVisibleSurface::SDLGLVisibleSurface(
		unsigned width, unsigned height, bool fullscreen,
		FrameBuffer frameBuffer_)
	: buffer(0)
	, frameBuffer(frameBuffer_)
{
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);
	int flags = SDL_OPENGL | SDL_HWSURFACE | SDL_DOUBLEBUF |
	            (fullscreen ? SDL_FULLSCREEN : 0);
	//flags |= SDL_RESIZABLE;
	createSurface(width, height, flags);

	// From the glew documentation:
	//   GLEW obtains information on the supported extensions from the
	//   graphics driver. Experimental or pre-release drivers, however,
	//   might not report every available extension through the standard
	//   mechanism, in which case GLEW will report it unsupported. To
	//   circumvent this situation, the glewExperimental global switch can
	//   be turned on by setting it to GL_TRUE before calling glewInit(),
	//   which ensures that all extensions with valid entry points will be
	//   exposed.
	// The 'glewinfo' utility also sets this flag before reporting results,
	// so I believe it would cause less confusion to do the same here.
	glewExperimental = GL_TRUE;

	// Initialise GLEW library.
	// This must happen after GL itself is initialised, which is done by
	// the SDL_SetVideoMode() call in createSurface().
	GLenum glew_error = glewInit();
	if (glew_error != GLEW_OK) {
		throw InitException(
			"Failed to init GLEW: " + std::string(
				reinterpret_cast<const char*>(glewGetErrorString(glew_error))
			));
	}

	glViewport(0, 0, width, height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, width, height, 0, -1, 1);
	glMatrixMode(GL_MODELVIEW);

	SDL_PixelFormat& format = getSDLFormat();
	format.palette = 0;
	format.colorkey = 0;
	format.alpha = 0;
	if (frameBuffer == FB_16BPP) {
		format.BitsPerPixel = 16;
		format.BytesPerPixel = 2;
		format.Rloss = 3;
		format.Gloss = 2;
		format.Bloss = 3;
		format.Aloss = 8;
		format.Rshift = 11;
		format.Gshift = 5;
		format.Bshift = 0;
		format.Ashift = 0;
		format.Rmask = 0xF800;
		format.Gmask = 0x07E0;
		format.Bmask = 0x001F;
		format.Amask = 0x0000;
	} else {
		// BGRA format
		format.BitsPerPixel = 32;
		format.BytesPerPixel = 4;
		format.Rloss = 0;
		format.Gloss = 0;
		format.Bloss = 0;
		format.Aloss = 0;
		if (OPENMSX_BIGENDIAN) {
			format.Rshift =  8;
			format.Gshift = 16;
			format.Bshift = 24;
			format.Ashift =  0;
			format.Rmask = 0x0000FF00;
			format.Gmask = 0x00FF0000;
			format.Bmask = 0xFF000000;
			format.Amask = 0x000000FF;
		} else {
			format.Rshift = 16;
			format.Gshift =  8;
			format.Bshift =  0;
			format.Ashift = 24;
			format.Rmask = 0x00FF0000;
			format.Gmask = 0x0000FF00;
			format.Bmask = 0x000000FF;
			format.Amask = 0xFF000000;
		}
	}

	if (frameBuffer == FB_NONE) {
		setBufferPtr(0, 0); // direct access not allowed
	} else {
		// TODO 64 byte aligned (see RawFrame)
		unsigned texW = Math::powerOfTwo(width);
		unsigned texH = Math::powerOfTwo(height);
		buffer = new char[format.BytesPerPixel * texW * texH];
		unsigned pitch = width * format.BytesPerPixel;
		setBufferPtr(buffer, pitch);

		texCoordX = double(width)  / texW;
		texCoordY = double(height) / texH;

		texture.reset(new Texture());
		texture->bind();
		if (frameBuffer == FB_16BPP) {
			// TODO: Why use RGB texture instead of RGBA?
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, texW, texH, 0,
			             GL_RGB, GL_UNSIGNED_SHORT_5_6_5, buffer);
		} else {
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, texW, texH, 0,
			             GL_BGRA, GL_UNSIGNED_BYTE, buffer);
		}
	}
}

SDLGLVisibleSurface::~SDLGLVisibleSurface()
{
	delete[] buffer;
}

void SDLGLVisibleSurface::init()
{
	// nothing
}

void SDLGLVisibleSurface::drawFrameBuffer()
{
	assert((frameBuffer == FB_16BPP) || (frameBuffer == FB_32BPP));
	unsigned width  = getWidth();
	unsigned height = getHeight();

	texture->bind();
	if (frameBuffer == FB_16BPP) {
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height,
		                GL_RGB, GL_UNSIGNED_SHORT_5_6_5, buffer);
	} else {
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height,
		                GL_BGRA, GL_UNSIGNED_BYTE, buffer);
	}

	glEnable(GL_TEXTURE_2D);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glBegin(GL_QUADS);
	glTexCoord2f(0.0,       texCoordY); glVertex2i(0,     height);
	glTexCoord2f(texCoordX, texCoordY); glVertex2i(width, height);
	glTexCoord2f(texCoordX, 0.0      ); glVertex2i(width, 0     );
	glTexCoord2f(0.0,       0.0      ); glVertex2i(0,     0     );
	glEnd();
	glDisable(GL_TEXTURE_2D);
}

void SDLGLVisibleSurface::finish()
{
	SDL_GL_SwapBuffers();
}

void SDLGLVisibleSurface::takeScreenShot(const std::string& filename)
{
	unsigned width  = getWidth();
	unsigned height = getHeight();
	VLA(const void*, rowPointers, height);
	std::vector<byte> buffer(width * height * 3);
	for (unsigned i = 0; i < height; ++i) {
		rowPointers[height - 1 - i] = &buffer[width * 3 * i];
	}
	glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, &buffer[0]);
	ScreenShotSaver::save(width, height, rowPointers, filename);
}

std::auto_ptr<Layer> SDLGLVisibleSurface::createSnowLayer()
{
	return std::auto_ptr<Layer>(new GLSnow(getWidth(), getHeight()));
}

std::auto_ptr<Layer> SDLGLVisibleSurface::createConsoleLayer(
		Reactor& reactor)
{
	const bool openGL = true;
	return std::auto_ptr<Layer>(new OSDConsoleRenderer(reactor, *this, openGL));
}

std::auto_ptr<Layer> SDLGLVisibleSurface::createOSDGUILayer(OSDGUI& gui)
{
	return std::auto_ptr<Layer>(new GLOSDGUILayer(gui));
}

} // namespace openmsx
