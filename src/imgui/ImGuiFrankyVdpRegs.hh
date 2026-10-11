#ifndef IMGUI_FRANKY_VDPREGS_HH
#define IMGUI_FRANKY_VDPREGS_HH

#include "ImGuiPart.hh"

#include "EmuTime.hh"

#include <cstdint>
#include <span>
#include <string>

namespace openmsx {

class SMSVDP;

/** Register viewer for the Sega VDP inside the Franky cartridge.
  *
  * This is the Franky counterpart of ImGuiVdpRegs: it shows the SMS VDP
  * registers R#0-R#10 and the status register S#0 with per-bit names, an
  * editable hex field and a live explanation of the derived VDP state.
  */
class ImGuiFrankyVdpRegs final : public ImGuiPart
{
public:
	ImGuiFrankyVdpRegs(ImGuiManager& manager_, size_t index);

	[[nodiscard]] zstring_view iniName() const override { return title; }
	void save(ImGuiTextBuffer& buf) override;
	void loadLine(std::string_view name, zstring_view value) override;
	void paint(MSXMotherBoard* motherBoard) override;

private:
	void drawSection(std::span<const uint8_t> showRegisters,
	                 std::span<const uint8_t> regValues,
	                 SMSVDP& vdp, EmuTime time);

public:
	bool show = true;

private:
	std::string title;

	int hoveredFunction = -1;
	int newHoveredFunction = -1;
	bool explanation = true;
	bool openMode = false;
	bool openBase = false;
	bool openScroll = false;
	bool openColor = false;
	bool openLineCounter = false;
	bool openStatus = false;

	static constexpr auto persistentElements = std::tuple{
		PersistentElement{"show",            &ImGuiFrankyVdpRegs::show},
		PersistentElement{"explanation",     &ImGuiFrankyVdpRegs::explanation},
		PersistentElement{"openMode",        &ImGuiFrankyVdpRegs::openMode},
		PersistentElement{"openBase",        &ImGuiFrankyVdpRegs::openBase},
		PersistentElement{"openScroll",      &ImGuiFrankyVdpRegs::openScroll},
		PersistentElement{"openColor",       &ImGuiFrankyVdpRegs::openColor},
		PersistentElement{"openLineCounter", &ImGuiFrankyVdpRegs::openLineCounter},
		PersistentElement{"openStatus",      &ImGuiFrankyVdpRegs::openStatus}
	};
};

} // namespace openmsx

#endif
