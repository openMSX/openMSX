#ifndef IMGUI_SETTINGS_HH
#define IMGUI_SETTINGS_HH

#include "FileListWidget.hh"
#include "ImGuiPart.hh"
#include "ImGuiUtils.hh"
#include "freetype_utils.hh"

#include "Shortcuts.hh"

#include "EventListener.hh"

#include <span>
#include <string>
#include <vector>

namespace openmsx {

class ImGuiSettings final : public ImGuiPart, private EventListener
{
public:
	explicit ImGuiSettings(ImGuiManager& manager);
	~ImGuiSettings();

	[[nodiscard]] zstring_view iniName() const override { return "settings"; }
	void save(ImGuiTextBuffer& buf) override;
	void loadLine(std::string_view name, zstring_view value) override;
	void loadEnd() override;

	void showMenu(MSXMotherBoard* motherBoard) override;
	void paint(MSXMotherBoard* motherBoard) override;

private:
	bool signalEvent(const Event& event) override;
	void initListener();
	void deinitListener();

	void setStyle() const;
	void paintJoystick(MSXMotherBoard& motherBoard);
	void paintFont();
	void paintShortcut();
	void paintEditShortcut();

	struct FontInfo {
		std::string filename;
		std::string displayName;
		int faceIndex = 0;
	};
	std::span<const FontInfo> getAvailableFonts();

private:
	bool showConfigureJoystick = false;
	bool showFont = false;
	bool showShortcut = false;
	Shortcuts::ID editShortcutId = Shortcuts::ID::INVALID;

	unsigned joystick = 0;
	unsigned popupForKey = unsigned(-1);
	float popupTimeout = 0.0f;
	bool listening = false;

	int selectedStyle = -1; // no style loaded yet
	std::string saveLayoutName;

	FileListWidget saveLayout;
	FileListWidget loadLayout;
	ConfirmDialog confirmOverwrite;

	std::vector<FontInfo> availableFonts;

	static constexpr auto persistentElements = std::tuple{
		PersistentElement{"style", &ImGuiSettings::selectedStyle},
	};
};

} // namespace openmsx

#endif
