#include "ImGuiConnector.hh"

#include "ImGuiCpp.hh"
#include "ImGuiManager.hh"
#include "ImGuiUtils.hh"

#include "Connector.hh"
#include "MSXMotherBoard.hh"
#include "Pluggable.hh"
#include "PluggingController.hh"

#include <imgui.h>

using namespace std::literals;

namespace openmsx {

[[nodiscard]] static zstring_view pluggableToGuiString(zstring_view pluggable)
{
	if (pluggable == "msxjoystick1")       return "MSX joystick 1";
	if (pluggable == "msxjoystick2")       return "MSX joystick 2";
	if (pluggable == "joymega1")           return "JoyMega controller 1";
	if (pluggable == "joymega2")           return "JoyMega controller 2";
	if (pluggable == "arkanoidpad")        return "Arkanoid Vaus paddle";
	if (pluggable == "ninjatap")           return "Ninja Tap";
	if (pluggable == "cassetteplayer")     return "Tape deck";
	if (pluggable == "simpl")              return "SIMPL";
	if (pluggable == "msx-printer")        return "MSX printer";
	if (pluggable == "epson-printer")      return "Epson printer";
	if (pluggable == "MSXPlotter")         return "MSX plotter";
	if (pluggable == "tetris2-protection") return "Tetris II SE dongle";
	if (pluggable == "circuit-designer-rd-dongle") return "Circuit Designer RD dongle";
	return pluggable;
}

void ImGuiConnector::showMenu(MSXMotherBoard* motherBoard)
{
	im::Menu("Connectors", motherBoard != nullptr, [&]{
		showPluggables(motherBoard->getPluggingController(), Mode::COMBO);
	});
}

void ImGuiConnector::paintPluggableSelectables(PluggingController& controller, const Connector& connector)
{
	const auto& currentPluggable = connector.getPlugged();
	const auto& connectorName = connector.getName();
	if (!currentPluggable.getName().empty()) {
		if (ImGui::Selectable("[unplug]")) {
			manager.executeDelayed(makeTclList("unplug", connectorName));
		}
	}
	auto connectorClass = connector.getClass();
	for (const auto& plug : controller.getPluggables()) {
		if (plug->getClass() != connectorClass) continue;
		auto plugName = plug->getName();
		bool selected = plug.get() == &currentPluggable;
		int flags = !selected && plug->getConnector() ? ImGuiSelectableFlags_Disabled : 0; // plugged in another connector
		if (ImGui::Selectable(pluggableToGuiString(plugName).c_str(), selected, flags)) {
			manager.executeDelayed(makeTclList("plug", connectorName, plugName));
		}
		simpleToolTip(plug->getDescription());
		if (selected) {
			ImGui::SetItemDefaultFocus();
		}
	}
}

void ImGuiConnector::showPluggables(PluggingController& controller, Mode mode)
{
	im::Table("##shared-table", 2, [&]{
		ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableSetupColumn("Value");
		for (const auto* connector : controller.getConnectors()) {
			if (ImGui::TableNextColumn()) { // connector
				ImGui::TextUnformatted(connector->getDescription());
			}
			if (ImGui::TableNextColumn()) { // pluggable
				const auto& currentPluggable = connector->getPlugged();
				const auto& plugName = currentPluggable.getName();
				const auto& connectorName = connector->getName();
				using enum ImGuiConnector::Mode;
				switch (mode) {
					case VIEW:
						ImGui::TextUnformatted(plugName.empty() ? "(empty)"sv : pluggableToGuiString(plugName));
						break;
					case COMBO:
						ImGui::SetNextItemWidth(ImGui::GetFontSize() * 12.0f);
						im::Combo(tmpStrCat("##", connectorName).c_str(), pluggableToGuiString(plugName).c_str(), [&]{
							paintPluggableSelectables(controller, *connector);
						});
						break;
					case SUBMENU:
						im::Menu(pluggableToGuiString(plugName.empty() ? tmpStrCat("(empty)", "##", connectorName).c_str() : plugName).c_str(), [&] {
							paintPluggableSelectables(controller, *connector);
						});
						break;
					default: UNREACHABLE;
				}
				if (mode != SUBMENU) {
					simpleToolTip(currentPluggable.getDescription());
				}
			}
		}
	});
}


} // namespace openmsx
