// $Id$

#include "OSDGUILayer.hh"
#include "OSDGUI.hh"
#include "OSDWidget.hh"

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

string_ref OSDGUILayer::getLayerName() const
{
	return "OSDGUI";
}


// class SDLOSDGUILayer

SDLOSDGUILayer::SDLOSDGUILayer(OSDGUI& gui)
	: OSDGUILayer(gui)
{
	getGUI().setOpenGL(false);
}

void SDLOSDGUILayer::paint(OutputSurface& output)
{
	getGUI().getTopWidget().paintSDLRecursive(output);
}


// class GLOSDGUILayer

GLOSDGUILayer::GLOSDGUILayer(OSDGUI& gui)
	: OSDGUILayer(gui)
{
	getGUI().setOpenGL(true);
}

void GLOSDGUILayer::paint(OutputSurface& output)
{
	getGUI().getTopWidget().paintGLRecursive(output);
}

} // namespace openmsx
