#include "ImGuiLayer.hh"

#include "ImGuiManager.hh"

#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl2.h>

#include <SDL3/SDL.h>

namespace openmsx {

ImGuiLayer::ImGuiLayer(ImGuiManager& manager_)
	: Layer(Layer::Coverage::PARTIAL, Layer::ZIndex::IMGUI)
	, manager(manager_)
{
}

void ImGuiLayer::paint(OutputSurface& /*surface*/)
{
	manager.preNewFrame();

	// Start the Dear ImGui frame
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplSDL2_NewFrame();
	ImGui::NewFrame();

	manager.paintImGui();

	// Allow docking in main window
	ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(),
		ImGuiDockNodeFlags_NoDockingOverCentralNode |
		ImGuiDockNodeFlags_PassthruCentralNode);

	if (ImGui::GetFrameCount() == 1) {
		// on startup, focus main openMSX window instead of the GUI window
		ImGui::SetWindowFocus(nullptr);
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
