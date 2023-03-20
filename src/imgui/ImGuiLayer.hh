#ifndef IMGUILAYER_HH
#define IMGUILAYER_HH

#include "Layer.hh"

#include <string>

namespace openmsx {

class Reactor;

class ImGuiLayer final : public Layer
{
public:
	ImGuiLayer(Reactor& reactor);
	~ImGuiLayer() override;

private:
	// Layer
	void paint(OutputSurface& output) override;

private:
	Reactor& reactor;

	bool showDemoWindow = false;
	bool first = true;
};

} // namespace openmsx

#endif
