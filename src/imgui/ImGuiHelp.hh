#ifndef IMGUI_HELP_HH
#define IMGUI_HELP_HH

#include "ImGuiPart.hh"

#include "GLUtil.hh"

#include <optional>

namespace openmsx {

// This used to be an inner class of ImGuiHelp, but for some reason that didn't compile with (some version of) clang???
// Moving it outside is a workaround.
struct LogoImage {
	LogoImage() = default;

	gl::Texture texture{gl::Null{}};
	gl::vec2 size;
};

class ImGuiHelp final : public ImGuiPart
{
public:
	using ImGuiPart::ImGuiPart;

	void showMenu(MSXMotherBoard* motherBoard) override;
	void paint(MSXMotherBoard* motherBoard) override;

private:
	void paintAbout();

private:
	bool showImGuiUserGuide = false;
	bool showAboutOpenMSX = false;
	bool showAboutImGui = false;
	bool showDemoWindow = false;

	std::optional<LogoImage> logo; // initialized on first use
};

} // namespace openmsx

#endif
