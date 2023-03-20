#ifndef IMGUI_HELP_HH
#define IMGUI_HELP_HH

#include "ImGuiMarkdown.hh"

namespace openmsx {

class ImGuiHelp
{
public:
	ImGuiHelp(ImGuiManager& manager)
		: markdown(manager) {}

	void showMenu();
	void paint();

private:
	ImGuiMarkdown markdown;
	bool showHelpWindow = false;
};

} // namespace openmsx

#endif
