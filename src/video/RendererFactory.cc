// $Id$

#include "RendererFactory.hh"
#include "RenderSettings.hh"
#include "MSXMotherBoard.hh"
#include "CommandLineParser.hh"
#include "EnumSetting.hh"
#include "VDP.hh"
#include "V9990.hh"
#include "Display.hh"
#include "Version.hh"

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

VideoSystem* RendererFactory::createVideoSystem(MSXMotherBoard& motherboard)
{
	VideoSystem* result;
	Display& display = motherboard.getDisplay();
	switch (display.getRenderSettings().getRenderer().getValue()) {
		case DUMMY:
			result = new DummyVideoSystem();
			break;
		case SDL:
			result = new SDLVideoSystem(motherboard, SDL);
			break;
#ifdef COMPONENT_GL
		case SDLGL:
			result = new SDLGLVideoSystem(motherboard);
			break;
		case SDLGL_FB16:
			result = new SDLVideoSystem(motherboard, SDLGL_FB16);
			break;
		case SDLGL_FB32:
			result = new SDLVideoSystem(motherboard, SDLGL_FB32);
			break;
#endif
		default:
			result = 0;
	}
	assert(result);
	return result;
}

Renderer* RendererFactory::createRenderer(VDP& vdp, Display& display)
{
	Renderer* result;
	switch (display.getRenderSettings().getRenderer().getValue()) {
		case DUMMY:
			result = new DummyRenderer();
			break;
		case SDL:
		case SDLGL:
		case SDLGL_FB16:
		case SDLGL_FB32:
			result = new PixelRenderer(vdp, display);
			break;
#ifdef HAVE_X11
		case XLIB:
			result = new XRenderer(XLIB, vdp);
			break;
#endif
		default:
			result = 0;
	}
	assert(result);
	return result;
}

V9990Renderer* RendererFactory::createV9990Renderer(
	V9990& vdp, Display& display)
{
	V9990Renderer* result;
	switch (display.getRenderSettings().getRenderer().getValue()) {
		case DUMMY:
			result = new V9990DummyRenderer();
			break;
		case SDL:
		case SDLGL:
		case SDLGL_FB16:
		case SDLGL_FB32:
			result = new V9990PixelRenderer(vdp);
			break;
#ifdef HAVE_X11
		case XLIB:
			result = new V9990DummyRenderer();
			//result = new V9990XRenderer(XLIB, vdp);
			break;
#endif
		default:
			result = 0;
	}
	assert(result);
	return result;
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
	if (setting->getValue() == DUMMY) {
		// workaround: sometimes the saved value becomes "none"
		setting->setValue(SDL);
	}
	setting->setDefaultValue(setting->getValue());
	if (CommandLineParser::hiddenStartup) {
		setting->setValue(DUMMY);
	}
	return setting;
}

} // namespace openmsx

