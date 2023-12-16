#ifndef IMGUI_TOOLS_HH
#define IMGUI_TOOLS_HH

#include "ImGuiPart.hh"

#include "TclObject.hh"

namespace openmsx {

class ImGuiManager;

class ImGuiTools final : public ImGuiPart
{
public:
	ImGuiTools(ImGuiManager& manager_)
		: manager(manager_) {}

	[[nodiscard]] zstring_view iniName() const override { return "tools"; }
	void save(ImGuiTextBuffer& buf) override;
	void loadLine(std::string_view name, zstring_view value) override;
	void showMenu(MSXMotherBoard* motherBoard) override;
	void paint(MSXMotherBoard* motherBoard) override;

private:
	void paintScreenshot();
	void paintVideo();
	void paintSound();

	[[nodiscard]] bool screenshotNameExists() const;
	void generateScreenshotName();
	void nextScreenshotName();

private:
	ImGuiManager& manager;
	bool showScreenshot = false;
	bool showRecordVideo = false;
	bool showRecordSound = false;

	std::string screenshotName;
	enum class SsType : int { RENDERED, MSX, NUM };
	int screenshotType = static_cast<int>(SsType::RENDERED);
	enum class SsSize : int { S_320, S_640, NUM };
	int screenshotSize = static_cast<int>(SsSize::S_320);
	bool screenshotWithOsd = false;
	bool screenshotHideSprites = false;

	TclObject confirmCmd;
	std::string confirmText;
	bool openConfirmPopup = false;

	static constexpr auto persistentElements = std::tuple{
		PersistentElement{"showScreenshot",  &ImGuiTools::showScreenshot},
		PersistentElement{"showRecordVideo", &ImGuiTools::showRecordVideo},
		PersistentElement{"showRecordSound", &ImGuiTools::showRecordSound},
		PersistentElementMax{"screenshotType", &ImGuiTools::screenshotType, static_cast<int>(SsType::NUM)},
		PersistentElementMax{"screenshotSize", &ImGuiTools::screenshotSize, static_cast<int>(SsSize::NUM)},
		PersistentElement{"screenshotWithOsd", &ImGuiTools::screenshotWithOsd}
	};
};

} // namespace openmsx

#endif
