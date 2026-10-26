#ifndef IMGUI_SMS_VDP_REGS_HH
#define IMGUI_SMS_VDP_REGS_HH

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
class ImGuiSMSVDPRegs final : public ImGuiPart
{
public:
	ImGuiSMSVDPRegs(ImGuiManager& manager_, size_t index);

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
		PersistentElement{"show",            &ImGuiSMSVDPRegs::show},
		PersistentElement{"explanation",     &ImGuiSMSVDPRegs::explanation},
		PersistentElement{"openMode",        &ImGuiSMSVDPRegs::openMode},
		PersistentElement{"openBase",        &ImGuiSMSVDPRegs::openBase},
		PersistentElement{"openScroll",      &ImGuiSMSVDPRegs::openScroll},
		PersistentElement{"openColor",       &ImGuiSMSVDPRegs::openColor},
		PersistentElement{"openLineCounter", &ImGuiSMSVDPRegs::openLineCounter},
		PersistentElement{"openStatus",      &ImGuiSMSVDPRegs::openStatus}
	};
};

} // namespace openmsx

#endif
