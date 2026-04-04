#include "OSDGUILayer.hh"

#include "OSDGUI.hh"
#include "OSDTopWidget.hh"

namespace openmsx {

OSDGUILayer::OSDGUILayer(OSDGUI& gui_)
	: gui(gui_)
{
}

OSDGUILayer::~OSDGUILayer()
{
	getGUI().getTopWidget().invalidateRecursive();
}

void OSDGUILayer::paint(const OutputDimensions& output)
{
	auto& top = getGUI().getTopWidget();
	top.paintRecursive(output);
	top.showAllErrors();
}

} // namespace openmsx
