// $Id$

// Platform independent includes:
#include "PlatformFactory.hh"
#include "MSXConfig.hh"

// Platform dependent includes:
#include "SDLLoRenderer.hh"
#include "SDLHiRenderer.hh"
#include "SDLGLRenderer.hh"
#include "XRenderer.hh"

Renderer *PlatformFactory::createRenderer(
	const std::string &name, VDP *vdp,
	bool fullScreen, const EmuTime &time)
{
	if (name == "SDLLo") {
		return createSDLLoRenderer(vdp, fullScreen, time);
	}
	else if (name == "SDLHi") {
		return createSDLHiRenderer(vdp, fullScreen, time);
	}
	else if (name == "Xlib") {
		return new XRenderer(vdp, fullScreen, time);
	}
#ifdef __SDLGLRENDERER_AVAILABLE__
	else if (name == "SDLGL") {
		return createSDLGLRenderer(vdp, fullScreen, time);
	}
#endif
	else {
		throw MSXException("Unknown renderer");
	}
}
