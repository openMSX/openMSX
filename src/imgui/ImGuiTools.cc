#include "ImGuiSettings.hh"

#include "ImGuiCpp.hh"
#include "ImGuiManager.hh"
#include "ImGuiUtils.hh"

#include "ranges.hh"
#include "StringOp.hh"

#include <imgui.h>

#include <string>
#include <vector>

namespace openmsx {

static const std::vector<std::string>& getAllToyScripts(ImGuiManager& manager)
{
	static float refresh = FLT_MAX;
	static std::vector<std::string> result;

	if (refresh > 2.5f) { // only recalculate every so often
		refresh = 0.0f;

		if (auto commands = manager.execute(TclObject("openmsx::all_command_names"))) {
			result.clear();
			for (const auto& cmd : *commands) {
				if (cmd.starts_with("toggle_")) {
					result.emplace_back(cmd.view());
				}
			}
			ranges::sort(result, StringOp::caseless{});
		}
	}
	refresh += ImGui::GetIO().DeltaTime;
	return result;
}

void ImGuiTools::showMenu(MSXMotherBoard* /*motherBoard*/)
{
	im::Menu("Tools", [&]{
		ImGui::MenuItem("Show virtual keyboard", nullptr, &manager.keyboard.show);
		const auto& hotKey = manager.getReactor().getHotKey();
		auto consoleShortCut = getShortCutForCommand(hotKey, "toggle console");
		ImGui::MenuItem("Show console", consoleShortCut.c_str(), &manager.console.show);
		ImGui::MenuItem("Show message log ...", nullptr, &manager.messages.showLog);
		ImGui::Separator();
		ImGui::MenuItem("Trainer Selector ...", nullptr, &manager.trainer.show);
		ImGui::MenuItem("Cheat Finder ...", nullptr, &manager.cheatFinder.show);
		ImGui::Separator();

		im::Menu("Toys", [&]{
			const auto& toys = getAllToyScripts(manager);
			for (const auto& toy : toys) {
				std::string displayText = toy.substr(7);
				ranges::replace(displayText, '_', ' ');
				if (ImGui::MenuItem(displayText.c_str())) {
					manager.executeDelayed(TclObject(toy));
				}
				auto help = manager.execute(TclObject("help " + toy));
				if (help) {
					simpleToolTip(help->getString());
				}
			}
		});
	});
}

} // namespace openmsx
