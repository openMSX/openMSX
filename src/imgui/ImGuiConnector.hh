#ifndef IMGUI_CONNECTOR_HH
#define IMGUI_CONNECTOR_HH

#include "ImGuiPart.hh"

namespace openmsx {

class ImGuiConnector final : public ImGuiPart
{
public:
	explicit ImGuiConnector(ImGuiManager& manager_)
		: ImGuiPart(manager_) {}

	void showMenu(MSXMotherBoard* motherBoard) override;
};

} // namespace openmsx

#endif
