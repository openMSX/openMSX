// $Id$

#include "RendererFactory.hh"
#include "RenderSettings.hh"
#include "Reactor.hh"
#include "EnumSetting.hh"
#include "Display.hh"
#include "Version.hh"

// Video systems:
#include "components.hh"
#include "DummyVideoSystem.hh"
#include "SDLVideoSystem.hh"

// Renderers:
#include "DummyRenderer.hh"
#include "PixelRenderer.hh"
#include "XRenderer.hh"
#include "V9990DummyRenderer.hh"
#include "V9990PixelRenderer.hh"

using std::auto_ptr;

namespace openmsx {

VideoSystem* RendererFactory::createVideoSystem(Reactor& reactor)
{
	switch (reactor.getDisplay().getRenderSettings().getRenderer().getValue()) {
		case DUMMY:
			return new DummyVideoSystem();
		case SDL:
		case SDLGL:
		case SDLGL2:
		case SDLGL_PP:
		case SDLGL_FB16:
		case SDLGL_FB32:
			return new SDLVideoSystem(reactor);
		default:
			assert(false);
			return 0;
	}
}

Renderer* RendererFactory::createRenderer(VDP& vdp, Display& display)
{
	switch (display.getRenderSettings().getRenderer().getValue()) {
		case DUMMY:
			return new DummyRenderer();
		case SDL:
		case SDLGL:
		case SDLGL2:
		case SDLGL_PP:
		case SDLGL_FB16:
		case SDLGL_FB32:
			return new PixelRenderer(vdp, display);
#ifdef HAVE_X11
		case XLIB:
			return new XRenderer(XLIB, vdp);
#endif
		default:
			assert(false);
			return 0;
	}
}

V9990Renderer* RendererFactory::createV9990Renderer(
	V9990& vdp, Display& display)
{
	switch (display.getRenderSettings().getRenderer().getValue()) {
		case DUMMY:
			return new V9990DummyRenderer();
		case SDL:
		case SDLGL:
		case SDLGL2:
		case SDLGL_PP:
		case SDLGL_FB16:
		case SDLGL_FB32:
			return new V9990PixelRenderer(vdp);
#ifdef HAVE_X11
		case XLIB:
			//return new V9990XRenderer(XLIB, vdp);
			return new V9990DummyRenderer();
#endif
		default:
			assert(false);
			return 0;
	}
}

auto_ptr<RendererFactory::RendererSetting>
	RendererFactory::createRendererSetting(
		CommandController& commandController)
{
	typedef EnumSetting<RendererID>::Map RendererMap;
	RendererMap rendererMap;
	rendererMap["none"] = DUMMY; // TODO: only register when in CliComm mode
	rendererMap["SDL"] = SDL;
#ifdef COMPONENT_GL
	rendererMap["SDLGL"] = SDLGL;
	rendererMap["SDLGL2"] = SDLGL2;
	rendererMap["SDLGL-PP"] = SDLGL_PP;
	if (!Version::RELEASE) {
		// disabled for the release:
		//  these 2 renderers don't offer anything more than the existing
		//  renderers and sdlgl-fb32 still has endian problems on PPC
		rendererMap["SDLGL-FB16"] = SDLGL_FB16;
		rendererMap["SDLGL-FB32"] = SDLGL_FB32;
	}
#endif
#ifdef HAVE_X11
	// XRenderer is not ready for users.
	// rendererMap["Xlib" ] = XLIB;
#endif
	auto_ptr<RendererSetting> setting(new RendererSetting(commandController,
		"renderer", "rendering back-end used to display the MSX screen",
		SDL, rendererMap));

	// a saved value 'none' can be very confusing, if so restore to 'SDL'
	if (setting->getValue() == DUMMY) {
		setting->setValue(SDL);
	}
	// set saved value as default
	setting->setDefaultValue(setting->getValue());

	setting->setValue(DUMMY); // always start hidden

	return setting;
}

} // namespace openmsx

