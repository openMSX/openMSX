#ifndef IMGUI_SETTINGS_HH
#define IMGUI_SETTINGS_HH

#include "ImGuiPart.hh"

#include "Event.hh"
#include "EventListener.hh"

#include <array>
#include <vector>

namespace openmsx {

class ImGuiManager;

class ImGuiSettings final : public ImGuiPart, private EventListener
{
public:
	ImGuiSettings(ImGuiManager& manager_)
		: manager(manager_) {}
	~ImGuiSettings();

	void showMenu(MSXMotherBoard* motherBoard) override;
	void paint(MSXMotherBoard* motherBoard) override;

private:
	int signalEvent(const Event& event) override;
	void initListener();
	void deinitListener();

	void paintJoystick();

private:
	ImGuiManager& manager;
	bool showConfigureJoystick = false;
	bool showDemoWindow = false;

	enum {UP, DOWN, LEFT, RIGHT, TRIG_A, TRIG_B, NUM_BUTTONS, NUM_DIRECTIONS = TRIG_A};
	std::array<std::vector<Event>, NUM_BUTTONS> bindings;

	unsigned popupForKey = unsigned(-1);
	float popupTimeout = 0.0f;
	bool listening = false;
	char protoTypeCounter = 'A';
};

} // namespace openmsx

#endif
