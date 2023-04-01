#ifndef IMGUI_SETTINGS_HH
#define IMGUI_SETTINGS_HH

namespace openmsx {

class ImGuiManager;

class ImGuiSettings
{
public:
	ImGuiSettings(ImGuiManager& manager_)
		: manager(manager_) {}

	void showMenu();

private:
	ImGuiManager& manager;
	bool showDemoWindow = false;
};

} // namespace openmsx

#endif
