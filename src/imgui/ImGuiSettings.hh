#ifndef IMGUI_SETTINGS_HH
#define IMGUI_SETTINGS_HH

#include "ImGuiPart.hh"

#include "EventListener.hh"

#include <functional>
#include <string>

namespace openmsx {

class ImGuiManager;

class ImGuiSettings final : public ImGuiPart, private EventListener
{
public:
	ImGuiSettings(ImGuiManager& manager);
	~ImGuiSettings();

	[[nodiscard]] virtual zstring_view iniName() const { return "settings"; }
	void save(ImGuiTextBuffer& buf) override;
	void loadLine(std::string_view name, zstring_view value) override;
	void loadEnd() override;

	void showMenu(MSXMotherBoard* motherBoard) override;
	void paint(MSXMotherBoard* motherBoard) override;

private:
	int signalEvent(const Event& event) override;
	void initListener();
	void deinitListener();

	void setStyle();
	void paintJoystick(MSXMotherBoard& motherBoard);

private:
	ImGuiManager& manager;
	bool showConfigureJoystick = false;
	bool showDemoWindow = false;

	unsigned joystick = 0;
	unsigned popupForKey = unsigned(-1);
	float popupTimeout = 0.0f;
	bool listening = false;

	int selectedStyle = 0; // dark (also the default (recommended) Dear ImGui style)
	std::string saveLayoutName;

	std::string confirmText;
	std::function<void()> confirmAction;

	static constexpr auto persistentElements = std::tuple{
		PersistentElement{"style", &ImGuiSettings::selectedStyle},
	};
};

} // namespace openmsx

#endif
