#include "ImGuiConnector.hh"

#include "ImGuiCpp.hh"
#include "ImGuiManager.hh"
#include "ImGuiUtils.hh"

#include "Connector.hh"
#include "MSXMotherBoard.hh"
#include "Pluggable.hh"
#include "PluggingController.hh"

#include <imgui.h>

namespace openmsx {

[[nodiscard]] static std::string pluggableToGuiString(std::string_view pluggable)
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
	if (pluggable == "tetris2-protection") return "Tetris II SE dongle";
	if (pluggable == "circuit-designer-rd-dongle") return "Circuit Designer RD dongle";
	return std::string(pluggable);
}

void ImGuiConnector::showMenu(MSXMotherBoard* motherBoard)
{
	im::Menu("Connectors", motherBoard != nullptr, [&]{
		im::Table("table", 2, [&]{
			const auto& pluggingController = motherBoard->getPluggingController();
			const auto& pluggables = pluggingController.getPluggables();
			for (const auto* connector : pluggingController.getConnectors()) {
				if (ImGui::TableNextColumn()) { // connector
					ImGui::TextUnformatted(connector->getDescription());
				}
				if (ImGui::TableNextColumn()) { // pluggable
					const auto& connectorName = connector->getName();
					auto connectorClass = connector->getClass();
					const auto& currentPluggable = connector->getPlugged();
					ImGui::SetNextItemWidth(ImGui::GetFontSize() * 12.0f);
					im::Combo(tmpStrCat("##", connectorName).c_str(), pluggableToGuiString(currentPluggable.getName()).c_str(), [&]{
						if (!currentPluggable.getName().empty()) {
							if (ImGui::Selectable("[unplug]")) {
								manager.executeDelayed(makeTclList("unplug", connectorName));
							}
						}
						for (auto& plug : pluggables) {
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
					});
					if (const auto& desc = currentPluggable.getDescription(); !desc.empty()) {
						simpleToolTip(desc);
					}
				}
			}
		});
	});
}

} // namespace openmsx
