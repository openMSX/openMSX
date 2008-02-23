// $Id$

#include "OSDGUILayer.hh"
#include "OSDGUI.hh"
#include "OSDWidget.hh"
#include "Display.hh"
#include "VideoSystem.hh"

namespace openmsx {

class OutputSurface;

// class OSDGUILayer

OSDGUILayer::OSDGUILayer(OSDGUI& gui_)
	: Layer(COVER_PARTIAL, Z_OSDGUI)
	, gui(gui_)
{
}

OSDGUILayer::~OSDGUILayer()
{
	const OSDGUI::Widgets& widgets = getGUI().getWidgets();
	for (OSDGUI::Widgets::const_iterator it = widgets.begin();
	     it != widgets.end(); ++it) {
		(*it)->invalidate();
	}
}

OSDGUI& OSDGUILayer::getGUI()
{
	return gui;
}

const std::string& OSDGUILayer::getName()
{
	static const std::string name = "OSDGUI";
	return name;
}


// class SDLOSDGUILayer

SDLOSDGUILayer::SDLOSDGUILayer(OSDGUI& gui)
	: OSDGUILayer(gui)
{
}

void SDLOSDGUILayer::paint()
{
	OutputSurface* output =
		getGUI().getDisplay().getVideoSystem().getOutputSurface();
	if (!output) return;

	const OSDGUI::Widgets& widgets = getGUI().getWidgets();
	for (OSDGUI::Widgets::const_iterator it = widgets.begin();
	     it != widgets.end(); ++it) {
		(*it)->paintSDL(*output);
	}
}


// class GLOSDGUILayer

GLOSDGUILayer::GLOSDGUILayer(OSDGUI& gui)
	: OSDGUILayer(gui)
{
}

void GLOSDGUILayer::paint()
{
	OutputSurface* output =
		getGUI().getDisplay().getVideoSystem().getOutputSurface();
	if (!output) return;

	const OSDGUI::Widgets& widgets = getGUI().getWidgets();
	for (OSDGUI::Widgets::const_iterator it = widgets.begin();
	     it != widgets.end(); ++it) {
		(*it)->paintGL(*output);
	}
}

} // namespace openmsx
