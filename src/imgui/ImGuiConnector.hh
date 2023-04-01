#ifndef IMGUI_CONNECTOR_HH
#define IMGUI_CONNECTOR_HH

#include "ImGuiPart.hh"

namespace openmsx {

class ImGuiManager;
class MSXMotherBoard;

class ImGuiConnector final : public ImGuiPart
{
public:
	ImGuiConnector(ImGuiManager& manager_)
		: manager(manager_) {}

	void showMenu(MSXMotherBoard* motherBoard) override;

private:
	ImGuiManager& manager;
};

} // namespace openmsx

#endif
