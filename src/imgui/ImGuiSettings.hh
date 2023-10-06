#ifndef IMGUI_SETTINGS_HH
#define IMGUI_SETTINGS_HH

#include "ImGuiPart.hh"

#include "EventListener.hh"

namespace openmsx {

class ImGuiManager;

class ImGuiSettings final : public ImGuiPart, private EventListener
{
public:
	enum {UP, DOWN, LEFT, RIGHT, TRIG_A, TRIG_B, NUM_BUTTONS, NUM_DIRECTIONS = TRIG_A};

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

private:
	ImGuiManager& manager;
	bool showConfigureJoystick = false;
	bool showDemoWindow = false;

	unsigned joystick;
	unsigned popupForKey = unsigned(-1);
	float popupTimeout = 0.0f;
	bool listening = false;
};

} // namespace openmsx

#endif
