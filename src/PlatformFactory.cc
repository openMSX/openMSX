// $Id$

// Platform independent includes:
#include "PlatformFactory.hh"
#include "MSXConfig.hh"

// Platform dependent includes:
#include "SDLHiRenderer.hh"
#include "SDLGLRenderer.hh"
#include "XRenderer.hh"

Renderer *PlatformFactory::createRenderer(
	const std::string &name, VDP *vdp,
	bool fullScreen, const EmuTime &time)
{
	PRT_DEBUG("OK\n  Opening display... ");
	/*if (renderType == "SDLLo") {
		return createSDLLoRenderer(vdp, fullScreen, time);
	}
	else*/ if (name == "SDLHi") {
		return createSDLHiRenderer(vdp, fullScreen, time);
	}
	else if (name == "Xlib") {
		try {
			return new XRenderer (vdp, fullScreen, time);
		}
		catch (MSXException &) {
			return NULL;
		}
	}
#ifdef __SDLGLRENDERER_AVAILABLE__
	else if (name == "SDLGL") {
		return createSDLGLRenderer(vdp, fullScreen, time);
	}
#endif
	else {
		// throw exception?
		PRT_ERROR("Unknown renderer \"" << name << "\"");
		return 0; // unreachable
	}

}
