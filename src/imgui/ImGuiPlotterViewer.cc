#include "ImGuiPlotterViewer.hh"
#include "ImGuiManager.hh"
#include "ImGuiUtils.hh"
#include "Plotter.hh"
#include "Paper.hh"
#include "MSXMotherBoard.hh"
#include "PluggingController.hh"
#include "Connector.hh"
#include "Pluggable.hh"
#include <imgui.h>

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
			if (texture) texture.reset();
			return;
		}

		auto* paper = plotter->getPaper();
		if (!paper) {
			ImGui::TextUnformatted("Plotter has no paper.");
			if (texture) texture.reset();
			return;
		}

		unsigned width = paper->getWidth();
		unsigned height = paper->getHeight();
		auto rgbData = paper->getRGBData();

		if (!texture || texture->getWidth() != (int)width || texture->getHeight() != (int)height) {
			texture.emplace(width, height);
			texture->setInterpolation(false);
			texture->setWrapMode(false);
			texture->bind();
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, rgbData.data());
		} else {
			texture->bind();
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, rgbData.data());
		}

		// Calculate available space and aspect ratio
		float availableWidth = ImGui::GetContentRegionAvail().x;
		float availableHeight = ImGui::GetContentRegionAvail().y - ImGui::GetTextLineHeight() * 2.5f;
		float scale = std::min(availableWidth / (float)width, availableHeight / (float)height);
		ImVec2 displaySize = ImVec2(width * scale, height * scale);

		ImVec2 screenPos = ImGui::GetCursorScreenPos();
		ImGui::Image(texture->getImGui(), displaySize);

		// Draw crosshair at pen position
		auto [plotX, plotY] = plotter->getPlotterPos();
		// MSXPlotter: X-axis is short axis (carriage), Y-axis is long axis (paper feed).
		// Origin (0,0) is bottom-left of plotting area.
		// Paper location in plotWithPen:
		// double paperX = MARGIN_X + (x + originX);
		// double paperY = (PAPER_HEIGHT_STEPS - MARGIN_Y) - (y + originY);
		// NOTE: Plotter steps might be slightly different than paper size, but MARGIN_X handles it.
		// Let's use the same logic as plotWithPen:
		double paperX = plotX + 45.0; // MARGIN_X is 45
		double paperY = 1480.0 - 48.0 - plotY; // PAPER_HEIGHT_STEPS = 1480, MARGIN_Y = 48

		double stepToPixelX = (double)width / 1050.0;
		double stepToPixelY = (double)height / 1480.0;
		float penX = (float)(paperX * stepToPixelX * (double)scale);
		float penY = (float)(paperY * stepToPixelY * (double)scale);

		auto* drawList = ImGui::GetWindowDrawList();
		ImVec2 penPos = ImVec2(screenPos.x + penX, screenPos.y + penY);
		float crossSize = 10.0f;
		drawList->AddLine(ImVec2(penPos.x - crossSize, penPos.y), ImVec2(penPos.x + crossSize, penPos.y), 0xFF0000FF);
		drawList->AddLine(ImVec2(penPos.x, penPos.y - crossSize), ImVec2(penPos.x, penPos.y + crossSize), 0xFF0000FF);

		ImGui::Separator();
		unsigned pen = plotter->getSelectedPen();
		static constexpr std::array<const char*, 4> penNames = {"Black", "Blue", "Green", "Red"};
		static constexpr std::array<uint32_t, 4> penColors = {0xFF000000, 0xFFFF0000, 0xFF00FF00, 0xFF0000FF};

		ImGui::Text("Pen: %s", penNames[pen]);
		ImGui::SameLine();
		ImGui::ColorButton("##pencolor", ImGui::ColorConvertU32ToFloat4(penColors[pen]), ImGuiColorEditFlags_NoTooltip);
		ImGui::SameLine();
		ImGui::Text("  Pos: (%.1f, %.1f)", plotX, plotY);

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
