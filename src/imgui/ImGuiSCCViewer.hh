#ifndef IMGUI_SCCVIEWER_HH
#define IMGUI_SCCVIEWER_HH

#include "ImGuiPart.hh"

namespace openmsx {

class ImGuiSCCViewer final : public ImGuiPart
{
public:
	explicit ImGuiSCCViewer(ImGuiManager& manager);

	void paint(MSXMotherBoard* motherBoard) override;

public:
	bool show = false;
};

} // namespace openmsx

#endif
