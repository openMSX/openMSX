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

	void showMenu(MSXMotherBoard* motherBoard) override;
	void paint(MSXMotherBoard* motherBoard) override;

private:
	int signalEvent(const Event& event) override;
	void initListener();
	void deinitListener();

	void paintJoystick(MSXMotherBoard& motherBoard);
	void paintStyleEditor();

private:
	ImGuiManager& manager;
	bool showConfigureJoystick = false;
	bool showStyleEditor = false;
	bool showDemoWindow = false;

	unsigned joystick = 0;
	unsigned popupForKey = unsigned(-1);
	float popupTimeout = 0.0f;
	bool listening = false;

	std::string saveLayoutName;

	std::string confirmText;
	std::function<void()> confirmAction;
};

} // namespace openmsx

#endif
