#include "ImGuiPalette.hh"

#include "ImGuiCpp.hh"

#include "VDP.hh"

#include "StringOp.hh"
#include "xrange.hh"

#include <imgui.h>

#include <cassert>

namespace openmsx {

static constexpr std::array<uint16_t, 16> fixedPalette = { // GRB
	0x000, 0x000, 0x611, 0x733,
	0x117, 0x327, 0x151, 0x627,
	0x171, 0x373, 0x661, 0x664,
	0x411, 0x265, 0x555, 0x777,
};

ImGuiPalette::ImGuiPalette()
{
	customPalette = fixedPalette;
}

void ImGuiPalette::save(ImGuiTextBuffer& buf)
{
	buf.appendf("show=%d\n", show);

	buf.append("customPalette=");
	for (auto i : xrange(16)) {
		if (i) buf.append(",");
		buf.appendf("%d", customPalette[i]);
	}
	buf.append("\n");

	buf.appendf("palette=%d\n", whichPalette);
}

void ImGuiPalette::loadLine(std::string_view name, zstring_view value)
{
	if (name == "show") {
		show = StringOp::stringToBool(value);
	} else if (name == "customPalette") {
		int i = 0;
		for (auto s : StringOp::split_view(value, ',')) {
			if (auto v = StringOp::stringTo<uint16_t>(s)) {
				customPalette[i] = *v;
			}
			if (++i == 16) break;
		}
	} else if (name == "palette") {
		if (auto r = StringOp::stringTo<unsigned>(value)) {
			if (*r <= 2) whichPalette = *r;
		}
	}
}

static std::array<int, 3> extractRGB(uint16_t grb)
{
	auto r = (grb >> 4) & 7;
	auto g = (grb >> 8) & 7;
	auto b = (grb >> 0) & 7;
	return {r, g, b};
}

static void insertRGB(std::span<uint16_t, 16> msxPalette, unsigned index, std::array<int, 3> rgb)
{
	assert(index < 16);
	msxPalette[index] = narrow<int16_t>(((rgb[1] & 7) << 8) | ((rgb[0] & 7) << 4) | ((rgb[2] & 7) << 0));
}

uint32_t ImGuiPalette::toRGBA(uint16_t msxColor)
{
	auto [r, g, b] = extractRGB(msxColor);
	return ImColor(float(r) / 7.0f, float(g) / 7.0f, float(b) / 7.0f);
}

static bool coloredButton(const char* id, ImU32 color, ImVec2 size)
{
	bool result = false;
	auto col = ImGui::ColorConvertU32ToFloat4(color);
	im::StyleColor(ImGuiCol_Button,        col,
	               ImGuiCol_ButtonHovered, col,
	               ImGuiCol_ButtonActive,  col, [&]{
		result = ImGui::Button(id, size);
	});
	return result;
}

std::span<const uint16_t, 16> ImGuiPalette::getPalette(VDP* vdp) const
{
	if (whichPalette == PALETTE_CUSTOM) {
		return customPalette;
	} else if (whichPalette == PALETTE_VDP && vdp) {
		return vdp->getPalette();
	} else {
		return fixedPalette;
	}
}

void ImGuiPalette::paint(MSXMotherBoard* motherBoard)
{
	if (!show) return;
	im::Window("Palette editor", &show, [&]{
		VDP* vdp = motherBoard ? dynamic_cast<VDP*>(motherBoard->findDevice("VDP")) : nullptr; // TODO name based OK?

		im::Disabled(vdp == nullptr, [&]{
			ImGui::RadioButton("VDP palette", &whichPalette, PALETTE_VDP);
		});
		ImGui::SameLine();
		ImGui::RadioButton("Custom palette", &whichPalette, PALETTE_CUSTOM);
		ImGui::SameLine();
		ImGui::RadioButton("Fixed palette", &whichPalette, PALETTE_FIXED);

		std::array<uint16_t, 16> paletteCopy;
		std::span<uint16_t, 16> palette = customPalette;
		bool disabled = (whichPalette == PALETTE_FIXED) ||
				((whichPalette == PALETTE_VDP) && !vdp);
		if (disabled) {
			palette = std::span<uint16_t, 16>{const_cast<uint16_t*>(fixedPalette.data()), 16};
		} else if (whichPalette == PALETTE_VDP) {
			ranges::copy(vdp->getPalette(), paletteCopy);
			palette = paletteCopy;
		}

		im::Table("left/right", 2, [&]{
			ImGui::TableSetupColumn("left",  ImGuiTableColumnFlags_WidthFixed, 200.0f);
			ImGui::TableSetupColumn("right", ImGuiTableColumnFlags_WidthFixed, 150.0f);

			ImGui::TableNextColumn(); // left pane
			im::Table("grid", 4, [&]{
				for (auto i : xrange(16)) {
					if (ImGui::TableNextColumn()) {
						im::ID(i, [&]{
							if (i == selectedColor) {
								ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, ImGui::GetColorU32(ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive)));
							}
							auto color = toRGBA(palette[i]);
							if (coloredButton("", color, {44.0f, 30.0f})) {
								selectedColor = i;
							}
						});
					}
				}
			});

			ImGui::TableNextColumn(); // right pane
			ImGui::Text("Color %d", selectedColor);
			ImGui::TextUnformatted(" ");
			ImGui::SameLine();
			coloredButton("##color", toRGBA(palette[selectedColor]), {150.0f, 30.0f});
			ImGui::Spacing();
			ImGui::Spacing();

			im::Disabled(disabled, [&]{
				static constexpr std::array names = {"R", "G", "B"};
				auto rgb = extractRGB(palette[selectedColor]);
				for (auto i : xrange(3)) { // rgb sliders
					im::ID(i, [&]{
						ImGui::AlignTextToFramePadding();
						ImGui::TextUnformatted(names[i]);
						ImGui::SameLine();
						im::StyleColor(ImGuiCol_FrameBg,        static_cast<ImVec4>(ImColor::HSV(float(i) / 3.0f, 0.5f, 0.5f)),
						               ImGuiCol_FrameBgHovered, static_cast<ImVec4>(ImColor::HSV(float(i) / 3.0f, 0.6f, 0.5f)),
						               ImGuiCol_FrameBgActive,  static_cast<ImVec4>(ImColor::HSV(float(i) / 3.0f, 0.7f, 0.5f)),
						               ImGuiCol_SliderGrab,     static_cast<ImVec4>(ImColor::HSV(float(i) / 3.0f, 0.9f, 0.9f)), [&]{
							ImGui::SetNextItemWidth(-FLT_MIN);
							if (ImGui::SliderInt("##v", &rgb[i], 0, 7, "%d", ImGuiSliderFlags_AlwaysClamp)) {
								assert(!disabled);
								insertRGB(palette, selectedColor, rgb);
								if (whichPalette == PALETTE_VDP) {
									auto time = motherBoard->getCurrentTime();
									vdp->setPalette(selectedColor, palette[selectedColor], time);
								}
							}
						});
					});
				}
			});
		});
	});
}

} // namespace openmsx
