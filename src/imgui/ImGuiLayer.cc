#include "ImGuiLayer.hh"
#include "ImGuiDebugger.hh"
#include "ImGuiMachine.hh"
#include "ImGuiManager.hh"
#include "ImGuiOpenFile.hh"
#include "ImGuiUtils.hh"

#include "GlobalCommandController.hh"
#include "Reactor.hh"

#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_opengl3.h>
#include <imgui_stdlib.h>

#include <SDL.h>

namespace openmsx {

ImGuiLayer::ImGuiLayer(Reactor& reactor_)
	: Layer(Layer::COVER_PARTIAL, Layer::Z_IMGUI)
	, reactor(reactor_)
{
}

ImGuiLayer::~ImGuiLayer() = default;

void ImGuiLayer::paint(OutputSurface& /*surface*/)
{
	// Start the Dear ImGui frame
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplSDL2_NewFrame();
	ImGui::NewFrame();

	auto* motherBoard = reactor.getMotherBoard();
	auto& manager = reactor.getImGuiManager();
	manager.paint();

	// Allow docking in main window
	ImGui::DockSpaceOverViewport(ImGui::GetMainViewport(),
		ImGuiDockNodeFlags_NoDockingInCentralNode |
		ImGuiDockNodeFlags_PassthruCentralNode);

	if (ImGui::BeginMainMenuBar()) {
		manager.machine.showMenu(motherBoard);
		manager.media.showMenu(motherBoard);
		manager.connector.showMenu(motherBoard);
		manager.reverseBar.showMenu(motherBoard);
		manager.settings.showMenu();
		manager.debugger.showMenu(motherBoard);
		manager.help.showMenu();
		ImGui::EndMainMenuBar();
	}

	if (first) {
		// on startup, focus main openMSX window instead of the GUI window
		first = false;
		ImGui::SetWindowFocus(nullptr);
	}

	// Rendering
	ImGuiIO& io = ImGui::GetIO();
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
	}
}

} // namespace openmsx
