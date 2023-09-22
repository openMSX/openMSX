#ifndef IMGUI_REVERSE_BAR_HH
#define IMGUI_REVERSE_BAR_HH

#include "ImGuiPart.hh"

#include "GLUtil.hh"
#include "TclObject.hh"

#include <string>

namespace openmsx {

class ImGuiManager;
class MSXMotherBoard;

class ImGuiReverseBar final : public ImGuiPart
{
public:
	ImGuiReverseBar(ImGuiManager& manager);

	[[nodiscard]] zstring_view iniName() const override { return "reverse bar"; }
	void save(ImGuiTextBuffer& buf) override;
	void loadLine(std::string_view name, zstring_view value) override;
	void showMenu(MSXMotherBoard* motherBoard) override;
	void paint(MSXMotherBoard* motherBoard) override;

public:
	bool showReverseBar = true;
private:
	ImGuiManager& manager;

	std::string saveStateName;
	std::string saveReplayName;
	bool saveStateOpen = false;
	bool saveReplayOpen = false;
	TclObject overWriteCmd;
	std::string overWriteText;

	bool reverseHideTitle = true;
	bool reverseFadeOut = true;
	bool reverseAllowMove = false;
	float reverseAlpha = 1.0f;

	struct PreviewImage {
		std::string name;
		gl::Texture texture{gl::Null{}};
	} previewImage;

	static constexpr auto persistentElements = std::tuple{
		PersistentElement{"show",      &ImGuiReverseBar::showReverseBar},
		PersistentElement{"hideTitle", &ImGuiReverseBar::reverseHideTitle},
		PersistentElement{"fadeOut",   &ImGuiReverseBar::reverseFadeOut},
		PersistentElement{"allowMove", &ImGuiReverseBar::reverseAllowMove}
	};
};

} // namespace openmsx

#endif
