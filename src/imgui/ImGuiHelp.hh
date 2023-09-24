#ifndef IMGUI_HELP_HH
#define IMGUI_HELP_HH

#include "ImGuiMarkdown.hh"
#include "ImGuiPart.hh"
#include "GLUtil.hh"

namespace openmsx {

class ImGuiHelp final : public ImGuiPart
{
public:
	ImGuiHelp(ImGuiManager& manager)
		: markdown(manager) {}

	void showMenu(MSXMotherBoard* motherBoard) override;
	void paint(MSXMotherBoard* motherBoard) override;

private:
	void paintHelp();
	void paintAbout();

private:
	ImGuiMarkdown markdown;
	bool showHelpWindow = false;
	bool showAboutWindow = false;
	gl::Texture logoImageTexture{gl::Null{}};

};

} // namespace openmsx

#endif
