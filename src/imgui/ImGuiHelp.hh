#ifndef IMGUI_HELP_HH
#define IMGUI_HELP_HH

#include "ImGuiMarkdown.hh"
#include "ImGuiPart.hh"

#include "GLUtil.hh"

#include <optional>

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

	struct LogoImage {
		LogoImage() = default;

		gl::Texture texture{gl::Null{}};
		gl::vec2 size;
	};
	std::optional<LogoImage> logo; // initialized on first use
};

} // namespace openmsx

#endif
