#ifndef IMGUI_CONNECTOR_HH
#define IMGUI_CONNECTOR_HH

#include "ImGuiPart.hh"

namespace openmsx {

class PluggingController;
class Connector;
class Pluggable;

class ImGuiConnector final : public ImGuiPart
{
public:
	using ImGuiPart::ImGuiPart;

	void showMenu(MSXMotherBoard* motherBoard) override;

	enum class Mode {
		COMBO,
		VIEW,
		SUBMENU
	};
	void showPluggables(PluggingController& pluggingController, Mode mode);
private:
	void paintPluggableSelectables(const Connector& connector, const std::vector<std::unique_ptr<Pluggable>>& pluggables);
};

} // namespace openmsx

#endif
