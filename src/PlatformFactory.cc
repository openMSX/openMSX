// $Id$

// Platform independent includes:
#include "PlatformFactory.hh"
#include "MSXConfig.hh"
#include "HotKey.hh"

// Platform dependent includes:
#include "SDLHiRenderer.hh"
#include "SDLGLRenderer.hh"

Renderer *PlatformFactory::createRenderer(VDP *vdp, const EmuTime &time)
{
	// Get renderer type and parameters from config.
	MSXConfig::Config *renderConfig =
		MSXConfig::Backend::instance()->getConfigById("renderer");
	bool fullScreen = renderConfig->getParameterAsBool("full_screen");
	std::string renderType = renderConfig->getType();

	PRT_DEBUG("OK\n  Opening display... ");
	Renderer *renderer;
	/*if (renderType == "SDLLo") {
		renderer = createSDLLoRenderer(vdp, fullScreen, time);
	}
	else*/ if (renderType == "SDLHi") {
		renderer = createSDLHiRenderer(vdp, fullScreen, time);
	}
#ifdef __SDLGLRENDERER_AVAILABLE__
	else if (renderType == "SDLGL") {
		renderer = createSDLGLRenderer(vdp, fullScreen, time);
	}
#endif
	else {
		PRT_ERROR("Unknown renderer \"" << renderType << "\"");
		return 0; // unreachable
	}

	// Register hotkey for fullscreen togling
	HotKey::instance()->registerAsyncHotKey(SDLK_PRINT, renderer);

	return renderer;
}
