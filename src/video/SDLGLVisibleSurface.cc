#include "SDLGLVisibleSurface.hh"
#include "SDLGLOffScreenSurface.hh"
#include "GLContext.hh"
#include "GLSnow.hh"
#include "OSDConsoleRenderer.hh"
#include "OSDGUILayer.hh"
#include "Display.hh"
#include "RenderSettings.hh"
#include "PNG.hh"
#include "MemBuffer.hh"
#include "outer.hh"
#include "vla.hh"
#include "InitException.hh"
#include <memory>

#include "GLUtil.hh"

namespace openmsx {

SDLGLVisibleSurface::SDLGLVisibleSurface(
		int width, int height,
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
#if OPENGL_VERSION == OPENGL_ES_2_0
	#define VERSION_STRING "openGL ES 2.0"
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#elif OPENGL_VERSION == OPENGL_2_1
	#define VERSION_STRING "openGL 2.1"
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
#elif OPENGL_VERSION == OPENGL_3_3
	#define VERSION_STRING "openGL 3.3"
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
#endif

	int flags = SDL_WINDOW_OPENGL;
	//flags |= SDL_RESIZABLE;
	createSurface(width, height, flags);

	glContext = SDL_GL_CreateContext(window.get());
	if (!glContext) {
		throw InitException(
			"Failed to create " VERSION_STRING " context: ", SDL_GetError());
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

	// GLEW fails to initialise on Wayland because it has no GLX, since the
	// one provided by the distros was built to use GLX instead of EGL. We
	// ignore the GLEW_ERROR_NO_GLX_DISPLAY error with this temporary fix
	// until it is fixed by GLEW upstream and released by major distros.
	// See https://github.com/nigels-com/glew/issues/172
	if (glew_error != GLEW_OK && glew_error != GLEW_ERROR_NO_GLX_DISPLAY) {
		throw InitException(
			"Failed to init GLEW: ",
			reinterpret_cast<const char*>(
				glewGetErrorString(glew_error)));
	}
	if (!GLEW_VERSION_2_1) {
		throw InitException(
			"Need at least OpenGL version " VERSION_STRING);
	}

	bool fullScreen = getDisplay().getRenderSettings().getFullScreen();
	setViewPort(gl::ivec2(width, height), fullScreen); // set initial values

	setOpenGlPixelFormat();
	gl::context.emplace(width, height);

	getDisplay().getRenderSettings().getVSyncSetting().attach(vSyncObserver);
	// set initial value
	vSyncObserver.update(getDisplay().getRenderSettings().getVSyncSetting());

#if OPENGL_VERSION == OPENGL_3_3
	// We don't (yet/anymore) use VAO, but apparently it's required in openGL 3.3.
	// Luckily this workaround is sufficient: create one global VAO and then don't care anymore.
	GLuint vao;
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);
#endif
}

SDLGLVisibleSurface::~SDLGLVisibleSurface()
{
	getDisplay().getRenderSettings().getVSyncSetting().detach(vSyncObserver);

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

	// OpenGL ES only supports reading RGBA (not RGB)
	MemBuffer<uint8_t> buffer(4 * size_t(w) * size_t(h));
	glReadPixels(x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE, buffer.data());

	// perform in-place conversion of RGBA -> RGB
	VLA(const void*, rowPointers, h);
	for (auto i : xrange(size_t(h))) {
		uint8_t* out = &buffer[4 * size_t(w) * i];
		const uint8_t* in = out;
		rowPointers[h - 1 - i] = out;

		for (auto j : xrange(size_t(w))) {
			out[3 * j + 0] = in[4 * j + 0];
			out[3 * j + 1] = in[4 * j + 1];
			out[3 * j + 2] = in[4 * j + 2];
		}
	}

	PNG::save(w, rowPointers, filename);
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

void SDLGLVisibleSurface::VSyncObserver::update(const Setting& setting) noexcept
{
	auto& visSurface = OUTER(SDLGLVisibleSurface, vSyncObserver);
	auto& syncSetting = visSurface.getDisplay().getRenderSettings().getVSyncSetting();
	assert(&setting == &syncSetting); (void)setting;

	// for now, we assume that adaptive vsync is the best kind of vsync, so when
	// vsync is enabled, we attempt adaptive vsync.
	int interval = syncSetting.getBoolean() ? -1 : 0;

	if ((SDL_GL_SetSwapInterval(interval) < 0) && (interval == -1)) {
		// "Adaptive vsync" is not supported by all drivers. SDL
		// documentation suggests to fallback to "regular vsync" in
		// that case.
		SDL_GL_SetSwapInterval(1);
	}
}

void SDLGLVisibleSurface::setViewPort(gl::ivec2 logicalSize, bool fullScreen)
{
	gl::ivec2 physicalSize = [&] {
#ifndef __APPLE__
		// On macos we set 'SDL_WINDOW_ALLOW_HIGHDPI', and in that case
		// it's required to use SDL_GL_GetDrawableSize(), but then this
		// 'full screen'-workaround/hack is counter-productive.
		if (!fullScreen) {
			// ??? When switching  back from full screen to windowed mode,
			// SDL_GL_GetDrawableSize() still returns the dimensions of the
			// full screen window ??? Is this a bug ???
			// But we know that in windowed mode, physical and logical size
			// must be the same, so enforce that.
			return logicalSize;
		}
#endif
		(void)fullScreen;
		int w, h;
		SDL_GL_GetDrawableSize(window.get(), &w, &h);
		return gl::ivec2(w, h);
	}();

	// The created surface may be larger than requested.
	// If that happens, center the area that we actually use.
	calculateViewPort(logicalSize, physicalSize);
	auto [vx, vy] = getViewOffset();
	auto [vw, vh] = getViewSize();
	glViewport(vx, vy, vw, vh);
}

void SDLGLVisibleSurface::fullScreenUpdated(bool fullScreen)
{
	setViewPort(getLogicalSize(), fullScreen);
}

} // namespace openmsx
