// $Id$

#include <SDL.h>
#include "RendererFactory.hh"
#include "openmsx.hh"
#include "RenderSettings.hh"
#include "DummyRenderer.hh"
#include "CliCommOutput.hh"
#include "CommandLineParser.hh"
#include "Icon.hh"
#include "InputEventGenerator.hh"
#include "Version.hh"
#include "HardwareConfig.hh"
#include "Display.hh"
#include "CommandConsole.hh"
#include "SDLUtil.hh"

// Video system Dummy
// Note: DummyRenderer is not part of Dummy video system.
#include "DummyVideoSystem.hh"

// Video system SDL
#include "SDLVideoSystem.hh"

// Video system SDLGL
#ifdef COMPONENT_GL
#include "SDLGLVideoSystem.hh"
#endif

// Video system X11
#include "XRenderer.hh"


namespace openmsx {

auto_ptr<RendererFactory> RendererFactory::getCurrent()
{
	switch (RenderSettings::instance().getRenderer()->getValue()) {
	case DUMMY:
		return auto_ptr<RendererFactory>(new DummyRendererFactory());
	case SDLHI:
		return auto_ptr<RendererFactory>(new SDLHiRendererFactory());
	case SDLLO:
		return auto_ptr<RendererFactory>(new SDLLoRendererFactory());
#ifdef COMPONENT_GL
	case SDLGL:
		return auto_ptr<RendererFactory>(new SDLGLRendererFactory());
#endif
#ifdef HAVE_X11
	case XLIB:
		return auto_ptr<RendererFactory>(new XRendererFactory());
#endif
	default:
		throw MSXException("Unknown renderer");
	}
}

Renderer* RendererFactory::createRenderer(VDP* vdp)
{
	auto_ptr<RendererFactory> factory = getCurrent();
	return factory->create(vdp);
}

auto_ptr<RendererFactory::RendererSetting> RendererFactory::createRendererSetting(
	const string& defaultRenderer)
{
	typedef EnumSetting<RendererID>::Map RendererMap;
	RendererMap rendererMap;
	rendererMap["none"] = DUMMY; // TODO: only register when in CliComm mode
	rendererMap["SDLHi"] = SDLHI;
	rendererMap["SDLLo"] = SDLLO;
#ifdef COMPONENT_GL
	rendererMap["SDLGL"] = SDLGL;
#endif
#ifdef HAVE_X11
	// XRenderer is not ready for users.
	// rendererMap["Xlib" ] = XLIB;
#endif
	RendererMap::const_iterator it =
	       rendererMap.find(defaultRenderer);
	RendererID defaultValue;
	if (it != rendererMap.end()) {
		defaultValue = it->second;
	} else {
		CliCommOutput::instance().printWarning(
			"Invalid renderer requested: \"" + defaultRenderer + "\"");
		defaultValue = SDLHI;
	}
	RendererID initialValue =
		(CommandLineParser::instance().getParseStatus() ==
		 CommandLineParser::CONTROL)
		? DUMMY
		: defaultValue;
	return auto_ptr<RendererSetting>(new RendererSetting(
		"renderer", "rendering back-end used to display the MSX screen",
		initialValue, defaultValue, rendererMap));
}

// Dummy ===================================================================

bool DummyRendererFactory::isAvailable()
{
	return true;
}

Renderer* DummyRendererFactory::create(VDP* /*vdp*/)
{
	// Destruct old layers.
	Display::INSTANCE.reset();
	Display* display = new Display(auto_ptr<VideoSystem>(
		new DummyVideoSystem() ));
	Renderer* renderer = new DummyRenderer(DUMMY);
	display->addLayer(dynamic_cast<Layer*>(renderer));
	display->setAlpha(dynamic_cast<Layer*>(renderer), 255);
	Display::INSTANCE.reset(display);
	return renderer;
}

// SDLHi ===================================================================

bool SDLHiRendererFactory::isAvailable()
{
	return true; // TODO: Actually query.
}

Renderer* SDLHiRendererFactory::create(VDP* vdp)
{
	SDLVideoSystem* videoSystem = new SDLVideoSystem(vdp, SDLHI);
	return videoSystem->renderer;
}

// SDLLo ===================================================================

bool SDLLoRendererFactory::isAvailable()
{
	return true; // TODO: Actually query.
}

Renderer* SDLLoRendererFactory::create(VDP* vdp)
{
	SDLVideoSystem* videoSystem = new SDLVideoSystem(vdp, SDLLO);
	return videoSystem->renderer;
}

// SDLGL ===================================================================

#ifdef COMPONENT_GL

bool SDLGLRendererFactory::isAvailable()
{
	return true; // TODO: Actually query.
}

Renderer* SDLGLRendererFactory::create(VDP* vdp)
{
	SDLGLVideoSystem* videoSystem = new SDLGLVideoSystem(vdp);
	return videoSystem->renderer;
}

#endif // COMPONENT_GL


#ifdef HAVE_X11
// Xlib ====================================================================

bool XRendererFactory::isAvailable()
{
	return true; // TODO: Actually query.
}

Renderer* XRendererFactory::create(VDP* vdp)
{
	return new XRenderer(XLIB, vdp);
}
#endif

} // namespace openmsx

