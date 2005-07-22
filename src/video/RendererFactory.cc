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

int RendererFactory::createInProgress = 0;


VideoSystem* RendererFactory::createVideoSystem()
{
	++createInProgress;
	VideoSystem* result;
	switch (RenderSettings::instance().getRenderer().getValue()) {
		case DUMMY:
			result = new DummyVideoSystem();
			break;
		case SDLHI:
			result = new SDLVideoSystem();
			break;
#ifdef COMPONENT_GL
		case SDLGL:
			result = new SDLGLVideoSystem();
			break;
#endif
		default:
			result = 0;
	}
	--createInProgress;
	assert(result);
	return result;
}

Renderer* RendererFactory::createRenderer(VDP& vdp)
{
	++createInProgress;
	Renderer* result;
	switch (RenderSettings::instance().getRenderer().getValue()) {
		case DUMMY:
			result = new DummyRenderer();
			break;
		case SDLHI:
		case SDLGL:
			result = new PixelRenderer(vdp);
			break;
#ifdef HAVE_X11
		case XLIB:
			result = new XRenderer(XLIB, vdp);
			break;
#endif
		default:
			result = 0;
	}
	--createInProgress;
	assert(result);
	return result;
}

V9990Renderer* RendererFactory::createV9990Renderer(V9990& vdp)
{
	++createInProgress;
	V9990Renderer* result;
	switch (RenderSettings::instance().getRenderer().getValue()) {
		case DUMMY:
			result = new V9990DummyRenderer();
			break;
		case SDLHI:
		case SDLGL:
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
	--createInProgress;
	assert(result);
	return result;
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

bool RendererFactory::isCreateInProgress()
{
	return createInProgress;
}

} // namespace openmsx

