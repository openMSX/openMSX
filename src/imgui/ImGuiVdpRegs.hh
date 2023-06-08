#ifndef IMGUI_VDP_REGS_HH
#define IMGUI_VDP_REGS_HH

#include "ImGuiPart.hh"

#include "EmuTime.hh"

#include <cstdint>
#include <span>

namespace openmsx {

class ImGuiManager;
class VDP;

class ImGuiVdpRegs final : public ImGuiPart
{
public:
	ImGuiVdpRegs(ImGuiManager& manager);

	[[nodiscard]] zstring_view iniName() const override { return "vdp registers"; }
	void save(ImGuiTextBuffer& buf) override;
	void loadLine(std::string_view name, zstring_view value) override;
	void paint(MSXMotherBoard* motherBoard) override;

private:
	void drawSection(std::span<const uint8_t> showRegisters, std::span<const uint8_t> regValues,
	                 VDP& vdp, EmuTime::param time);

public:
	bool show = false;

private:
	//ImGuiManager& manager;
	int hoveredFunction = -1;
	int newHoveredFunction = -1;
	bool explanation = true;
};

} // namespace openmsx

#endif
