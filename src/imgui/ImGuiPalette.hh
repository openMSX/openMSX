#ifndef IMGUI_PALETTE_HH
#define IMGUI_PALETTE_HH

#include "ImGuiPart.hh"

#include <array>
#include <cstdint>

namespace openmsx {

class ImGuiPalette final : public ImGuiPart
{
public:
	ImGuiPalette();

	[[nodiscard]] zstring_view iniName() const override { return "Palette editor"; }
	void save(ImGuiTextBuffer& buf) override;
	void loadLine(std::string_view name, zstring_view value) override;
	void paint(MSXMotherBoard* motherBoard) override;

public:
	bool show = false;

private:
	static constexpr int PALETTE_VDP = 0;
	static constexpr int PALETTE_CUSTOM = 1;
	static constexpr int PALETTE_FIXED = 2;
	int whichPalette = PALETTE_VDP;
	int selectedColor = 0;

	std::array<uint16_t, 16> customPalette; // palette in MSX format: 0GRB nibbles
};

} // namespace openmsx

#endif
