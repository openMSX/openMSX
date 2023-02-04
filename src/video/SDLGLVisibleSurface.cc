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

#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_opengl3.h"
#include "imgui_memory_editor.h"

#include "GLUtil.hh"
#include "Reactor.hh"
#include "MSXMotherBoard.hh"
#include "MSXCPUInterface.hh"
#include "CommandController.hh"

bool ImGuiInitialized = false;

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
	//SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
	//SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24); // imgui ?
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8); // imgui ?
#if OPENGL_VERSION == OPENGL_ES_2_0
	#define VERSION_STRING "openGL ES 2.0"
	const char* glsl_version = "#version 100";
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
	const char* glsl_version = "#version 150";
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
	SDL_GL_MakeCurrent(window.get(), glContext); // imgui ?

	//////////////////////
	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiInitialized = true;
	ImGuiIO& io = ImGui::GetIO();
	(void)io;
	io.ConfigFlags |=
	        ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
	// io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable
	// Gamepad Controls
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable; // Enable Docking
	io.ConfigFlags |=
	        ImGuiConfigFlags_ViewportsEnable; // Enable Multi-Viewport /
	                                          // Platform Windows
	// io.ConfigViewportsNoAutoMerge = true;
	// io.ConfigViewportsNoTaskBarIcon = true;

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();
	// ImGui::StyleColorsLight();

	// When viewports are enabled we tweak WindowRounding/WindowBg so
	// platform windows can look identical to regular ones.
	ImGuiStyle& style = ImGui::GetStyle();
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
		style.WindowRounding = 0.0f;
		style.Colors[ImGuiCol_WindowBg].w = 1.0f;
	}

	// Setup Platform/Renderer backends
	ImGui_ImplSDL2_InitForOpenGL(window.get(), glContext);
	ImGui_ImplOpenGL3_Init(glsl_version);
	//////////////////////

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

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();

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

static void HelpMarker(const char* desc)
{
	ImGui::SameLine();
	ImGui::TextDisabled("(?)");
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
		ImGui::BeginTooltip();
		ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
		ImGui::TextUnformatted(desc);
		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
	}
}

void SDLGLVisibleSurface::beginFrame()
{
	// Start the Dear ImGui frame
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplSDL2_NewFrame();
	ImGui::NewFrame();

	// TODO the remainder of this method does not belong in this class, but good enough for a prototype

	auto& display = getDisplay();
	auto& rendererSettings = display.getRenderSettings();
	auto& reactor = display.getReactor();
	auto& commandController = reactor.getCommandController();

	static bool show_demo_window = false;
	if (show_demo_window) {
		ImGui::ShowDemoWindow(&show_demo_window);
	}

	static MemoryEditor mem_edit;
	if (mem_edit.Open) {
		if (auto* motherBoard = reactor.getMotherBoard()) {
			auto& cpuInterface = motherBoard->getCPUInterface();
			auto& memoryDebug = cpuInterface.getMemoryDebuggable();
			std::array<uint8_t, 0x10000> data;
			// Stupid implementation, can be improved in the future.
			// So that also writes are supported
			for (unsigned i = 0; i < 0x10000; ++i) {
				data[i] = memoryDebug.read(i);
			}
			mem_edit.DrawWindow("Memory Editor", data.data(), 0x10000);
		}
	}

	ImGui::Begin("OpenMSX ImGui integration proof of concept");

	ImGui::Checkbox("ImGui Demo Window", &show_demo_window);
	HelpMarker("Show the ImGui demo window.\n"
	           "This is purely to demonstrate the ImGui capabilities.\n"
	           "There is no connection with any openMSX functionality.");

	ImGui::Checkbox("memory view", &mem_edit.Open);
	HelpMarker("In the distant future we might want to integrate the debugger.");

	if (ImGui::Button("Reset MSX")) {
		commandController.executeCommand("reset");
	}
	HelpMarker("Reset the emulated MSX machine.");

	// This takes currently 4 lines of code. But we can/should simplify
	// this further. E.g. so that you don't need to repeat the min/max.
	// Or that you don't need to manually pull/push the internal setting value.
	auto& noiseSetting = rendererSettings.getNoiseSetting();
	float noise = noiseSetting.getFloat();
	ImGui::SliderFloat("noise", &noise, 0.0f, 100.0f, "%.1f");
	noiseSetting.setDouble(noise); // TODO setFloat()
	HelpMarker("Stupid example. But good enough to demonstrate that this stuff is very easy.");

	ImGui::End();
}

void SDLGLVisibleSurface::endFrame()
{
	// Rendering
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	ImGui::Render();
	glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
	//glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	//glClear(GL_COLOR_BUFFER_BIT);
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

	// Update and Render additional Platform Windows
	// (Platform functions may change the current OpenGL context, so we
	// save/restore it to make it easier to paste this code elsewhere.
	//  For this specific demo app we could also call
	//  SDL_GL_MakeCurrent(window, gl_context) directly)
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
		SDL_Window* backup_current_window = SDL_GL_GetCurrentWindow();
		SDL_GLContext backup_current_context =
		        SDL_GL_GetCurrentContext();
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
		SDL_GL_MakeCurrent(backup_current_window,
		                   backup_current_context);
	}

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
