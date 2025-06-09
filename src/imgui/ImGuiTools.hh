#ifndef IMGUI_TOOLS_HH
#define IMGUI_TOOLS_HH

#include "ImGuiPart.hh"
#include "ImGuiUtils.hh"

namespace openmsx {

class ImGuiTools final : public ImGuiPart
{
public:
	explicit ImGuiTools(ImGuiManager& manager);

	[[nodiscard]] zstring_view iniName() const override { return "tools"; }
	void save(ImGuiTextBuffer& buf) override;
	void loadStart() override;
	void loadLine(std::string_view name, zstring_view value) override;
	void showMenu(MSXMotherBoard* motherBoard) override;
	void paint(MSXMotherBoard* motherBoard) override;

private:
	struct Note {
		std::string text;
		bool show = false;
	};

private:
	void paintScreenshot();
	void paintRecord();
	void paintNotes();

	[[nodiscard]] bool screenshotNameExists() const;
	void generateScreenshotName();
	void nextScreenshotName();

	[[nodiscard]] std::string getRecordFilename() const;

private:
	bool showScreenshot = false;
	bool showRecord = false;

	std::string screenshotName;
	enum class SsType : int { RENDERED, MSX, NUM };
	int screenshotType = static_cast<int>(SsType::RENDERED);
	enum class SsSize : int { S_320, S_640, AUTO, NUM }; // keep order for bw compat of persisted element
	int screenshotSize = static_cast<int>(SsSize::AUTO);
	bool screenshotWithOsd = false;
	bool screenshotHideSprites = false;

	std::string recordName;
	enum class Source : int { AUDIO, VIDEO, BOTH, NUM };
	int recordSource = static_cast<int>(Source::BOTH);
	enum class Audio : int { MONO, STEREO, AUTO, NUM };
	int recordAudio = static_cast<int>(Audio::AUTO);
	enum class VideoSize : int { V_320, V_640, V_960, NUM };
	int recordVideoSize = static_cast<int>(VideoSize::V_320);

	ConfirmDialogTclCommand confirmDialog;

	std::vector<Note> notes;

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
