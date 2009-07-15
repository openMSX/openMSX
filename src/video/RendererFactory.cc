// $Id$

#include "RendererFactory.hh"
#include "RenderSettings.hh"
#include "Reactor.hh"
#include "EnumSetting.hh"
#include "Display.hh"
#include "Version.hh"
#include "GLUtil.hh"

// Video systems:
#include "components.hh"
#include "DummyVideoSystem.hh"
#include "SDLVideoSystem.hh"

// Renderers:
#include "DummyRenderer.hh"
#include "PixelRenderer.hh"
#include "V9990DummyRenderer.hh"
#include "V9990PixelRenderer.hh"

#ifdef COMPONENT_LASERDISC
#include "LDDummyRenderer.hh"
#include "LDPixelRenderer.hh"
#endif

using std::auto_ptr;

namespace openmsx {
namespace RendererFactory {

VideoSystem* createVideoSystem(Reactor& reactor)
{
	Display& display = reactor.getDisplay();
	switch (display.getRenderSettings().getRenderer().getValue()) {
		case DUMMY:
			return new DummyVideoSystem();
		case SDL:
		case SDLGL_PP:
		case SDLGL_FB16:
		case SDLGL_FB32:
			return new SDLVideoSystem(reactor, display.getCommandConsole());
		default:
			assert(false);
			return 0;
	}
}

Renderer* createRenderer(VDP& vdp, Display& display)
{
	switch (display.getRenderSettings().getRenderer().getValue()) {
		case DUMMY:
			return new DummyRenderer();
		case SDL:
		case SDLGL_PP:
		case SDLGL_FB16:
		case SDLGL_FB32:
			return new PixelRenderer(vdp, display);
		default:
			assert(false);
			return 0;
	}
}

V9990Renderer* createV9990Renderer(V9990& vdp, Display& display)
{
	switch (display.getRenderSettings().getRenderer().getValue()) {
		case DUMMY:
			return new V9990DummyRenderer();
		case SDL:
		case SDLGL_PP:
		case SDLGL_FB16:
		case SDLGL_FB32:
			return new V9990PixelRenderer(vdp);
		default:
			assert(false);
			return 0;
	}
}

#ifdef COMPONENT_LASERDISC
LDRenderer* createLDRenderer(LaserdiscPlayer& ld, Display& display)
{
	switch (display.getRenderSettings().getRenderer().getValue()) {
		case DUMMY:
			return new LDDummyRenderer();
		case SDL:
		case SDLGL_PP:
		case SDLGL_FB16:
		case SDLGL_FB32:
			return new LDPixelRenderer(ld, display);
		default:
			assert(false);
			return 0;
	}
}
#endif

auto_ptr<RendererSetting> createRendererSetting(
	CommandController& commandController)
{
	typedef EnumSetting<RendererID>::Map RendererMap;
	RendererMap rendererMap;
	rendererMap["none"] = DUMMY; // TODO: only register when in CliComm mode
	rendererMap["SDL"] = SDL;
#ifdef COMPONENT_GL
	#ifdef GL_VERSION_2_0
	// compiled with OpenGL-2.0, still need to test whether
	// it's available at run time, but cannot be done here
	rendererMap["SDLGL-PP"] = SDLGL_PP;
	#endif
	if (!Version::RELEASE) {
		// disabled for the release:
		//  these renderers don't offer anything more than the existing
		//  renderers and sdlgl-fb32 still has endian problems on PPC
		// TODO is this still true now that SDLGL is removed?
		rendererMap["SDLGL-FB16"] = SDLGL_FB16;
		rendererMap["SDLGL-FB32"] = SDLGL_FB32;
	}
#endif
	auto_ptr<RendererSetting> setting(new RendererSetting(commandController,
		"renderer", "rendering back-end used to display the MSX screen",
		SDL, rendererMap));

	// A saved value 'none' can be very confusing, so don't save it.
	// If it did get saved for some reason (old openmsx version?)
	// then change it to 'SDL'
	setting->setDontSaveValue("none");
	if (setting->getValue() == DUMMY) {
		setting->changeValue(SDL);
	}
	// set saved value as default
	setting->setRestoreValue(setting->getValue());

	setting->changeValue(DUMMY); // always start hidden

	return setting;
}

} // namespace RendererFactory
} // namespace openmsx

