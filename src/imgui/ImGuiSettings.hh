#ifndef IMGUI_SETTINGS_HH
#define IMGUI_SETTINGS_HH

#include "AnalogInput.hh"
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

class JoystickManager;

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
	void deinitListenerIfIdle();

	void setStyle() const;
	void paintJoystick(MSXMotherBoard& motherBoard, JoystickManager& joystickManager);
	void paintCalibrate(JoystickManager& joystickManager);
	void paintFont();
	void paintShortcut();
	void paintEditShortcut();
	void paintExternalVideo();
	void paintFrontPanel();
	void refreshExternalVideoDevices();
	void refreshExternalAudioDevices();
	void startExternalVideo();
	void stopExternalVideo();

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
	unsigned addPopupForKey = unsigned(-1);
	unsigned removePopupForKey = unsigned(-1);
	float popupTimeout = 0.0f;
	bool listening = false;

	bool showCalibrateJoystick = false;

	struct AnalogStatus {
		AnalogInput binding;
		float value = 0.0f;
	};
	std::vector<AnalogStatus> analogBindings;

	int selectedStyle = -1; // no style loaded yet
	unsigned selectedJoystick = 0; // calibrate joystick
	std::string saveLayoutName;

	FileListWidget saveLayout;
	FileListWidget loadLayout;
	ConfirmDialog confirmOverwrite;

	std::vector<FontInfo> availableFonts;
	bool showExternalVideo = false;
	bool showFrontPanel = false;
	int externalVideoSource = 0; // 0: capture device, 1: network stream
	std::string externalVideoDevice;
	std::string externalVideoUrl;
	std::string externalVideoPython = "python";
	std::string externalVideoFFmpeg;
	std::string externalVideoStatus;
	std::string externalVideoStopFile;
	std::string externalVideoFrameFile;
	std::vector<std::string> externalVideoDevices;
	bool externalVideoActive = false;
	bool externalAudio = false; // also capture and play the audio of the source
	std::string externalAudioDevice;
	std::string externalAudioFile;
	std::vector<std::string> externalAudioDevices; // names, as the bridge expects them
	std::vector<std::string> externalAudioLabels;  // friendly names to show
	int externalAudioDelay = 120; // ms
	int externalAudioGain = 0; // dB, applied when the capture starts
	int externalAudioMix = 50; // 0: computer (INT) only, 100: external (EXT) only
	bool externalAudioFollow = false; // obey the audio routing of SET VIDEO

	static constexpr auto persistentElements = std::tuple{
		PersistentElement{"style", &ImGuiSettings::selectedStyle},
		PersistentElement{"showExternalVideo", &ImGuiSettings::showExternalVideo},
		PersistentElement{"showFrontPanel", &ImGuiSettings::showFrontPanel},
		PersistentElement{"externalVideoSource", &ImGuiSettings::externalVideoSource},
		PersistentElement{"externalVideoDevice", &ImGuiSettings::externalVideoDevice},
		PersistentElement{"externalVideoPython", &ImGuiSettings::externalVideoPython},
		PersistentElement{"externalVideoFFmpeg", &ImGuiSettings::externalVideoFFmpeg},
		PersistentElement{"externalAudio", &ImGuiSettings::externalAudio},
		PersistentElement{"externalAudioDevice", &ImGuiSettings::externalAudioDevice},
		PersistentElement{"externalAudioDelay", &ImGuiSettings::externalAudioDelay},
		PersistentElement{"externalAudioGain", &ImGuiSettings::externalAudioGain},
		PersistentElement{"externalAudioMix", &ImGuiSettings::externalAudioMix},
		PersistentElement{"externalAudioFollow", &ImGuiSettings::externalAudioFollow},
	};
};

} // namespace openmsx

#endif
