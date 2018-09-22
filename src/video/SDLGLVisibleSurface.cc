#include "SDLGLVisibleSurface.hh"
#include "SDLGLOffScreenSurface.hh"
#include "GLContext.hh"
#include "GLSnow.hh"
#include "OSDConsoleRenderer.hh"
#include "OSDGUILayer.hh"
#include "InitException.hh"
#include "RenderSettings.hh"
#include <memory>

namespace openmsx {

SDLGLVisibleSurface::SDLGLVisibleSurface(
		unsigned width, unsigned height,
		Display& display_,
		RTScheduler& rtScheduler_,
		EventDistributor& eventDistributor_,
		InputEventGenerator& inputEventGenerator_,
		CliComm& cliComm_,
		FrameBuffer frameBuffer_)
	: VisibleSurface(display_, rtScheduler_, eventDistributor_, inputEventGenerator_,
			cliComm_)
	, SDLGLOutputSurface(frameBuffer_)
{
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);
	int flags = SDL_OPENGL | SDL_HWSURFACE | SDL_DOUBLEBUF;
	//flags |= SDL_RESIZABLE;
	createSurface(width, height, flags);

	// The created surface may be larger than requested.
	// If that happens, center the area that we actually use.
	SDL_Surface* surf = getSDLSurface();
	unsigned actualWidth  = surf->w;
	unsigned actualHeight = surf->h;
	surf->w = width;
	surf->h = height;
	setPosition((actualWidth - width ) / 2, (actualHeight - height) / 2);

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
			"Failed to init GLEW: ",
			reinterpret_cast<const char*>(
				glewGetErrorString(glew_error)));
	}
	if (!GLEW_VERSION_2_0) {
		throw InitException("Need at least openGL version 2.0.");
	}

	glViewport(getX(), getY(), width, height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, width, height, 0, -1, 1);
	glMatrixMode(GL_MODELVIEW);

	// This stuff logically belongs in the SDLGLOutputSurface constructor,
	// but it cannot be executed before the openGL context is created which
	// is done in this constructor. So construction of SDLGLOutputSurface
	// is split in two phases.
	SDLGLOutputSurface::init(*this);

	gl::context = std::make_unique<gl::Context>(width, height);
}

SDLGLVisibleSurface::~SDLGLVisibleSurface()
{
	gl::context.reset();
}

void SDLGLVisibleSurface::flushFrameBuffer()
{
	SDLGLOutputSurface::flushFrameBuffer(getWidth(), getHeight());
}

void SDLGLVisibleSurface::clearScreen()
{
	SDLGLOutputSurface::clearScreen();
}

void SDLGLVisibleSurface::saveScreenshot(const std::string& filename)
{
	SDLGLOutputSurface::saveScreenshot(filename, getWidth(), getHeight());
}

void SDLGLVisibleSurface::finish()
{
	SDL_GL_SwapBuffers();
}

std::unique_ptr<Layer> SDLGLVisibleSurface::createSnowLayer()
{
	return std::make_unique<GLSnow>(getDisplay());
}

std::unique_ptr<Layer> SDLGLVisibleSurface::createConsoleLayer(
		Reactor& reactor, CommandConsole& console)
{
	const bool openGL = true;
	return std::make_unique<OSDConsoleRenderer>(
		reactor, console, getWidth(), getHeight(), openGL);
}

std::unique_ptr<Layer> SDLGLVisibleSurface::createOSDGUILayer(OSDGUI& gui)
{
	return std::make_unique<GLOSDGUILayer>(gui);
}

std::unique_ptr<OutputSurface> SDLGLVisibleSurface::createOffScreenSurface()
{
	return std::make_unique<SDLGLOffScreenSurface>(*this);
}

} // namespace openmsx
