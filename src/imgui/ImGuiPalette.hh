#ifndef IMGUI_PALETTE_HH
#define IMGUI_PALETTE_HH

#include "ImGuiCpp.hh"
#include "ImGuiPart.hh"

#include <array>
#include <cstdint>
#include <span>

namespace openmsx {

class VDP;

class ImGuiPalette final : public ImGuiPart
{
public:
	explicit ImGuiPalette(ImGuiManager& manager_);

	[[nodiscard]] zstring_view iniName() const override { return "Palette editor"; }
	void save(ImGuiTextBuffer& buf) override;
	void loadLine(std::string_view name, zstring_view value) override;
	void paint(MSXMotherBoard* motherBoard) override;

	std::span<const uint16_t, 16> getPalette(VDP* vdp) const;
	static uint32_t toRGBA(uint16_t msxColor);

public:
	im::WindowStatus window;
	int whichPalette = PALETTE_VDP;

private:
	static constexpr int PALETTE_VDP = 0;
	static constexpr int PALETTE_CUSTOM = 1;
	static constexpr int PALETTE_FIXED = 2;
	int selectedColor = 0;

	std::array<uint16_t, 16> customPalette; // palette in MSX format: 0GRB nibbles

	static constexpr auto persistentElements = std::tuple{
		PersistentElement   {"show",    &ImGuiPalette::window},
		PersistentElementMax{"palette", &ImGuiPalette::whichPalette, 3}
		// manually handle "customPalette"
	};
};

} // namespace openmsx

#endif
