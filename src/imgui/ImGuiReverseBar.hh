#ifndef IMGUI_REVERSE_BAR_HH
#define IMGUI_REVERSE_BAR_HH

#include "FileListWidget.hh"
#include "ImGuiAdjust.hh"
#include "ImGuiPart.hh"
#include "ImGuiUtils.hh"

#include "GLUtil.hh"

#include <string>

namespace openmsx {

class ImGuiReverseBar final : public ImGuiPart
{
public:
	explicit ImGuiReverseBar(ImGuiManager& manager);

	[[nodiscard]] zstring_view iniName() const override { return "reverse bar"; }
	void save(ImGuiTextBuffer& buf) override;
	void loadLine(std::string_view name, zstring_view value) override;
	void showMenu(MSXMotherBoard* motherBoard) override;
	void paint(MSXMotherBoard* motherBoard) override;

public:
	bool showReverseBar = true;

private:
	std::string saveStateName;
	std::string saveReplayName;
	FileListWidget saveStateFileList;
	FileListWidget replayFileList;

	bool saveStateOpen = false;
	bool saveReplayOpen = false;

	ConfirmDialogTclCommand confirmDialog;

	bool reverseHideTitle = true;
	bool reverseFadeOut = true;
	bool reverseAllowMove = false;
	float reverseAlpha = 1.0f;

	struct PreviewImage {
		std::string name;
		gl::Texture texture{gl::Null{}};
	} previewImage;

	AdjustWindowInMainViewPort adjust;

	static constexpr auto persistentElements = std::tuple{
		PersistentElement{"show",      &ImGuiReverseBar::showReverseBar},
		PersistentElement{"hideTitle", &ImGuiReverseBar::reverseHideTitle},
		PersistentElement{"fadeOut",   &ImGuiReverseBar::reverseFadeOut},
		PersistentElement{"allowMove", &ImGuiReverseBar::reverseAllowMove}
		// manually handle "adjust"
	};
};

} // namespace openmsx

#endif
