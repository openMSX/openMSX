// $Id$

// Platform independent includes:
#include "PlatformFactory.hh"
#include "MSXConfig.hh"
#include "RenderSettings.hh"
#include "RendererFactory.hh"

// Platform dependent includes:
#include "SDLLoRenderer.hh"
#include "SDLHiRenderer.hh"
#include "SDLGLRenderer.hh"
#include "XRenderer.hh"

Renderer *PlatformFactory::createRenderer(
	const std::string &name, VDP *vdp, const EmuTime &time)
{
	RendererFactory *factory = getRendererFactory(name);
	return factory->create(vdp, time);
}

RendererFactory *getRendererFactory(const std::string &name)
{
	if (name == "SDLLo") {
		return new SDLLoRendererFactory();
	}
	else if (name == "SDLHi") {
		return new SDLHiRendererFactory();
	}
	else if (name == "Xlib") {
		return new XRendererFactory();
	}
#ifdef __SDLGLRENDERER_AVAILABLE__
	else if (name == "SDLGL") {
		return new SDLGLRendererFactory();
	}
#endif
	else {
		throw MSXException("Unknown renderer");
	}
}

