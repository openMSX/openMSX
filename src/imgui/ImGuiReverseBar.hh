#ifndef IMGUI_REVERSE_BAR_HH
#define IMGUI_REVERSE_BAR_HH

#include "ImGuiReadHandler.hh"

#include "GLUtil.hh"

#include <string>

struct ImGuiTextBuffer;

namespace openmsx {

class ImGuiManager;
class MSXMotherBoard;

class ImGuiReverseBar : public ImGuiReadHandler
{
public:
	ImGuiReverseBar(ImGuiManager& manager);
	void save(ImGuiTextBuffer& buf);
	void loadLine(std::string_view name, zstring_view value) override;
	void showMenu(MSXMotherBoard* motherBoard);
	void paint(MSXMotherBoard& motherBoard);

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
