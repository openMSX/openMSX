#include "ImGuiLayer.hh"

#include "ImGuiCpp.hh"
#include "ImGuiManager.hh"

#include "Keyboard.hh"
#include "MSXMotherBoard.hh"
#include "Reactor.hh"

#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl3.h>
#include <imgui_internal.h>

#include <SDL3/SDL.h>

namespace openmsx {

ImGuiLayer::ImGuiLayer(ImGuiManager& manager_)
	: Layer(Layer::Coverage::PARTIAL, Layer::ZIndex::IMGUI)
	, manager(manager_)
{
}

// In fullscreen mode the window covers the whole display, including the area
// that is normally reserved for a taskbar or panel. SDL still reports that
// area as being outside the monitor's work area. Dear ImGui clamps popups
// (e.g. the main menu) to the monitor's work area, which causes a gap below
// the menu bar in fullscreen. Use the full display bounds in that case.
static void updateMonitorWorkArea()
{
	auto* window = SDL_GetWindowFromID(WindowEvent::getMainWindowId());
	if (!window) return;

	const SDL_DisplayID display = SDL_GetDisplayForWindow(window);
	if (!display) return;

	SDL_Rect mainRect;
	if (!SDL_GetDisplayBounds(display, &mainRect)) return;

	SDL_Rect workRect = mainRect;
	if (!(SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) &&
	    (!SDL_GetDisplayUsableBounds(display, &workRect) ||
	     workRect.w <= 0 || workRect.h <= 0)) {
		workRect = mainRect;
	}

	for (auto& monitor : ImGui::GetPlatformIO().Monitors) {
		if (monitor.MainPos.x == float(mainRect.x) &&
		    monitor.MainPos.y == float(mainRect.y) &&
		    monitor.MainSize.x == float(mainRect.w) &&
		    monitor.MainSize.y == float(mainRect.h)) {
			monitor.WorkPos = ImVec2(float(workRect.x), float(workRect.y));
			monitor.WorkSize = ImVec2(float(workRect.w), float(workRect.h));
			break;
		}
	}
}

void ImGuiLayer::paint(OutputSurface& /*surface*/)
{
	manager.preNewFrame();

	// Start the Dear ImGui frame
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplSDL3_NewFrame();
	updateMonitorWorkArea();
	// Windows that are restored as open (from imgui.ini) and lie (partly)
	// outside the main window get an OS window of their own. Window managers
	// tend to give such a new OS window the focus, and ImGui then also moves
	// its keyboard focus there. Ignore such OS focus changes shortly after
	// startup, so that the MSX keeps the keyboard focus. (ImGui's time is
	// the time since the first frame.)
	ImGui::GetIO().ConfigViewportsPlatformFocusSetsImGuiFocus = ImGui::GetTime() > 1.0;
	ImGui::NewFrame();

	// Allow docking in main window
	auto* mainViewport = ImGui::GetMainViewport();
	auto dockspaceId = 0;
	im::StyleVar(ImGuiStyleVar_Alpha, /*transparent*/0.0f, [&]{
		dockspaceId = ImGui::DockSpaceOverViewport(0, mainViewport,
			ImGuiDockNodeFlags_NoDockingOverCentralNode |
			ImGuiDockNodeFlags_PassthruCentralNode |
			ImGuiDockNodeFlags_AutoHideTabBar);
	});

	// Main content window: focus target for the MSX display area.  Docked
	// into central node so it receives clicks there.  When focused, we
	// override WantCapture so input goes to the application (see
	// ImGuiManager).
	ImGui::SetNextWindowPos(mainViewport->Pos);
	ImGui::SetNextWindowSize(mainViewport->Size);
	auto flags = ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoBackground |
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoScrollbar |
			ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoBringToFrontOnFocus;
	bool focus = false;
	im::Window("MSX Display Area", nullptr, flags, [&]{
		// If no other window currently has focus, then take it ourselves.
		if (ImGui::GetCurrentContext()->NavWindow == nullptr) {
			ImGui::SetWindowFocus();
		}

		focus = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootWindow);
		if (focus) {
			ImGui::SetNextFrameWantCaptureMouse(false);
			ImGui::SetNextFrameWantCaptureKeyboard(false);
		}
		if (auto* motherBoard = manager.getReactor().getMotherBoard()) {
			if (auto* keyb = motherBoard->getKeyboard()) {
				auto time = motherBoard->getCurrentTime();
				keyb->setFocus(focus, time);
			}
		}
	});

	manager.paintImGui(focus);

	// Ensure this window has focus on startup: windows that are restored
	// as open (from imgui.ini) take the focus when they first appear, so do
	// this at the end of the first frame, after they appeared.
	if (ImGui::GetFrameCount() == 1) {
		if (auto* window = ImGui::FindWindowByName("MSX Display Area")) {
			ImGui::FocusWindow(window);
		}
	}

	// Rendering
	const ImGuiIO& io = ImGui::GetIO();
	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

	// Update and Render additional Platform Windows
	// (Platform functions may change the current OpenGL context, so we
	// save/restore it to make it easier to paste this code elsewhere.
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
		SDL_Window* backup_current_window = SDL_GL_GetCurrentWindow();
		SDL_GLContext backup_current_context = SDL_GL_GetCurrentContext();
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
		SDL_GL_MakeCurrent(backup_current_window, backup_current_context);

#ifdef __APPLE__
		if (ImGui::GetFrameCount() == 1) {
			// On startup, bring main openMSX window to front, which it doesn't
			// do it by itself due to creation of platform windows for viewports.
			SDL_RaiseWindow(SDL_GetWindowFromID(WindowEvent::getMainWindowId()));
		}
#endif
	}
}

} // namespace openmsx
