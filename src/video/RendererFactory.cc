// $Id$

#include "RendererFactory.hh"
#include "RenderSettings.hh"
#include "CommandLineParser.hh"
#include "EnumSetting.hh"

// Video systems:
#include "components.hh"
#include "DummyVideoSystem.hh"
#include "SDLVideoSystem.hh"
#ifdef COMPONENT_GL
#include "SDLGLVideoSystem.hh"
#endif
#include "XRenderer.hh"

// Renderers:
#include "DummyRenderer.hh"
#include "PixelRenderer.hh"

#include "V9990DummyRenderer.hh"
#include "V9990PixelRenderer.hh"

using std::auto_ptr;

namespace openmsx {

VideoSystem* RendererFactory::createVideoSystem()
{
	RendererID id = RenderSettings::instance().getRenderer().getValue();
	switch (id) {
	case DUMMY: {
		return new DummyVideoSystem();
	}
	case SDLHI: {
		return new SDLVideoSystem();
	}
#ifdef COMPONENT_GL
	case SDLGL: {
		return new SDLGLVideoSystem();
	}
#endif
#ifdef HAVE_X11
	case XLIB: {
		return 0; // TODO: Implement X11 video system.
	}
#endif
	default:
		assert(false);
		return 0;
	}
}

Renderer* RendererFactory::createRenderer(VDP& vdp)
{
	RendererID id = RenderSettings::instance().getRenderer().getValue();
	switch (id) {
	case DUMMY: {
		return new DummyRenderer();
	}
	case SDLHI:
	case SDLGL: {
		return new PixelRenderer(vdp);
	}
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

V9990Renderer* RendererFactory::createV9990Renderer(V9990& vdp)
{
	RendererID id = RenderSettings::instance().getRenderer().getValue();
	switch (id) {
	case DUMMY: {
		return new V9990DummyRenderer();
	}
	case SDLHI:
	case SDLGL: {
		return new V9990PixelRenderer(vdp);
	}
#ifdef HAVE_X11
	case XLIB: {
		return new V9990DummyRenderer();
		//return new V9990XRenderer(XLIB, vdp);
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
	if (setting->getValue() == DUMMY) {
		// workaround: sometimes the saved value becomes "none"
		setting->setValue(SDLHI);
	}
	setting->setDefaultValue(setting->getValue());
	if (CommandLineParser::hiddenStartup) {
		setting->setValue(DUMMY);
	}
	return setting;
}

} // namespace openmsx

