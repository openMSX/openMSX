#ifndef IMGUI_V9990_REGS_HH
#define IMGUI_V9990_REGS_HH

#include "ImGuiPart.hh"

#include <string>

namespace openmsx {

/** Register viewer for the V9990 (GFX9000).
  *
  * This is the V9990 counterpart of ImGuiVdpRegs: it shows the control
  * registers R#0-R#54, the status port P#5 and the interrupt flag port P#6
  * with per-bit names, an editable hex field and a live explanation.
  */
class ImGuiV9990Regs final : public ImGuiPart
{
public:
	ImGuiV9990Regs(ImGuiManager& manager_, size_t index);

	[[nodiscard]] zstring_view iniName() const override { return title; }
	void save(ImGuiTextBuffer& buf) override;
	void loadLine(std::string_view name, zstring_view value) override;
	void paint(MSXMotherBoard* motherBoard) override;

public:
	bool show = true;

private:
	std::string title;

	int hoveredFunction = -1;
	bool explanation = true;
	bool openScreen = false;
	bool openControl = false;
	bool openPalette = false;
	bool openScroll = false;
	bool openSprite = false;
	bool openVramAccess = false;
	bool openCommand = false;
	bool openStatus = false;

	static constexpr auto persistentElements = std::tuple{
		PersistentElement{"show",           &ImGuiV9990Regs::show},
		PersistentElement{"explanation",    &ImGuiV9990Regs::explanation},
		PersistentElement{"openScreen",     &ImGuiV9990Regs::openScreen},
		PersistentElement{"openControl",    &ImGuiV9990Regs::openControl},
		PersistentElement{"openPalette",    &ImGuiV9990Regs::openPalette},
		PersistentElement{"openScroll",     &ImGuiV9990Regs::openScroll},
		PersistentElement{"openSprite",     &ImGuiV9990Regs::openSprite},
		PersistentElement{"openVramAccess", &ImGuiV9990Regs::openVramAccess},
		PersistentElement{"openCommand",    &ImGuiV9990Regs::openCommand},
		PersistentElement{"openStatus",     &ImGuiV9990Regs::openStatus}
	};
};

} // namespace openmsx

#endif
