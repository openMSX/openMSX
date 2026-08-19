#ifndef IMGUI_PLUGGABLE_HH
#define IMGUI_PLUGGABLE_HH

namespace openmsx {

// Optional interface for pluggables that want to render extra inline
// widgets in the connector GUI. Called from ImGuiConnector::showPluggables
// (COMBO/SUBMENU modes) right after the pluggable selector, e.g. to show
// additional items or settings controls next to it.
class ImGuiPluggable
{
public:
	virtual void handleImGuiExtraMenuItems() = 0;

protected:
	~ImGuiPluggable() = default;
};

} // namespace openmsx

#endif
