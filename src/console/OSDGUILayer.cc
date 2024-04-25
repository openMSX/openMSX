#include "OSDGUILayer.hh"
#include "OSDGUI.hh"
#include "OSDTopWidget.hh"

namespace openmsx {

OSDGUILayer::OSDGUILayer(OSDGUI& gui_)
	: Layer(Coverage::PARTIAL, Z_OSDGUI)
	, gui(gui_)
{
}

OSDGUILayer::~OSDGUILayer()
{
	getGUI().getTopWidget().invalidateRecursive();
}

void OSDGUILayer::paint(OutputSurface& output)
{
	auto& top = getGUI().getTopWidget();
	top.paintRecursive(output);
	top.showAllErrors();
}

} // namespace openmsx
