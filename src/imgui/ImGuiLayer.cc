#include "ImGuiLayer.hh"

#include "ImGuiManager.hh"

namespace openmsx {

ImGuiLayer::ImGuiLayer(ImGuiManager& manager_)
	: Layer(Layer::Coverage::PARTIAL, Layer::ZIndex::IMGUI)
	, manager(manager_)
{
}

void ImGuiLayer::paint(const OutputDimensions& /*output*/)
{
	manager.paintFrame();
}

} // namespace openmsx
