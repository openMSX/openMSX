#include "ImGuiPlotterViewer.hh"
#include "Connector.hh"
#include "ImGuiManager.hh"
#include "ImGuiUtils.hh"
#include "MSXMotherBoard.hh"
#include "Paper.hh"
#include "Plotter.hh"
#include "Pluggable.hh"
#include "PluggingController.hh"
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

void ImGuiPlotterViewer::updateTexture(const Paper* paper)
{
	unsigned width = paper->getWidth();
	unsigned height = paper->getHeight();
	auto currentGeneration = paper->getGeneration();

	if (!texture || texture->getWidth() != int(width) ||
	    texture->getHeight() != int(height) ||
	    currentGeneration != lastGeneration) {
		auto rgbData = paper->getRGBData();
		int scaleX = (int(width) + 2047) / 2048;
		int scaleY = (int(height) + 2047) / 2048;

		int texW = width / scaleX;
		int texH = height / scaleY;

		const uint8_t *dataToUpload = rgbData.data();
		if (scaleX > 1 || scaleY > 1) {
			downsampledBuf.resize(size_t(texW) * texH * 3);
			for (int y = 0; y < texH; ++y) {
				for (int x = 0; x < texW; ++x) {
					unsigned r = 0, g = 0, b = 0;
					for (int sy = 0; sy < scaleY; ++sy) {
						for (int sx = 0; sx < scaleX; ++sx) {
							size_t srcIdx = (size_t(y * scaleY + sy) * width + (x * scaleX + sx)) * 3;
							r += rgbData[srcIdx + 0];
							g += rgbData[srcIdx + 1];
							b += rgbData[srcIdx + 2];
						}
					}
					size_t dstIdx = (size_t(y) * texW + x) * 3;
					int area = scaleX * scaleY;
					downsampledBuf[dstIdx + 0] = uint8_t(r / area);
					downsampledBuf[dstIdx + 1] = uint8_t(g / area);
					downsampledBuf[dstIdx + 2] = uint8_t(b / area);
				}
			}
			dataToUpload = downsampledBuf.data();
		}

		if (!texture || texture->getWidth() != texW || texture->getHeight() != texH) {
			texture.emplace(texW, texH);
			texture->setInterpolation(false);
			texture->setWrapMode(false);
			texture->bind();
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, texW, texH, 0, GL_RGB,
			             GL_UNSIGNED_BYTE, dataToUpload);
		} else {
			texture->bind();
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, texW, texH, GL_RGB,
			                GL_UNSIGNED_BYTE, dataToUpload);
		}
		lastGeneration = currentGeneration;
	}
}

void ImGuiPlotterViewer::paint(MSXMotherBoard *motherBoard)
{
	if (!show) return;

	im::Window("Plotter viewer", &show, [&]{
		if (!motherBoard) {
			ImGui::TextUnformatted("No machine present.");
			return;
		}

		// Find MSXPlotter
		MSXPlotter *plotter = nullptr;
		const auto &controller = motherBoard->getPluggingController();
		for (auto *connector : controller.getConnectors()) {
			if (auto *pluggable = &connector->getPlugged()) {
				if (auto *p = dynamic_cast<MSXPlotter *>(pluggable)) {
					plotter = p;
					break;
				}
			}
		}

		if (!plotter) {
			ImGui::TextUnformatted("Plotter not plugged in.");
			texture.reset();
			return;
		}

		auto* paper = plotter->getPaper();
		if (!paper) {
			ImGui::TextUnformatted("Plotter has no paper.");
			texture.reset();
			return;
		}

		updateTexture(paper);

		// Calculate available space and aspect ratio
		unsigned width = paper->getWidth();
		unsigned height = paper->getHeight();
		auto paperSize = paper->getSize();
		auto availableSize = gl::vec2(ImGui::GetContentRegionAvail()) -
		                     gl::vec2{0.0f, 2.5f * ImGui::GetTextLineHeight()};
		auto scaleVec = availableSize / paperSize;
		float scale = min_component(scaleVec);
		ImVec2 displaySize = paperSize * scale;

		ImVec2 screenPos = ImGui::GetCursorScreenPos();
		ImGui::Image(texture->getImGui(), displaySize);

		// Draw crosshair at pen position
		auto [plotX, plotY] = plotter->getPlotterPos();
		// MSXPlotter: X-axis is short axis (carriage), Y-axis is long axis (paper
		// feed). Origin (0,0) is bottom-left of plotting area. Paper location in
		// plotWithPen: double paperX = MARGIN_X + (x + originX); double paperY =
		// (PAPER_HEIGHT_STEPS - MARGIN_Y) - (y + originY); NOTE: Plotter steps
		// might be slightly different than paper size, but MARGIN_X handles it.
		// Let's use the same logic as plotWithPen:
		double paperX = double(plotX) + double(MSXPlotter::MARGIN_X);
		double paperY = double(MSXPlotter::PAPER_HEIGHT_STEPS) -
		                double(MSXPlotter::MARGIN_Y) - double(plotY);

		double stepToPixelX = double(width)  / double(MSXPlotter::PAPER_WIDTH_STEPS);
		double stepToPixelY = double(height) / double(MSXPlotter::PAPER_HEIGHT_STEPS);
		auto penX = float(paperX * stepToPixelX * double(scale));
		auto penY = float(paperY * stepToPixelY * double(scale));

		auto *drawList = ImGui::GetWindowDrawList();
		ImVec2 penPos = ImVec2(screenPos.x + penX, screenPos.y + penY);
		float crossSize = 10.0f;
		drawList->AddLine(ImVec2(penPos.x - crossSize, penPos.y),
		                  ImVec2(penPos.x + crossSize, penPos.y), 0xFF0000FF);
		drawList->AddLine(ImVec2(penPos.x, penPos.y - crossSize),
		                  ImVec2(penPos.x, penPos.y + crossSize), 0xFF0000FF);

		ImGui::Separator();
		unsigned pen = plotter->getSelectedPen();
		static constexpr std::array<const char *, 4> penNames = {
			"Black", "Blue", "Green", "Red"
		};
		ImGui::Text("Pen: %s", penNames[pen]);
		ImGui::SameLine();
		const auto &c = MSXPlotter::penColors[pen];
		float sz = ImGui::GetFontSize();
		ImGui::ColorButton("##pencolor",
		                   ImVec4(float(c[0]) / 255.0f, float(c[1]) / 255.0f,
		                          float(c[2]) / 255.0f, 1.0f),
		                   ImGuiColorEditFlags_NoTooltip, {sz, sz});
		ImGui::SameLine();
		ImGui::Text("  Pos: (%.1f, %.1f)", double(plotX), double(plotY));
		ImGui::SameLine();
		ImGui::Text("  Mode: %s", plotter->isGraphicMode() ? "Graphic" : "Text");

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
