#include "OSDGUILayer.hh"
#include "OSDGUI.hh"
#include "OSDTopWidget.hh"

namespace openmsx {

// class OSDGUILayer

OSDGUILayer::OSDGUILayer(OSDGUI& gui_)
	: Layer(COVER_PARTIAL, Z_OSDGUI)
	, gui(gui_)
{
}

OSDGUILayer::~OSDGUILayer()
{
	getGUI().getTopWidget().invalidateRecursive();
}

OSDGUI& OSDGUILayer::getGUI()
{
	return gui;
}


// class SDLOSDGUILayer

SDLOSDGUILayer::SDLOSDGUILayer(OSDGUI& gui)
	: OSDGUILayer(gui)
{
	getGUI().setOpenGL(false);
}

void SDLOSDGUILayer::paint(OutputSurface& output)
{
	auto& top = getGUI().getTopWidget();
	top.paintSDLRecursive(output);
	top.showAllErrors();
}


// class GLOSDGUILayer

GLOSDGUILayer::GLOSDGUILayer(OSDGUI& gui)
	: OSDGUILayer(gui)
{
	getGUI().setOpenGL(true);
}

void GLOSDGUILayer::paint(OutputSurface& output)
{
	auto& top = getGUI().getTopWidget();
	top.paintGLRecursive(output);
	top.showAllErrors();
}

} // namespace openmsx
