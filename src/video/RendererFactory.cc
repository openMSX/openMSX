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

// Rasterizers:
#include "GLRasterizer.hh"


namespace openmsx {

Renderer* RendererFactory::createRenderer(VDP* vdp)
{
	RendererID id = RenderSettings::instance().getRenderer()->getValue();
	switch (id) {
	case DUMMY: {
		new DummyVideoSystem();
		return new DummyRenderer();
	}
	case SDLHI:
	case SDLLO: {
		SDLVideoSystem* videoSystem = new SDLVideoSystem(id);
		return new PixelRenderer(vdp, videoSystem->createRasterizer(vdp, id));
	}
#ifdef COMPONENT_GL
	case SDLGL: {
		new SDLGLVideoSystem();
		return new PixelRenderer(vdp, new GLRasterizer(vdp));
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

auto_ptr<RendererFactory::RendererSetting> RendererFactory::createRendererSetting()
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
	auto_ptr<RendererSetting> setting(new RendererSetting(
		"renderer", "rendering back-end used to display the MSX screen",
		SDLHI, rendererMap));
	if (CommandLineParser::instance().getParseStatus() ==
	    CommandLineParser::CONTROL) {
		setting->setValue(DUMMY);
	}
	return setting;
}

} // namespace openmsx

