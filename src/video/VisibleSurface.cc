#include "VisibleSurface.hh"

#include "BooleanSetting.hh"
#include "CliComm.hh"
#include "Display.hh"
#include "Event.hh"
#include "EventDistributor.hh"
#include "FileContext.hh"
#include "FloatSetting.hh"
#include "GLContext.hh"
#include "GLSnow.hh"
#include "GLUtil.hh"
#include "Icon.hh"
#include "ImGuiLayer.hh"
#include "InitException.hh"
#include "InputEventGenerator.hh"
#include "MemBuffer.hh"
#include "OffScreenSurface.hh"
#include "OSDGUILayer.hh"
#include "PNG.hh"
#include "RenderSettings.hh"
#include "VideoSystem.hh"

#include "narrow.hh"
#include "outer.hh"
#include "small_buffer.hh"
#include "view.hh"

#include "build-info.hh"

#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_opengl3.h>

#include <bit>
#include <cassert>
#include <memory>

namespace openmsx {

VisibleSurface::VisibleSurface(
		Display& display_,
		RTScheduler& rtScheduler_,
		EventDistributor& eventDistributor_,
		InputEventGenerator& inputEventGenerator_,
		CliComm& cliComm_,
		VideoSystem& videoSystem_)
	: RTSchedulable(rtScheduler_)
	, display(display_)
	, eventDistributor(eventDistributor_)
	, inputEventGenerator(inputEventGenerator_)
	, cliComm(cliComm_)
	, videoSystem(videoSystem_)
{
	auto& renderSettings = display.getRenderSettings();

	inputEventGenerator.getGrabInput().attach(*this);
	renderSettings.getPointerHideDelaySetting().attach(*this);
	renderSettings.getFullScreenSetting().attach(*this);

	for (auto type : {EventType::MOUSE_MOTION,
	                  EventType::MOUSE_BUTTON_DOWN,
	                  EventType::MOUSE_BUTTON_UP,
	                  EventType::IMGUI_ACTIVE}) {
		eventDistributor.registerEventListener(type, *this);
	}

	updateCursor();
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);
#if OPENGL_VERSION == OPENGL_ES_2_0
	#define VERSION_STRING "openGL ES 2.0"
	const char* glsl_version = "#version 100";
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#elif OPENGL_VERSION == OPENGL_2_1
	#define VERSION_STRING "openGL 2.1"
	const char* glsl_version = "#version 120";
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
#elif OPENGL_VERSION == OPENGL_3_3
	#define VERSION_STRING "openGL 3.3"
	const char* glsl_version = "#version 150";
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
#endif

	int flags = SDL_WINDOW_OPENGL;
	//flags |= SDL_RESIZABLE;
	auto size = display.getWindowSize();
	createSurface(size, flags);
	WindowEvent::setMainWindowId(SDL_GetWindowID(window.get()));

	glContext = SDL_GL_CreateContext(window.get());
	if (!glContext) {
		throw InitException(
			"Failed to create " VERSION_STRING " context: ", SDL_GetError());
	}

	// Setup ImGui Platform/Renderer backends
	ImGui_ImplSDL2_InitForOpenGL(window.get(), glContext);
	ImGui_ImplOpenGL3_Init(glsl_version);

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
	// GLEW fails to initialise on Wayland because it has no GLX, since the
	// one provided by the distros was built to use GLX instead of EGL. We
	// ignore the GLEW_ERROR_NO_GLX_DISPLAY error with this temporary fix
	// until it is fixed by GLEW upstream and released by major distros.
	// See https://github.com/nigels-com/glew/issues/172
	if (GLenum glew_error = glewInit();
	    glew_error != GLEW_OK && glew_error != GLEW_ERROR_NO_GLX_DISPLAY) {
		throw InitException(
			"Failed to init GLEW: ",
			std::bit_cast<const char*>(glewGetErrorString(glew_error)));
	}
	if (!GLEW_VERSION_2_1) {
		throw InitException(
			"Need at least OpenGL version " VERSION_STRING);
	}
	gl::context.emplace();

	bool fullScreen = renderSettings.getFullScreen();
	setViewPort(size, fullScreen); // set initial values

	renderSettings.getVSyncSetting().attach(vSyncObserver);
	// set initial value
	vSyncObserver.update(renderSettings.getVSyncSetting());

#if OPENGL_VERSION == OPENGL_3_3
	// We don't (yet/anymore) use VAO, but apparently it's required in openGL 3.3.
	// Luckily this workaround is sufficient: create one global VAO and then don't care anymore.
	GLuint vao;
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);
#endif
}

VisibleSurface::~VisibleSurface()
{
	auto& renderSettings = display.getRenderSettings();
	renderSettings.getVSyncSetting().detach(vSyncObserver);

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL2_Shutdown();

	gl::context.reset();
	SDL_GL_DeleteContext(glContext);

	// store last known position for when we recreate it
	// the window gets recreated when changing renderers, for instance.
	// Do not store if we're full screen, the location is the top-left
	if (auto pos = getWindowPosition()) {
		display.storeWindowPosition(*pos);
	}

	for (auto type : {EventType::IMGUI_ACTIVE,
	                  EventType::MOUSE_BUTTON_UP,
	                  EventType::MOUSE_BUTTON_DOWN,
	                  EventType::MOUSE_MOTION}) {
		eventDistributor.unregisterEventListener(type, *this);
	}

	inputEventGenerator.getGrabInput().detach(*this);
	renderSettings.getPointerHideDelaySetting().detach(*this);
	renderSettings.getFullScreenSetting().detach(*this);
}

std::optional<gl::ivec2> VisibleSurface::getWindowPosition() const
{
	if (SDL_GetWindowFlags(window.get()) & SDL_WINDOW_FULLSCREEN) return {};
	int x, y;
	SDL_GetWindowPosition(window.get(), &x, &y);
	return gl::ivec2{x, y};
}

void VisibleSurface::setWindowPosition(gl::ivec2 pos)
{
	if (SDL_GetWindowFlags(window.get()) & SDL_WINDOW_FULLSCREEN) return;
	SDL_SetWindowPosition(window.get(), pos.x, pos.y);
}

// TODO: The video subsystem is not de-initialized on errors.
//       While it would be consistent to do so, doing it in this class is
//       not ideal since the init doesn't happen here.
void VisibleSurface::createSurface(gl::ivec2 size, unsigned flags)
{
	if (getDisplay().getRenderSettings().getFullScreen()) {
		flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
	}
#ifdef __APPLE__
	// See VisibleSurface::setViewPort() for why only macos (for now).
	flags |= SDL_WINDOW_ALLOW_HIGHDPI;
#endif

	assert(!window);
	auto pos = display.retrieveWindowPosition();
	window.reset(SDL_CreateWindow(
			getDisplay().getWindowTitle().c_str(),
			pos.x, pos.y,
			size.x, size.y,
			flags));
	if (!window) {
		std::string err = SDL_GetError();
		throw InitException("Could not create window: ", err);
	}

	updateWindowTitle();

	// prefer linear filtering (instead of nearest neighbour)
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

	// set icon
	if constexpr (OPENMSX_SET_WINDOW_ICON) {
		SDLSurfacePtr iconSurf;
		// always use 32x32 icon on Windows, for some reason you get badly scaled icons there
#ifndef _WIN32
		try {
			iconSurf = PNG::load(preferSystemFileContext().resolve("icons/openMSX-logo-256.png"), true);
		} catch (MSXException& e) {
			getCliComm().printWarning(
				"Falling back to built in 32x32 icon, because failed to load icon: ",
				e.getMessage());
#endif
			PixelOperations pixelOps;
			iconSurf.reset(SDL_CreateRGBSurfaceFrom(
				const_cast<char*>(openMSX_icon.pixel_data),
				narrow<int>(openMSX_icon.width),
				narrow<int>(openMSX_icon.height),
				narrow<int>(openMSX_icon.bytes_per_pixel * 8),
				narrow<int>(openMSX_icon.bytes_per_pixel * openMSX_icon.width),
				pixelOps.getRmask(),
				pixelOps.getGmask(),
				pixelOps.getBmask(),
				pixelOps.getAmask()));
#ifndef _WIN32
		}
#endif
		SDL_SetColorKey(iconSurf.get(), SDL_TRUE, 0);
		SDL_SetWindowIcon(window.get(), iconSurf.get());
	}
}

void VisibleSurface::update(const Setting& /*setting*/) noexcept
{
	updateCursor();
}

void VisibleSurface::executeRT()
{
	// timer expired, hide cursor
	videoSystem.showCursor(false);
	inputEventGenerator.updateGrab(grab);
}

bool VisibleSurface::signalEvent(const Event& event)
{
	if (getType(event) == EventType::IMGUI_ACTIVE) {
		guiActive = get_event<ImGuiActiveEvent>(event).getActive();
	}
	updateCursor();
	return false;
}

void VisibleSurface::updateCursor()
{
	cancelRT();
	const auto& renderSettings = display.getRenderSettings();
	grab = !guiActive &&
	       (renderSettings.getFullScreen() ||
	        inputEventGenerator.getGrabInput().getBoolean());
	if (grab) {
		// always hide cursor in fullscreen or grab-input mode, but do it
		// after the derived class is constructed to avoid an SDL bug.
		scheduleRT(0);
		return;
	}
	inputEventGenerator.updateGrab(grab);
	float delay = renderSettings.getPointerHideDelay();
	if (delay == 0.0f) {
		videoSystem.showCursor(false);
	} else {
		videoSystem.showCursor(true);
		if (delay > 0.0f) {
			scheduleRT(int(delay * 1e6f)); // delay in s, schedule in us
		}
	}
}

bool VisibleSurface::setFullScreen(bool fullscreen)
{
	auto flags = SDL_GetWindowFlags(window.get());
	// Note: SDL_WINDOW_FULLSCREEN_DESKTOP also has the SDL_WINDOW_FULLSCREEN
	//       bit set.
	if (bool currentState = (flags & SDL_WINDOW_FULLSCREEN) != 0;
	    currentState == fullscreen) {
		// already wanted stated
		return true;
	}

	if (SDL_SetWindowFullscreen(window.get(),
			fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0) != 0) {
		return false; // error, try re-creating the window
	}
	fullScreenUpdated(fullscreen);
	return true; // success
}

void VisibleSurface::resize()
{
	auto size = display.getWindowSize();
	SDL_SetWindowSize(window.get(), size.x, size.y);

	bool fullScreen = display.getRenderSettings().getFullScreen();
	setViewPort(size, fullScreen);
}

void VisibleSurface::updateWindowTitle()
{
	assert(window);
	SDL_SetWindowTitle(window.get(), getDisplay().getWindowTitle().c_str());
}

void VisibleSurface::saveScreenshot(const std::string& filename)
{
	saveScreenshotGL(*this, filename);
}

void VisibleSurface::saveScreenshotGL(
	const OutputSurface& output, const std::string& filename)
{
	auto [x, y] = output.getViewOffset();
	auto [w_, h_] = output.getViewSize();
	auto w = w_; // pre-clang-16 workaround
	auto h = h_;

	// OpenGL ES only supports reading RGBA (not RGB)
	MemBuffer<uint32_t> buffer(size_t(w) * size_t(h));
	glReadPixels(x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE, buffer.data());

	small_buffer<const uint32_t*, 1080> rowPointers(view::transform(xrange(size_t(h)),
		[&](auto i) { return &buffer[size_t(w) * (h - 1 - i)]; }));

	PNG::saveRGBA(w, rowPointers, filename);
}

void VisibleSurface::finish()
{
	SDL_GL_SwapWindow(window.get());
}

std::unique_ptr<Layer> VisibleSurface::createSnowLayer()
{
	return std::make_unique<GLSnow>(getDisplay());
}

std::unique_ptr<Layer> VisibleSurface::createOSDGUILayer(OSDGUI& gui)
{
	return std::make_unique<OSDGUILayer>(gui);
}

std::unique_ptr<Layer> VisibleSurface::createImGUILayer(ImGuiManager& manager)
{
	return std::make_unique<ImGuiLayer>(manager);
}

std::unique_ptr<OutputSurface> VisibleSurface::createOffScreenSurface()
{
	return std::make_unique<OffScreenSurface>(*this);
}

void VisibleSurface::VSyncObserver::update(const Setting& setting) noexcept
{
	const auto& visSurface = OUTER(VisibleSurface, vSyncObserver);
	const auto& syncSetting = visSurface.getDisplay().getRenderSettings().getVSyncSetting();
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

void VisibleSurface::setViewPort(gl::ivec2 logicalSize, bool fullScreen)
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
	// actually setting the viewport is done in PostProcessor::paint()

	gl::context->setupMvpMatrix(gl::vec2(logicalSize));
}

void VisibleSurface::fullScreenUpdated(bool fullScreen)
{
	setViewPort(getLogicalSize(), fullScreen);
}

} // namespace openmsx
