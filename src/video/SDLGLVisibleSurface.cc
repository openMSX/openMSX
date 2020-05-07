#include "SDLGLVisibleSurface.hh"
#include "SDLGLOffScreenSurface.hh"
#include "GLContext.hh"
#include "GLSnow.hh"
#include "OSDConsoleRenderer.hh"
#include "OSDGUILayer.hh"
#include "Display.hh"
#include "RenderSettings.hh"
#include "CliComm.hh"
#include "PNG.hh"
#include "build-info.hh"
#include "MemBuffer.hh"
#include "outer.hh"
#include "vla.hh"
#include "InitException.hh"
#include "unreachable.hh"
#include <memory>

namespace openmsx {

SDLGLVisibleSurface::SDLGLVisibleSurface(
		unsigned width, unsigned height,
		Display& display_,
		RTScheduler& rtScheduler_,
		EventDistributor& eventDistributor_,
		InputEventGenerator& inputEventGenerator_,
		CliComm& cliComm_,
		VideoSystem& videoSystem_)
	: SDLVisibleSurfaceBase(display_, rtScheduler_, eventDistributor_,
	                        inputEventGenerator_, cliComm_, videoSystem_)
{
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);
	int flags = SDL_WINDOW_OPENGL;
	//flags |= SDL_RESIZABLE;
	createSurface(width, height, flags);

	// Create an OpenGL 3.3 Core profile
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	glContext = SDL_GL_CreateContext(window.get());
	if (!glContext) {
		throw InitException(
			"Failed to create openGL-3.3 context: ", SDL_GetError());
	}

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
	GLenum glew_error = glewInit();
	if (glew_error != GLEW_OK) {
		throw InitException(
			"Failed to init GLEW: ",
			reinterpret_cast<const char*>(
				glewGetErrorString(glew_error)));
	}

	// The created surface may be larger than requested.
	// If that happens, center the area that we actually use.
	int w, h;
	SDL_GL_GetDrawableSize(window.get(), &w, &h);
	calculateViewPort(gl::ivec2(width, height), gl::ivec2(w, h));
	auto [vx, vy] = getViewOffset();
	auto [vw, vh] = getViewSize();
	glViewport(vx, vy, vw, vh);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, width, height, 0, -1, 1);
	glMatrixMode(GL_MODELVIEW);

	setOpenGlPixelFormat();
	gl::context = std::make_unique<gl::Context>(width, height);

	getDisplay().getRenderSettings().getSyncToVBlankModeSetting().attach(syncToVBlankModeObserver);
	// set initial value
	syncToVBlankModeObserver.update(getDisplay().getRenderSettings().getSyncToVBlankModeSetting());
}

SDLGLVisibleSurface::~SDLGLVisibleSurface()
{
	getDisplay().getRenderSettings().getSyncToVBlankModeSetting().detach(syncToVBlankModeObserver);

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
	auto [x, y] = output.getViewOffset();
	auto [w, h] = output.getViewSize();

	VLA(const void*, rowPointers, h);
	MemBuffer<uint8_t> buffer(w * h * 3);
	for (int i = 0; i < h; ++i) {
		rowPointers[h - 1 - i] = &buffer[w * 3 * i];
	}
	glReadPixels(x, y, w, h, GL_RGB, GL_UNSIGNED_BYTE, buffer.data());
	PNG::save(w, h, rowPointers, filename);
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
	auto [width, height] = getLogicalSize();
	return std::make_unique<OSDConsoleRenderer>(
		reactor, console, width, height, openGL);
}

std::unique_ptr<Layer> SDLGLVisibleSurface::createOSDGUILayer(OSDGUI& gui)
{
	return std::make_unique<GLOSDGUILayer>(gui);
}

std::unique_ptr<OutputSurface> SDLGLVisibleSurface::createOffScreenSurface()
{
	return std::make_unique<SDLGLOffScreenSurface>(*this);
}

void SDLGLVisibleSurface::SyncToVBlankModeObserver::update(const Setting& setting)
{
	auto& surface = OUTER(SDLGLVisibleSurface, syncToVBlankModeObserver);
	auto& syncSetting = surface.getDisplay().getRenderSettings().getSyncToVBlankModeSetting();
	assert(&setting == &syncSetting);

	int interval = [&] {
		switch (syncSetting.getEnum()) {
			case RenderSettings::SyncToVBlankMode::IMMEDIATE: return 0;
			case RenderSettings::SyncToVBlankMode::SYNC:      return 1;
			case RenderSettings::SyncToVBlankMode::ADAPTIVE:  return -1;
		}
		UNREACHABLE; return 0;
	}();
	if ((SDL_GL_SetSwapInterval(interval) < 0) && (interval == -1)) {
		// "Adaptive vsync" is not supported by all drivers. SDL
		// documentation suggests to fallback to "regular vsync" in
		// that case.
		surface.getCliComm().printWarning(
				"Adaptive vsync not supported; falling back to regular vsync");
		SDL_GL_SetSwapInterval(1);
	}
}

} // namespace openmsx
