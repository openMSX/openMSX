#ifndef IMGUI_SETTINGS_HH
#define IMGUI_SETTINGS_HH

#include "ImGuiPart.hh"

#include "Event.hh"

#include <array>
#include <vector>

namespace openmsx {

class ImGuiManager;

class ImGuiSettings final : public ImGuiPart
{
public:
	ImGuiSettings(ImGuiManager& manager_)
		: manager(manager_) {}

	void showMenu(MSXMotherBoard* motherBoard) override;
	void paint(MSXMotherBoard* motherBoard) override;

private:
	void paintJoystick();

private:
	ImGuiManager& manager;
	bool showConfigureJoystick = false;
	bool showDemoWindow = false;

	enum {UP, DOWN, LEFT, RIGHT, TRIG_A, TRIG_B, NUM_BUTTONS, NUM_DIRECTIONS = TRIG_A};
	std::array<std::vector<Event>, NUM_BUTTONS> bindings;
};

} // namespace openmsx

#endif
