#ifndef IMGUI_HELP_HH
#define IMGUI_HELP_HH

#include "ImGuiPart.hh"

#include "GLUtil.hh"

#include <optional>

namespace openmsx {

class ImGuiHelp final : public ImGuiPart
{
public:
	explicit ImGuiHelp(ImGuiManager& manager_)
		: ImGuiPart(manager_) {}
	void showMenu(MSXMotherBoard* motherBoard) override;
	void paint(MSXMotherBoard* motherBoard) override;

private:
	void paintAbout();

private:
	bool showImGuiUserGuide = false;
	bool showAboutOpenMSX = false;
	bool showAboutImGui = false;

	struct LogoImage {
		LogoImage() = default;

		gl::Texture texture{gl::Null{}};
		gl::vec2 size;
	};
	std::optional<LogoImage> logo; // initialized on first use
};

} // namespace openmsx

#endif
