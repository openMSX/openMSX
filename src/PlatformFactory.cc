// $Id$

// Platform independent includes:
#include "PlatformFactory.hh"
#include "MSXConfig.hh"
#include "RenderSettings.hh"
#include "RendererFactory.hh"

// Platform dependent includes:

Renderer *PlatformFactory::createRenderer(
	const std::string &name, VDP *vdp)
{
	return getRendererFactory(name)->create(vdp);
}

RendererFactory *PlatformFactory::getRendererFactory(const std::string &name)
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

