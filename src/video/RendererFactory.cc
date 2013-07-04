#include "RendererFactory.hh"
#include "RenderSettings.hh"
#include "Reactor.hh"
#include "EnumSetting.hh"
#include "Display.hh"
#include "Version.hh"
#include "GLUtil.hh"
#include "memory.hh"
#include "unreachable.hh"

// Video systems:
#include "components.hh"
#include "DummyVideoSystem.hh"
#include "SDLVideoSystem.hh"

// Renderers:
#include "DummyRenderer.hh"
#include "PixelRenderer.hh"
#include "V9990DummyRenderer.hh"
#include "V9990PixelRenderer.hh"

#if COMPONENT_LASERDISC
#include "LDDummyRenderer.hh"
#include "LDPixelRenderer.hh"
#endif

using std::unique_ptr;

namespace openmsx {
namespace RendererFactory {

unique_ptr<VideoSystem> createVideoSystem(Reactor& reactor)
{
	Display& display = reactor.getDisplay();
	switch (display.getRenderSettings().getRenderer().getEnum()) {
		case DUMMY:
			return make_unique<DummyVideoSystem>();
		case SDL:
		case SDLGL_PP:
		case SDLGL_FB16:
		case SDLGL_FB32:
			return make_unique<SDLVideoSystem>(
				reactor, display.getCommandConsole());
		default:
			UNREACHABLE; return nullptr;
	}
}

unique_ptr<Renderer> createRenderer(VDP& vdp, Display& display)
{
	switch (display.getRenderSettings().getRenderer().getEnum()) {
		case DUMMY:
			return make_unique<DummyRenderer>();
		case SDL:
		case SDLGL_PP:
		case SDLGL_FB16:
		case SDLGL_FB32:
			return make_unique<PixelRenderer>(vdp, display);
		default:
			UNREACHABLE; return nullptr;
	}
}

unique_ptr<V9990Renderer> createV9990Renderer(V9990& vdp, Display& display)
{
	switch (display.getRenderSettings().getRenderer().getEnum()) {
		case DUMMY:
			return make_unique<V9990DummyRenderer>();
		case SDL:
		case SDLGL_PP:
		case SDLGL_FB16:
		case SDLGL_FB32:
			return make_unique<V9990PixelRenderer>(vdp);
		default:
			UNREACHABLE; return nullptr;
	}
}

#if COMPONENT_LASERDISC
unique_ptr<LDRenderer> createLDRenderer(LaserdiscPlayer& ld, Display& display)
{
	switch (display.getRenderSettings().getRenderer().getEnum()) {
		case DUMMY:
			return make_unique<LDDummyRenderer>();
		case SDL:
		case SDLGL_PP:
		case SDLGL_FB16:
		case SDLGL_FB32:
			return make_unique<LDPixelRenderer>(ld, display);
		default:
			UNREACHABLE; return nullptr;
	}
}
#endif

unique_ptr<RendererSetting> createRendererSetting(
	CommandController& commandController)
{
	EnumSetting<RendererID>::Map rendererMap;
	rendererMap["none"] = DUMMY; // TODO: only register when in CliComm mode
	rendererMap["SDL"] = SDL;
#if COMPONENT_GL
	// compiled with OpenGL-2.0, still need to test whether
	// it's available at run time, but cannot be done here
	rendererMap["SDLGL-PP"] = SDLGL_PP;
	if (!Version::RELEASE) {
		// disabled for the release:
		//  these renderers don't offer anything more than the existing
		//  renderers and sdlgl-fb32 still has endian problems on PPC
		// TODO is this still true now that SDLGL is removed?
		rendererMap["SDLGL-FB16"] = SDLGL_FB16;
		rendererMap["SDLGL-FB32"] = SDLGL_FB32;
	}
#endif
	auto setting = make_unique<RendererSetting>(commandController,
		"renderer", "rendering back-end used to display the MSX screen",
		SDL, rendererMap);

	// A saved value 'none' can be very confusing. If so change it to 'SDL'.
	if (setting->getEnum() == DUMMY) {
		setting->setEnum(SDL);
	}
	// set saved value as default
	setting->setRestoreValue(setting->getString());

	setting->setEnum(DUMMY); // always start hidden

	return setting;
}

} // namespace RendererFactory
} // namespace openmsx
