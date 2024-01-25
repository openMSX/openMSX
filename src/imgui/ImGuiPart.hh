#ifndef IMGUI_PART_HH
#define IMGUI_PART_HH

#include "ImGuiManager.hh"
#include "ImGuiPartInterface.hh"

namespace openmsx {

class ImGuiPart : public ImGuiPartInterface
{
public:
	explicit ImGuiPart(ImGuiManager& manager_)
		: manager(manager_)
	{
		manager.registerPart(this);
	}

	~ImGuiPart()
	{
		manager.unregisterPart(this);
	}

	// disallow copy/move, the address of this object should remain stable
	ImGuiPart(const ImGuiPart&) = delete;
	ImGuiPart(ImGuiPart&&) = delete;
	ImGuiPart& operator=(const ImGuiPart&) = delete;
	ImGuiPart& operator=(ImGuiPart&&) = delete;

protected:
	ImGuiManager& manager;
};

} // namespace openmsx

#endif
