#include "ImGuiSMSVDPUtils.hh"

#include "SMSVDP.hh"

#include "xrange.hh"

#include <imgui.h>

namespace openmsx {

[[nodiscard]] static uint32_t toImColor(uint32_t rgb)
{
	return ImColor(int((rgb >> 16) & 0xff),
	               int((rgb >>  8) & 0xff),
	               int((rgb >>  0) & 0xff));
}

std::array<uint32_t, 32> getSMSPalette(const SMSVDP& vdp, bool mode4)
{
	// the chip colors: 0-63 for mode 4, 64-79 are the TMS9918 colors
	auto colors = vdp.getPaletteColors();
	std::array<uint32_t, 32> palette{};
	if (mode4) {
		for (auto i : xrange(32)) {
			palette[i] = toImColor(colors[vdp.peekCram(i) & 0x3f]);
		}
	} else {
		for (auto i : xrange(16)) {
			palette[i] = toImColor(colors[64 + i]);
		}
	}
	return palette;
}

void drawSMSCramEditor(SMSVDP& vdp)
{
	auto colors = vdp.getPaletteColors();
	for (auto i : xrange(32)) {
		auto cram = vdp.peekCram(i);
		ImGui::PushID(i);
		ImGui::ColorButton("##cram",
			ImGui::ColorConvertU32ToFloat4(toImColor(colors[cram & 0x3f])),
			ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoPicker, ImVec2(24.0f, 24.0f));
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("CRAM[%d] = 0x%02X (color 0x%02X)", i, cram, cram & 0x3f);
		}
		if (ImGui::IsItemClicked()) {
			ImGui::OpenPopup("##cramColors");
		}
		if (ImGui::BeginPopup("##cramColors")) {
			for (auto row : xrange(8)) {
				for (auto column : xrange(8)) {
					auto c = row * 8 + column;
					ImGui::PushID(c);
					ImGui::ColorButton("##color",
						ImGui::ColorConvertU32ToFloat4(toImColor(colors[c])),
						ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoPicker, ImVec2(24.0f, 24.0f));
					ImGui::PopID();
					if (ImGui::IsItemHovered()) {
						ImGui::SetTooltip("color 0x%02X", c);
					}
					if (ImGui::IsItemClicked()) {
						vdp.pokeCram(i, uint8_t(c));
						ImGui::CloseCurrentPopup();
					}
					if (column != 7) ImGui::SameLine();
				}
			}
			ImGui::EndPopup();
		}
		ImGui::PopID();
		if ((i % 16) != 15) ImGui::SameLine();
	}
}

} // namespace openmsx
