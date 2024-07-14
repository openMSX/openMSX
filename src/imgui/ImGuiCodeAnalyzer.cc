#include "ImGuiCodeAnalyzer.hh"

namespace openmsx {

ImGuiCodeAnalyzer::ImGuiCodeAnalyzer(ImGuiManager& manager_)
	: ImGuiPart(manager_)
	, symbolManager(manager.getReactor().getSymbolManager())
{
}

ImGuiCodeAnalyzer::~ImGuiCodeAnalyzer() = default;

void ImGuiCodeAnalyzer::paint(MSXMotherBoard* motherBoard)
{
        if (!motherBoard) return;
}

void ImGuiCodeAnalyzer::drawSource()
{
	if (!showSource) return;
	ImGui::SetNextWindowSize({340, 540}, ImGuiCond_FirstUseEver);
	im::Window("Code Analyzer", &showSource, [&]{
		if (ImGui::Button("From Cart A")) {
		}
		ImGui::SameLine();
		if (ImGui::Button("From Cart B")) {
		}
		im::Child("#code", [&]{
			im::TreeNode("Settings", ImGuiTreeNodeFlags_DefaultOpen, [&]{
			});
		});
	});
}

} // namespace openmsx
