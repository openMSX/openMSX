#ifndef IMGUI_REVERSE_BAR_HH
#define IMGUI_REVERSE_BAR_HH

#include "ImGuiPart.hh"

#include "GLUtil.hh"

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
	bool showReverseBar;
private:
	ImGuiManager& manager;
	bool reverseHideTitle;
	bool reverseFadeOut;
	bool reverseAllowMove;
	float reverseAlpha = 1.0f;

	struct PreviewImage {
		std::string name;
		gl::Texture texture{gl::Null{}};
	} previewImage;
};

} // namespace openmsx

#endif
