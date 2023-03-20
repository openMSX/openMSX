#ifndef IMGUI_CONNECTOR_HH
#define IMGUI_CONNECTOR_HH

namespace openmsx {

class ImGuiManager;
class MSXMotherBoard;

class ImGuiConnector
{
public:
	ImGuiConnector(ImGuiManager& manager_)
		: manager(manager_) {}

	void showMenu(MSXMotherBoard* motherBoard);

private:
	ImGuiManager& manager;
};

} // namespace openmsx

#endif
