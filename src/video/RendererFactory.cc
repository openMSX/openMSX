// $Id$

#include "RendererFactory.hh"
#include "RenderSettings.hh"
#include "CliCommOutput.hh"
#include "CommandLineParser.hh"

// Video systems:
#include "DummyVideoSystem.hh"
#include "SDLVideoSystem.hh"
#ifdef COMPONENT_GL
#include "SDLGLVideoSystem.hh"
#endif
#include "XRenderer.hh"


namespace openmsx {

Renderer* RendererFactory::createRenderer(VDP* vdp)
{
	switch (RenderSettings::instance().getRenderer()->getValue()) {
	case DUMMY: {
		DummyVideoSystem* videoSystem = new DummyVideoSystem();
		return videoSystem->renderer;
	}
	case SDLHI: {
		SDLVideoSystem* videoSystem = new SDLVideoSystem(vdp, SDLHI);
		return videoSystem->renderer;
	}
	case SDLLO: {
		SDLVideoSystem* videoSystem = new SDLVideoSystem(vdp, SDLLO);
		return videoSystem->renderer;
	}
#ifdef COMPONENT_GL
	case SDLGL: {
		SDLGLVideoSystem* videoSystem = new SDLGLVideoSystem(vdp);
		return videoSystem->renderer;
	}
#endif
#ifdef HAVE_X11
	case XLIB: {
		return new XRenderer(XLIB, vdp);
	}
#endif
	default:
		assert(false);
		return 0;
	}
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
	RendererMap::const_iterator it = rendererMap.find(defaultRenderer);
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

} // namespace openmsx

