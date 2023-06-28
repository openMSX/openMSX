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
	bool openControl = false;
	bool openMode = false;
	bool openBase = false;
	bool openColor = false;
	bool openDisplay = false;
	bool openAccess = false;
	bool openV9958 = false;
	bool openCommand = false;
	bool openStatus = false;
};

} // namespace openmsx

#endif
