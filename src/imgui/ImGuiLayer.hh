#ifndef IMGUILAYER_HH
#define IMGUILAYER_HH

#include "Layer.hh"

namespace openmsx {

class ImGuiManager;

class ImGuiLayer final : public Layer
{
public:
	explicit ImGuiLayer(ImGuiManager& manager);

private:
	// Layer
	void paint(OutputSurface& output) override;

private:
	ImGuiManager& manager;
};

} // namespace openmsx

#endif
