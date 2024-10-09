#ifndef IMGUI_CODE_ANALYSER_HH
#define IMGUI_CODE_ANALYSER_HH

#include "ImGuiPart.hh"

namespace openmsx {

class ImGuiCodeAnalyzer final : public ImGuiPart
{
public:
	explicit ImGuiCodeAnalyzer(ImGuiManager& manager);
	~ImGuiCodeAnalyzer();

	void paint(MSXMotherBoard* motherBoard) override;

public:
	bool show = false;

private:
	SymbolManager& symbolManager;
	void drawSource();

	bool showSource = false;
};

} // namespace openmsx

#endif
