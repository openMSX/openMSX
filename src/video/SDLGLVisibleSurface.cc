#include "SDLGLVisibleSurface.hh"
#include "SDLGLOffScreenSurface.hh"
#include "GLContext.hh"
#include "GLSnow.hh"
#include "OSDConsoleRenderer.hh"
#include "OSDGUILayer.hh"
#include "PNG.hh"
#include "build-info.hh"
#include "MemBuffer.hh"
#include "vla.hh"
#include "InitException.hh"
#include <memory>

namespace openmsx {

SDLGLVisibleSurface::SDLGLVisibleSurface(
		unsigned width, unsigned height,
		Display& display_,
		RTScheduler& rtScheduler_,
		EventDistributor& eventDistributor_,
		InputEventGenerator& inputEventGenerator_,
		CliComm& cliComm_)
	: VisibleSurface(display_, rtScheduler_, eventDistributor_, inputEventGenerator_,
			cliComm_)
{
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);
	int flags = SDL_WINDOW_OPENGL;
	//flags |= SDL_RESIZABLE;
	createSurface(width, height, flags);
	glContext = SDL_GL_CreateContext(window.get());

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

	// The created surface may be larger than requested.
	// If that happens, center the area that we actually use.
	int w, h;
	SDL_GL_GetDrawableSize(window.get(), &w, &h);
	calculateViewPort(gl::ivec2(w, h));
	gl::ivec2 offset = getViewOffset();
	gl::ivec2 size   = getViewSize();
	glViewport(offset[0], offset[1], size[0], size[1]);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, width, height, 0, -1, 1);
	glMatrixMode(GL_MODELVIEW);

	SDLAllocFormatPtr frmt(SDL_AllocFormat(
		        OPENMSX_BIGENDIAN ? SDL_PIXELFORMAT_RGBA8888 :
		                            SDL_PIXELFORMAT_ARGB8888));
	setSDLFormat(*frmt);
	setBufferPtr(nullptr, 0); // direct access not allowed

	gl::context = std::make_unique<gl::Context>(width, height);
}

SDLGLVisibleSurface::~SDLGLVisibleSurface()
{
	// TODO: Move context creation/deletion into Context class?
	gl::context.reset();
	SDL_GL_DeleteContext(glContext);
}

void SDLGLVisibleSurface::saveScreenshot(const std::string& filename)
{
	saveScreenshotGL(*this, filename);
}

void SDLGLVisibleSurface::saveScreenshotGL(
	const OutputSurface& output, const std::string& filename)
{
	gl::ivec2 offset = output.getViewOffset();
	gl::ivec2 size   = output.getViewSize();

	VLA(const void*, rowPointers, size[1]);
	MemBuffer<uint8_t> buffer(size[0] * size[1] * 3);
	for (int i = 0; i < size[1]; ++i) {
		rowPointers[size[1] - 1 - i] = &buffer[size[0] * 3 * i];
	}
	glReadPixels(offset[0], offset[1], size[0], size[1], GL_RGB, GL_UNSIGNED_BYTE, buffer.data());
	PNG::save(size[0], size[1], rowPointers, filename);
}

void SDLGLVisibleSurface::finish()
{
	SDL_GL_SwapWindow(window.get());
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
