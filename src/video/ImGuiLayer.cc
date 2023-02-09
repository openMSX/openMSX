#include "ImGuiLayer.hh"

#include "CommandController.hh"
#include "Connector.hh"
#include "Debugger.hh"
#include "Display.hh"
#include "FloatSetting.hh"
#include "GLUtil.hh"
#include "MSXMotherBoard.hh"
#include "MSXCPUInterface.hh"
#include "Pluggable.hh"
#include "PluggingController.hh"
#include "Reactor.hh"
#include "RealDrive.hh"

#include <imgui.h>
#include <imgui_impl_sdl.h>
#include <imgui_impl_opengl3.h>
#include <imgui_memory_editor.h>

#include <SDL.h>

namespace openmsx {

static void simpleToolTip(const char* desc)
{
	if (ImGui::IsItemHovered()) {
		ImGui::BeginTooltip();
		ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
		ImGui::TextUnformatted(desc);
		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
	}
}

static void HelpMarker(const char* desc)
{
	ImGui::SameLine();
	ImGui::TextDisabled("(?)");
	simpleToolTip(desc);
}

template<typename Getter, typename Setter>
static bool Checkbox(const char* label, Getter getter, Setter setter)
{
	bool value = getter();
	bool changed = ImGui::Checkbox(label, &value);
	if (changed) setter(value);
	return changed;
}

// Should we put these helpers in the ImGui namespace?
static bool SliderFloat(const char* label, FloatSetting& setting, const char* format = "%.3f", ImGuiSliderFlags flags = 0)
{
	float value = setting.getFloat();
	float min = narrow_cast<float>(setting.getMinValue());
	float max = narrow_cast<float>(setting.getMaxValue());
	bool changed = ImGui::SliderFloat(label, &value, min, max, format, flags);
	if (changed) setting.setDouble(value); // TODO setFloat()
	return changed;
}

struct DebuggableEditor : public MemoryEditor
{
	DebuggableEditor() {
		Open = false;
		ReadFn = [](const ImU8* userdata, size_t offset) -> ImU8 {
			auto* debuggable = reinterpret_cast<Debuggable*>(const_cast<ImU8*>(userdata));
			return debuggable->read(narrow<unsigned>(offset));
		};
		WriteFn = [](ImU8* userdata, size_t offset, ImU8 data) -> void {
			auto* debuggable = reinterpret_cast<Debuggable*>(userdata);
			debuggable->write(narrow<unsigned>(offset), data);
		};
	}

	void DrawWindow(const char* title, Debuggable& debuggable, size_t base_display_addr = 0x0000) {
		MemoryEditor::DrawWindow(title, &debuggable, debuggable.getSize(), base_display_addr);
	}
};

ImGuiLayer::ImGuiLayer(Reactor& reactor_)
	: Layer(Layer::COVER_PARTIAL, Layer::Z_IMGUI)
	, reactor(reactor_)
	, interp(reactor.getInterpreter())
{
}

ImGuiLayer::~ImGuiLayer()
{
}

std::optional<TclObject> ImGuiLayer::execute(TclObject command)
{
	if (!command.empty()) {
		try {
			return command.executeCommand(interp);
		} catch (CommandException&) {
			// ignore
		}
	}
	return {};
}

void ImGuiLayer::mediaMenu(MSXMotherBoard* motherBoard)
{
	if (!ImGui::BeginMenu("Media", motherBoard != nullptr)) {
		return;
	}

	TclObject command;
	if (motherBoard) {
		bool needSeparator = false;
		auto addSeparator = [&] {
			if (needSeparator) {
				needSeparator = false;
				ImGui::Separator();
			}
		};
		auto showCurrent = [&](TclObject current, const char* type) {
			if (current.empty()) {
				ImGui::Text("no %s inserted", type);
			} else {
				ImGui::Text("Current: %s", current.getString().c_str());
			}
			ImGui::Separator();
		};

		// diskX
		auto drivesInUse = RealDrive::getDrivesInUse(*motherBoard);
		std::string driveName = "diskX";
		for (auto i : xrange(RealDrive::MAX_DRIVES)) {
			if (!(*drivesInUse)[i]) continue;
			driveName[4] = char('a' + i);
			if (auto cmdResult = execute(TclObject(driveName))) {
				if (ImGui::BeginMenu(driveName.c_str())) {
					auto currentImage = cmdResult->getListIndex(interp, 1);
					showCurrent(currentImage, "disk");
					if (ImGui::MenuItem("eject", nullptr, false, !currentImage.empty())) {
						command.addListElement(driveName, "eject");
					}
					if (ImGui::MenuItem("insert disk image ... TODO")) {
						ImGui::OpenPopup(modalID); // WIP
					}
					ImGui::EndMenu();
					needSeparator = true;
				}
			}
		}
		addSeparator();

		// cartA / extX TODO
		if (ImGui::BeginMenu("carta TODO")) {
			ImGui::Text("currently inserted: TODO");
			ImGui::MenuItem("eject");
			ImGui::MenuItem("insert extension");
			if (ImGui::BeginMenu("insert ROM cartridge")) {
				ImGui::MenuItem("select ROM file");
				ImGui::MenuItem("select ROM type:");
				ImGui::MenuItem("patch files:");
				ImGui::EndMenu();
			}
			ImGui::EndMenu();
			needSeparator = true;
		}
		addSeparator();

		// cassetteplayer
		if (auto cmdResult = execute(TclObject("cassetteplayer"))) {
			if (ImGui::BeginMenu("cassetteplayer")) {
				auto currentImage = cmdResult->getListIndex(interp, 1);
				showCurrent(currentImage, "cassette");
				if (ImGui::MenuItem("eject", nullptr, false, !currentImage.empty())) {
					command.addListElement("cassetteplayer", "eject");
				}
				ImGui::MenuItem("insert cassette image ... TODO");
				ImGui::EndMenu();
				needSeparator = true;
			}
		}
		addSeparator();

		// laserdisc
		if (auto cmdResult = execute(TclObject("laserdiscplayer"))) {
			if (ImGui::BeginMenu("laserdisc")) {
				auto currentImage = cmdResult->getListIndex(interp, 1);
				showCurrent(currentImage, "laserdisc");
				if (ImGui::MenuItem("eject", nullptr, false, !currentImage.empty())) {
					command.addListElement("laserdiscplayer", "eject");
				}
				ImGui::MenuItem("insert laserdisc image ... TODO");
				ImGui::EndMenu();
				needSeparator = true;
			}
		}
	}
	ImGui::EndMenu();

	execute(command);
}

void ImGuiLayer::connectorsMenu(MSXMotherBoard* motherBoard)
{
	if (!ImGui::BeginMenu("Connectors", motherBoard != nullptr)) {
		return;
	}
	TclObject command;
	if (motherBoard) {
		const auto& pluggingController = motherBoard->getPluggingController();
		const auto& pluggables = pluggingController.getPluggables();
		for (auto* connector : pluggingController.getConnectors()) {
			const auto& connectorName = connector->getName();
			auto connectorClass = connector->getClass();
			const auto& currentPluggable = connector->getPlugged();
			if (ImGui::BeginCombo(connectorName.c_str(), std::string(currentPluggable.getName()).c_str())) {
				if (!currentPluggable.getName().empty()) {
					if (ImGui::Selectable("[unplug]")) {
						command.addListElement("unplug", connectorName);
					}
				}
				for (auto& plug : pluggables) {
					if (plug->getClass() != connectorClass) continue;
					auto plugName = std::string(plug->getName());
					bool selected = plug.get() == &currentPluggable;
					int flags = !selected && plug->getConnector() ? ImGuiSelectableFlags_Disabled : 0; // plugged in another connector
					if (ImGui::Selectable(plugName.c_str(), selected, flags)) {
						command.addListElement("plug", connectorName, plugName);
					}
					simpleToolTip(std::string(plug->getDescription()).c_str());
				}
				ImGui::EndCombo();
			}
		}
	}
	ImGui::EndMenu();
	// only execute the command after the full menu is drawn
	execute(command);
}

void ImGuiLayer::debuggableMenu(MSXMotherBoard* motherBoard)
{
	if (!ImGui::BeginMenu("Debuggables", motherBoard != nullptr)) {
		return;
	}
	if (motherBoard) {
		auto& debugger = motherBoard->getDebugger();
		// TODO sort debuggable names, either via sort here, or by
		//    storing the debuggable always in sorted order in Debugger
		for (auto& name : view::keys(debugger.getDebuggables())) {
			auto [it, inserted] = debuggables.try_emplace(name);
			auto& editor = it->second;
			if (inserted) editor = std::make_unique<DebuggableEditor>();
			ImGui::MenuItem(name.c_str(), nullptr, &editor->Open);
		}
	}
	ImGui::EndMenu();
}

void ImGuiLayer::paint(OutputSurface& /*surface*/)
{
	// Start the Dear ImGui frame
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplSDL2_NewFrame();
	ImGui::NewFrame();

	auto& rendererSettings = reactor.getDisplay().getRenderSettings();
	auto& commandController = reactor.getCommandController();
	auto* motherBoard = reactor.getMotherBoard();

	if (show_demo_window) {
		ImGui::ShowDemoWindow(&show_demo_window);
	}

	// Show the enabled 'debuggables'
	if (motherBoard) {
		auto& debugger = motherBoard->getDebugger();
		for (const auto& [name, editor] : debuggables) {
			if (editor->Open) {
				if (auto* debuggable = debugger.findDebuggable(name)) {
					editor->DrawWindow(name.c_str(), *debuggable);
				}
			}
		}
	}

	modalID = ImGui::GetID("openfile"); // WIP

	ImGui::Begin("OpenMSX ImGui integration proof of concept", nullptr, ImGuiWindowFlags_MenuBar);

	if (ImGui::BeginMenuBar()) {
		mediaMenu(motherBoard);
		connectorsMenu(motherBoard);
		debuggableMenu(motherBoard);
		ImGui::EndMenuBar();
	}

	ImGui::Checkbox("ImGui Demo Window", &show_demo_window);
	HelpMarker("Show the ImGui demo window.\n"
	           "This is purely to demonstrate the ImGui capabilities.\n"
	           "There is no connection with any openMSX functionality.");

	if (ImGui::Button("Reset MSX")) {
		commandController.executeCommand("reset");
	}
	HelpMarker("Reset the emulated MSX machine.");

	SliderFloat("noise", rendererSettings.getNoiseSetting(), "%.1f");
	HelpMarker("Stupid example. But good enough to demonstrate that this stuff is very easy.");

	ImGui::End();

	if (ImGui::BeginPopupModal("openfile", nullptr, 0)) { // WIP
		// WIP: the plan is to use something like:
		//  https://github.com/samhocevar/portable-file-dialogs
		ImGui::EndPopup();
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
