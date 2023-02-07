#ifndef IMGUILAYER_HH
#define IMGUILAYER_HH

#include "Layer.hh"
#include <memory>

namespace openmsx {

class DebuggableEditor;
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
	std::unique_ptr<DebuggableEditor> mem_edit;
	bool show_demo_window = false;
	bool first = true;
};

} // namespace openmsx

#endif
