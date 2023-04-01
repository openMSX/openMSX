#ifndef IMGUI_HELP_HH
#define IMGUI_HELP_HH

#include "ImGuiMarkdown.hh"
#include "ImGuiPart.hh"

namespace openmsx {

class ImGuiHelp final : public ImGuiPart
{
public:
	ImGuiHelp(ImGuiManager& manager)
		: markdown(manager) {}

	void showMenu(MSXMotherBoard* motherBoard) override;
	void paint(MSXMotherBoard* motherBoard) override;

private:
	ImGuiMarkdown markdown;
	bool showHelpWindow = false;
};

} // namespace openmsx

#endif
