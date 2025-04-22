#include "ImGuiCheatFinder.hh"

#include "ImGuiCpp.hh"
#include "ImGuiManager.hh"
#include "ImGuiUtils.hh"

#include "narrow.hh"
#include "stl.hh"

#include <ranges>

namespace openmsx {

using namespace std::literals;

void ImGuiCheatFinder::paint(MSXMotherBoard* /*motherBoard*/)
{
	if (!show) return;

	bool start = false;
	std::string searchExpr;

	ImGui::SetNextWindowSize(gl::vec2{35, 0} * ImGui::GetFontSize(), ImGuiCond_FirstUseEver);
	im::Window("Cheat Finder", &show, [&]{
		const auto& style = ImGui::GetStyle();
		auto tSize = ImGui::CalcTextSize("=="sv).x + 2.0f * style.FramePadding.x;
		auto bSpacing = 2.0f;
		auto height = 12.5f * ImGui::GetTextLineHeightWithSpacing();
		auto sWidth = 2.0f * (style.WindowBorderSize + style.WindowPadding.x)
		              + style.IndentSpacing + 6 * tSize + 5 * bSpacing;
		im::Child("search", {sWidth, height}, ImGuiChildFlags_Borders, [&]{
			ImGui::TextUnformatted("Search"sv);
			HelpMarker("OpenMSX cheat finder. See here for a quick tutorial:\n"
			           "  openMSX tutorial: Working with the Cheat Finder\n"
			           "  http://www.youtube.com/watch?v=F11ltfkCtKo\n"
			           "The UI has changed, but the ideas remain the same.");
			im::Disabled(searchResults.empty(), [&]{
				ImGui::TextUnformatted("Compare"sv);
				im::Indent([&]{
					auto bSize = ImVec2{tSize, 0.0f};
					if (ImGui::Button("<",  bSize)) searchExpr = "$new < $old";
					simpleToolTip("Search for memory locations with strictly decreased value");
					ImGui::SameLine(0.0f, bSpacing);
					if (ImGui::Button("<=", bSize)) searchExpr = "$new <= $old";
					simpleToolTip("Search for memory locations with decreased value");
					ImGui::SameLine(0.0f, bSpacing);
					if (ImGui::Button("!=", bSize)) searchExpr = "$new != $old";
					simpleToolTip("Search for memory locations with changed value");
					ImGui::SameLine(0.0f, bSpacing);
					if (ImGui::Button("==", bSize)) searchExpr = "$new == $old";
					simpleToolTip("Search for memory locations with unchanged value");
					ImGui::SameLine(0.0f, bSpacing);
					if (ImGui::Button(">=", bSize)) searchExpr = "$new >= $old";
					simpleToolTip("Search for memory locations with increased value");
					ImGui::SameLine(0.0f, bSpacing);
					if (ImGui::Button(">",  bSize)) searchExpr = "$new > $old";
					simpleToolTip("Search for memory locations with strictly increased value");
				});
				ImGui::TextUnformatted("Specific value"sv);
				im::Indent([&]{
					ImGui::SetNextItemWidth(3 * ImGui::GetFontSize());
					ImGui::InputScalar("##value", ImGuiDataType_U8, &searchValue);
					ImGui::SameLine();
					if (ImGui::Button("Go")) {
						searchExpr = strCat("$new == ", searchValue);
					}
					simpleToolTip("Search for memory locations with a specific value");
				});
			});
			start |= ImGui::Button("Restart search");
		});

		ImGui::SameLine();
		im::Child("result", {0.0f, height}, ImGuiChildFlags_Borders, [&]{
			auto num = searchResults.size();
			if (num == 0) {
				ImGui::TextUnformatted("Results: no remaining locations"sv);
				start |= ImGui::Button("Start a new search");
			} else {
				if (num == 1) {
					ImGui::TextUnformatted("Results: 1 remaining location"sv);
				} else {
					ImGui::Text("Results: %d remaining locations", narrow<int>(num));
				}
				int flags = ImGuiTableFlags_RowBg |
				            ImGuiTableFlags_BordersV |
				            ImGuiTableFlags_BordersOuter |
				            ImGuiTableFlags_ScrollY;
				im::Table("##table", 3, flags, [&]{
					ImGui::TableSetupScrollFreeze(0, 1); // Make top row always visible
					ImGui::TableSetupColumn("Address");
					ImGui::TableSetupColumn("Previous");
					ImGui::TableSetupColumn("New value");
					ImGui::TableHeadersRow();

					im::ListClipper(searchResults.size(), [&](int i) {
						const auto& row = searchResults[i];
						if (ImGui::TableNextColumn()) { // addr
							ImGui::Text("0x%04x", row.address);
						}
						if (ImGui::TableNextColumn()) { // old
							ImGui::Text("%d", row.oldValue);
						}
						if (ImGui::TableNextColumn()) { // new
							ImGui::Text("%d", row.newValue);
						}
					});
				});
			}
		});
	});

	if (start) {
		manager.execute(TclObject("cheat_finder::start"));
		searchExpr = "1"; // could be optimized, but good enough
	}
	if (!searchExpr.empty()) {
		auto result = manager.execute(makeTclList("cheat_finder::search", searchExpr)).value_or(TclObject{});
		searchResults = to_vector(std::views::transform(xrange(result.size()), [&](size_t i) {
			auto line = result.getListIndexUnchecked(i);
			auto addr     = line.getListIndexUnchecked(0).getOptionalInt().value_or(0);
			auto oldValue = line.getListIndexUnchecked(1).getOptionalInt().value_or(0);
			auto newValue = line.getListIndexUnchecked(2).getOptionalInt().value_or(0);
			return SearchResult{.address = narrow_cast<uint16_t>(addr),
			                    .oldValue = narrow_cast<uint8_t>(oldValue),
			                    .newValue = narrow_cast<uint8_t>(newValue)};
		}));
	}
}

} // namespace openmsx
