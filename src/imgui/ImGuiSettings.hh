#ifndef IMGUI_SETTINGS_HH
#define IMGUI_SETTINGS_HH

#include "ImGuiPart.hh"

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
};

} // namespace openmsx

#endif
