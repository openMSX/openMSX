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

// Kept all implementations to be able to compare them more easily.
// In the end we'll only keep one of them.
#if 0

void ImGuiConnector::showMenu(MSXMotherBoard* motherBoard)
{
	im::Menu("Connectors", motherBoard != nullptr, [&]{
		const auto& pluggingController = motherBoard->getPluggingController();
		const auto& pluggables = pluggingController.getPluggables();
		for (auto* connector : pluggingController.getConnectors()) {
			const auto& connectorName = connector->getName();
			auto connectorClass = connector->getClass();
			const auto& currentPluggable = connector->getPlugged();
			im::Combo(connectorName.c_str(), std::string(currentPluggable.getName()).c_str(), [&]{
				if (!currentPluggable.getName().empty()) {
					if (ImGui::Selectable("[unplug]")) {
						manager.executeDelayed(makeTclList("unplug", connectorName));
					}
				}
				for (auto& plug : pluggables) {
					if (plug->getClass() != connectorClass) continue;
					auto plugName = std::string(plug->getName());
					bool selected = plug.get() == &currentPluggable;
					int flags = !selected && plug->getConnector() ? ImGuiSelectableFlags_Disabled : 0; // plugged in another connector
					if (ImGui::Selectable(plugName.c_str(), selected, flags)) {
						manager.executeDelayed(makeTclList("plug", connectorName, plugName));
					}
					simpleToolTip(plug->getDescription());
				}
			});
			simpleToolTip(connector->getDescription());
		}
	});
}

#elif 1

// [Wouter]: This is my personal preference. But the discussion is ongoing.
void ImGuiConnector::showMenu(MSXMotherBoard* motherBoard)
{
	im::Menu("Connectors", motherBoard != nullptr, [&]{
		im::Table("table", 2, [&]{
			const auto& pluggingController = motherBoard->getPluggingController();
			const auto& pluggables = pluggingController.getPluggables();
			for (auto* connector : pluggingController.getConnectors()) {
				const auto& connectorName = connector->getName();
				if (ImGui::TableNextColumn()) { // connector
					ImGui::TextUnformatted(connectorName);
					simpleToolTip(connector->getDescription());
				}
				if (ImGui::TableNextColumn()) { // pluggable
					auto connectorClass = connector->getClass();
					const auto& currentPluggable = connector->getPlugged();
					ImGui::SetNextItemWidth(150.0f);
					im::Combo(tmpStrCat("##", connectorName).c_str(), std::string(currentPluggable.getName()).c_str(), [&]{
						if (!currentPluggable.getName().empty()) {
							if (ImGui::Selectable("[unplug]")) {
								manager.executeDelayed(makeTclList("unplug", connectorName));
							}
						}
						for (auto& plug : pluggables) {
							if (plug->getClass() != connectorClass) continue;
							auto plugName = std::string(plug->getName());
							bool selected = plug.get() == &currentPluggable;
							int flags = !selected && plug->getConnector() ? ImGuiSelectableFlags_Disabled : 0; // plugged in another connector
							if (ImGui::Selectable(plugName.c_str(), selected, flags)) {
								manager.executeDelayed(makeTclList("plug", connectorName, plugName));
							}
							simpleToolTip(plug->getDescription());
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

#else

// This version was requested by Manuel:
// * For the connector it shows the 'description' instead of the 'name' of the connector.
// ** I removed the tooltip for the connector
// * For the pluggable it shows '<name>: <description>' instead of only '<name>' (this is needed because <description> is not unique.
// ** Here I kept the tooltip (which shows the exact same info) to improve readability (IMHO). But feel free to experiment.
//
// [Wouter] Personally I don't like this version:
// * The change for the connector part is acceptable (though inconsistent).
// * I very much dislike the change for the pluggable part:
// ** Keep in mind that the GUI should remain usable with scale_factor=2.
// ** When the drop-down widget is not open, it doesn't show much more info than before (extra info is mostly truncated). We could
//    make the default size wider (but not much), but the majority of the info will remain truncated.
// ** When the drop-down widget is open, the lines are _very_ wide. At lower scale factors even going outside the openMSX window.
// ** IMHO a tooltip is a better solution to create a nice looking menu with the same information content.
void ImGuiConnector::showMenu(MSXMotherBoard* motherBoard)
{
	im::Menu("Connectors", motherBoard != nullptr, [&]{
		im::Table("table", 2, [&]{
			const auto& pluggingController = motherBoard->getPluggingController();
			const auto& pluggables = pluggingController.getPluggables();
			for (auto* connector : pluggingController.getConnectors()) {
				const auto& connectorName = connector->getName();
				if (ImGui::TableNextColumn()) { // connector
					ImGui::TextUnformatted(connector->getDescription());
					//simpleToolTip(connector->getDescription());
				}
				if (ImGui::TableNextColumn()) { // pluggable
					auto connectorClass = connector->getClass();
					auto displayName = [](const Pluggable& p) {
						const auto& n = p.getName();
						return n.empty() ? "" : strCat(p.getName(), ": ", p.getDescription());
					};
					const auto& currentPluggable = connector->getPlugged();
					ImGui::SetNextItemWidth(150.0f);
					im::Combo(tmpStrCat("##", connectorName).c_str(), displayName(currentPluggable).c_str(), [&]{
						if (!currentPluggable.getName().empty()) {
							if (ImGui::Selectable("[unplug]")) {
								manager.executeDelayed(makeTclList("unplug", connectorName));
							}
						}
						for (auto& plug : pluggables) {
							if (plug->getClass() != connectorClass) continue;
							auto plugName = std::string(plug->getName());
							bool selected = plug.get() == &currentPluggable;
							int flags = !selected && plug->getConnector() ? ImGuiSelectableFlags_Disabled : 0; // plugged in another connector
							if (ImGui::Selectable(displayName(*plug).c_str(), selected, flags)) {
								manager.executeDelayed(makeTclList("plug", connectorName, plugName));
							}
							simpleToolTip(plug->getDescription());
						}
					});
				}
			}
		});
	});
}

#endif

} // namespace openmsx
