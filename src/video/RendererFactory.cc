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

// Renderers:
#include "DummyRenderer.hh"
#include "PixelRenderer.hh"


namespace openmsx {

Renderer* RendererFactory::createRenderer(VDP* vdp)
{
	switch (RenderSettings::instance().getRenderer()->getValue()) {
	case DUMMY: {
		new DummyVideoSystem();
		return new DummyRenderer();
	}
	case SDLHI: {
		SDLVideoSystem* videoSystem = new SDLVideoSystem(vdp, SDLHI);
		return new PixelRenderer(vdp, videoSystem->rasterizer);
	}
	case SDLLO: {
		SDLVideoSystem* videoSystem = new SDLVideoSystem(vdp, SDLLO);
		return new PixelRenderer(vdp, videoSystem->rasterizer);
	}
#ifdef COMPONENT_GL
	case SDLGL: {
		SDLGLVideoSystem* videoSystem = new SDLGLVideoSystem(vdp);
		return new PixelRenderer(vdp, videoSystem->rasterizer);
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
	XMLElement& rendererElem)
{
	string defaultRenderer = rendererElem.getData();
	
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
	auto_ptr<RendererSetting> setting(new RendererSetting(
		rendererElem, "rendering back-end used to display the MSX screen",
		defaultValue, rendererMap));
	if (CommandLineParser::instance().getParseStatus() ==
	    CommandLineParser::CONTROL) {
		setting->setValue(DUMMY);
	}
	return setting;
}

} // namespace openmsx

