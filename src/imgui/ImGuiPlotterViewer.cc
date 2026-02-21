#include "ImGuiPlotterViewer.hh"

#include "Connector.hh"
#include "ImGuiManager.hh"
#include "ImGuiUtils.hh"
#include "MSXMotherBoard.hh"
#include "Plotter.hh"
#include "Pluggable.hh"
#include "PluggingController.hh"

#include <imgui.h>

using namespace std::literals;

namespace openmsx {

ImGuiPlotterViewer::ImGuiPlotterViewer(ImGuiManager& manager_)
	: ImGuiPart(manager_)
{
}

void ImGuiPlotterViewer::save(ImGuiTextBuffer& buf)
{
  savePersistent(buf, *this, persistentElements);
}

void ImGuiPlotterViewer::loadLine(std::string_view name, zstring_view value)
{
	loadOnePersistent(name, value, *this, persistentElements);
}

void ImGuiPlotterViewer::paint(MSXMotherBoard* motherBoard)
{
	if (!show) return;

	im::Window("Plotter viewer", &show, [&]{
		if (!motherBoard) {
			ImGui::TextUnformatted("No machine present.");
			return;
		}

		// Find MSXPlotter
		MSXPlotter* plotter = nullptr;
		const auto& controller = motherBoard->getPluggingController();
		for (auto* connector : controller.getConnectors()) {
			if (auto* pluggable = &connector->getPlugged()) {
				if (auto* p = dynamic_cast<MSXPlotter*>(pluggable)) {
					plotter = p;
					break;
				}
			}
		}

		if (!plotter) {
			ImGui::TextUnformatted("Plotter not plugged in.");
			return;
		}

		auto* paper = plotter->getPaper();
		if (!paper) {
			ImGui::TextUnformatted("Plotter has no paper.");
			return;
		}

		// Calculate available space and aspect ratio
		const auto& style = ImGui::GetStyle();
		auto panelHeight = 2.0f * ImGui::GetTextLineHeight() + 4.0f * style.ItemSpacing.y + 2.0f * style.FramePadding.y + 2.0f;
		auto availableSize = ImGui::GetContentRegionAvail() - gl::vec2{0.0f, panelHeight};

		auto paperSize = gl::vec2(paper->size());
		auto scaleVec = availableSize / paperSize;
		float scale = min_component(scaleVec);
		auto displaySize = paperSize * scale; // largest fit with same aspect ratio

		auto screenPos = ImGui::GetCursorScreenPos();
		auto& texture = paper->updateTexture(trunc(displaySize));
		ImGui::Image(texture.getImGui(), displaySize);

		// Draw crosshair at pen position
		// MSXPlotter: X-axis is short axis (carriage), Y-axis is long axis (paper feed).
		// Origin (0,0) is bottom-left of plotting area.
		auto plotPos = plotter->getPlotterPos();
		auto paperPos = gl::vec2{MSXPlotter::MARGIN.x, MSXPlotter::FULL_AREA.y - MSXPlotter::MARGIN.y}
		              + plotPos * gl::vec2{1, -1};
		auto stepToPixel = paperSize / MSXPlotter::FULL_AREA;
		auto penScrnPos = screenPos + (paperPos * stepToPixel * scale);

		auto* drawList = ImGui::GetWindowDrawList();
		float crossSize = 10.0f;
		drawList->AddLine(penScrnPos + gl::vec2{-crossSize, 0.0f},
		                  penScrnPos + gl::vec2{ crossSize, 0.0f}, 0xFF0000FF);
		drawList->AddLine(penScrnPos + gl::vec2{0.0f, -crossSize},
		                  penScrnPos + gl::vec2{0.0f,  crossSize}, 0xFF0000FF);

		ImGui::Separator();
		auto pen = plotter->getSelectedPen();
		static constexpr std::array<std::string_view, 4> penNames = {
			"Black", "Blue", "Green", "Red"
		};
		ImGui::StrCat("Pen: ", penNames[pen]);
		ImGui::SameLine();
		auto c = gl::vec4{gl::vec3(1.0f) - MSXPlotter::inkColors[pen], 1.0f};
		ImGui::ColorButton("##pencolor", c, ImGuiColorEditFlags_NoTooltip,
		                   gl::vec2{ImGui::GetFontSize()});
		ImGui::SameLine();
		ImGui::Text("  Pos: (%.1f, %.1f)", plotPos.x, plotPos.y);
		ImGui::SameLine();
		ImGui::StrCat("  Mode: ", plotter->isGraphicMode() ? "Graphic"sv : "Text"sv);

		ImGui::Separator();
		if (ImGui::Button("Cycle Pen")) {
			plotter->cyclePen();
		}
		ImGui::SameLine();
		if (ImGui::Button("Up")) {
			plotter->moveStep({0.0f, 10.0f});
		}
		ImGui::SameLine();
		if (ImGui::Button("Down")) {
			plotter->moveStep({0.0f, -10.0f});
		}
		ImGui::SameLine();
		if (ImGui::Button("Eject Paper")) {
			plotter->ejectPaper();
		}
	});
}

} // namespace openmsx
