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
	void paintRecord();

	[[nodiscard]] bool screenshotNameExists() const;
	void generateScreenshotName();
	void nextScreenshotName();

	[[nodiscard]] std::string getRecordFilename() const;

private:
	ImGuiManager& manager;
	bool showScreenshot = false;
	bool showRecord = false;

	std::string screenshotName;
	enum class SsType : int { RENDERED, MSX, NUM };
	int screenshotType = static_cast<int>(SsType::RENDERED);
	enum class SsSize : int { S_320, S_640, NUM };
	int screenshotSize = static_cast<int>(SsSize::S_320);
	bool screenshotWithOsd = false;
	bool screenshotHideSprites = false;

	std::string recordName;
	enum class Source : int { AUDIO, VIDEO, BOTH, NUM };
	int recordSource = static_cast<int>(Source::BOTH);
	enum class Audio : int { MONO, STEREO, AUTO, NUM };
	int recordAudio = static_cast<int>(Audio::AUTO);
	enum class VideoSize : int { V_320, V_640, V_960, NUM };
	int recordVideoSize = static_cast<int>(VideoSize::V_320);

	TclObject confirmCmd;
	std::string confirmText;
	bool openConfirmPopup = false;

	static constexpr auto persistentElements = std::tuple{
		PersistentElement{"showScreenshot",  &ImGuiTools::showScreenshot},
		PersistentElement{"showRecord", &ImGuiTools::showRecord},
		PersistentElementMax{"screenshotType", &ImGuiTools::screenshotType, static_cast<int>(SsType::NUM)},
		PersistentElementMax{"screenshotSize", &ImGuiTools::screenshotSize, static_cast<int>(SsSize::NUM)},
		PersistentElement{"screenshotWithOsd", &ImGuiTools::screenshotWithOsd},
		PersistentElementMax{"recordSource", &ImGuiTools::recordSource, static_cast<int>(Source::NUM)},
		PersistentElementMax{"recordAudio", &ImGuiTools::recordAudio, static_cast<int>(Audio::NUM)},
		PersistentElementMax{"recordVideoSize", &ImGuiTools::recordVideoSize, static_cast<int>(VideoSize::NUM)}
	};
};

} // namespace openmsx

#endif
