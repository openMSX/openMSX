// $Id$

#include "RendererFactory.hh"
#include "openmsx.hh"
#include "RenderSettings.hh"
#include "DummyRenderer.hh"
#include "SDLRenderer.hh"
#include "SDLGLRenderer.hh"
#include "XRenderer.hh"
#include <SDL/SDL.h>


namespace openmsx {

RendererFactory *RendererFactory::getCurrent()
{
	switch (RenderSettings::instance()->getRenderer()->getValue()) {
	case DUMMY:
		return new DummyRendererFactory();
	case SDLHI:
		return new SDLHiRendererFactory();
	case SDLLO:
		return new SDLLoRendererFactory();
#ifdef __OPENGL_AVAILABLE__
	case SDLGL:
		return new SDLGLRendererFactory();
#endif
#ifndef	X_DISPLAY_MISSING
	case XLIB:
		return new XRendererFactory();
#endif
	default:
		throw MSXException("Unknown renderer");
	}
}

Renderer *RendererFactory::createRenderer(VDP *vdp)
{
	RendererFactory* factory = getCurrent();
	Renderer* result = factory->create(vdp);
	delete factory;
	return result;
}

Renderer *RendererFactory::switchRenderer(VDP *vdp)
{
	RendererSwitcher switcher(vdp);
	return switcher.performSwitch();
}

RendererFactory::RendererSetting *RendererFactory::createRendererSetting()
{
	map<string, RendererID> rendererMap;
	rendererMap["none"] = DUMMY; // TODO: only register when in CliComm mode
	rendererMap["SDLHi"] = SDLHI;
	rendererMap["SDLLo"] = SDLLO;
#ifdef __OPENGL_AVAILABLE__
	rendererMap["SDLGL"] = SDLGL;
#endif
#ifndef	X_DISPLAY_MISSING
	// XRenderer is not ready for users.
	// rendererMap["Xlib" ] = XLIB;
#endif
	return new RendererSetting(
		"renderer", "rendering back-end used to display the MSX screen",
		SDLHI, rendererMap
		);
}

// RendererSwitcher ========================================================

RendererSwitcher::RendererSwitcher(VDP *vdp) {
	this->vdp = vdp;
	renderer = NULL;
}

Renderer *RendererSwitcher::performSwitch() {
	// Actually create renderer.
	renderer = RendererFactory::createRenderer(vdp);
	return renderer;
}

// FullScreenToggler =======================================================

FullScreenToggler::FullScreenToggler(SDL_Surface *screen) {
	this->screen = screen;
}

void FullScreenToggler::performToggle() {
	// Actually toggle.
	SDL_WM_ToggleFullScreen(screen);
}

// Dummy ===================================================================

bool DummyRendererFactory::isAvailable()
{
	return true; // TODO: Actually query.
}

Renderer *DummyRendererFactory::create(VDP *vdp)
{
	return new DummyRenderer(DUMMY, vdp);
}

// SDLHi ===================================================================

bool SDLHiRendererFactory::isAvailable()
{
	return true; // TODO: Actually query.
}

Renderer *SDLHiRendererFactory::create(VDP *vdp)
{
	const unsigned WIDTH = 640;
	const unsigned HEIGHT = 480;

	bool fullScreen = RenderSettings::instance()->getFullScreen()->getValue();
	int flags = SDL_SWSURFACE | (fullScreen ? SDL_FULLSCREEN : 0);

	// Try default bpp.
	SDL_Surface *screen = SDL_SetVideoMode(WIDTH, HEIGHT, 0, flags);
	// Can we handle this bbp?
	int bytepp = (screen ? screen->format->BytesPerPixel : 0);
	if (bytepp != 1 && bytepp != 2 && bytepp != 4) {
		screen = NULL;
	}
	// Try supported bpp in order of preference.
	if (!screen) screen = SDL_SetVideoMode(WIDTH, HEIGHT, 15, flags);
	if (!screen) screen = SDL_SetVideoMode(WIDTH, HEIGHT, 16, flags);
	if (!screen) screen = SDL_SetVideoMode(WIDTH, HEIGHT, 32, flags);
	if (!screen) screen = SDL_SetVideoMode(WIDTH, HEIGHT, 8, flags);

	if (!screen) {
		printf("FAILED to open any screen!");
		// TODO: Throw exception.
		return NULL;
	}
	PRT_DEBUG("Display is " << (int)(screen->format->BitsPerPixel) << " bpp.");

	switch (screen->format->BytesPerPixel) {
	case 1:
		return new SDLRenderer<Uint8, Renderer::ZOOM_512>(SDLHI, vdp, screen);
	case 2:
		return new SDLRenderer<Uint16, Renderer::ZOOM_512>(SDLHI, vdp, screen);
	case 4:
		return new SDLRenderer<Uint32, Renderer::ZOOM_512>(SDLHI, vdp, screen);
	default:
		printf("FAILED to open supported screen!");
		return NULL;
	}
}

// SDLLo ===================================================================

bool SDLLoRendererFactory::isAvailable()
{
	return true; // TODO: Actually query.
}

Renderer *SDLLoRendererFactory::create(VDP *vdp)
{
	const unsigned WIDTH = 320;
	const unsigned HEIGHT = 240;

	bool fullScreen = RenderSettings::instance()->getFullScreen()->getValue();
	int flags = SDL_SWSURFACE | (fullScreen ? SDL_FULLSCREEN : 0);

	// Try default bpp.
	SDL_Surface *screen = SDL_SetVideoMode(WIDTH, HEIGHT, 0, flags);
	// Can we handle this bbp?
	int bytepp = (screen ? screen->format->BytesPerPixel : 0);
	if (bytepp != 1 && bytepp != 2 && bytepp != 4) {
		screen = NULL;
	}
	// Try supported bpp in order of preference.
	if (!screen) screen = SDL_SetVideoMode(WIDTH, HEIGHT, 15, flags);
	if (!screen) screen = SDL_SetVideoMode(WIDTH, HEIGHT, 16, flags);
	if (!screen) screen = SDL_SetVideoMode(WIDTH, HEIGHT, 32, flags);
	if (!screen) screen = SDL_SetVideoMode(WIDTH, HEIGHT, 8, flags);

	if (!screen) {
		printf("FAILED to open any screen!");
		// TODO: Throw exception.
		return NULL;
	}
	PRT_DEBUG("Display is " << (int)(screen->format->BitsPerPixel) << " bpp.");

	switch (screen->format->BytesPerPixel) {
	case 1:
		return new SDLRenderer<Uint8, Renderer::ZOOM_256>(SDLLO, vdp, screen);
	case 2:
		return new SDLRenderer<Uint16, Renderer::ZOOM_256>(SDLLO, vdp, screen);
	case 4:
		return new SDLRenderer<Uint32, Renderer::ZOOM_256>(SDLLO, vdp, screen);
	default:
		printf("FAILED to open supported screen!");
		// TODO: Throw exception.
		return NULL;
	}

}

// SDLGL ===================================================================

#ifdef __OPENGL_AVAILABLE__

bool SDLGLRendererFactory::isAvailable()
{
	return true; // TODO: Actually query.
}

Renderer *SDLGLRendererFactory::create(VDP *vdp)
{
	const unsigned WIDTH = 640;
	const unsigned HEIGHT = 480;

	bool fullScreen = RenderSettings::instance()->getFullScreen()->getValue();
	int flags = SDL_OPENGL | SDL_HWSURFACE
		| (fullScreen ? SDL_FULLSCREEN : 0);

	// Enables OpenGL double buffering.
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, true);

	// Try default bpp.
	SDL_Surface *screen = SDL_SetVideoMode(WIDTH, HEIGHT, 0, flags);
	// Try supported bpp in order of preference.
	if (!screen) screen = SDL_SetVideoMode(WIDTH, HEIGHT, 15, flags);
	if (!screen) screen = SDL_SetVideoMode(WIDTH, HEIGHT, 16, flags);
	if (!screen) screen = SDL_SetVideoMode(WIDTH, HEIGHT, 32, flags);
	if (!screen) screen = SDL_SetVideoMode(WIDTH, HEIGHT, 8, flags);

	if (!screen) {
		printf("FAILED to open any screen!");
		// TODO: Throw exception.
		return NULL;
	}
	PRT_DEBUG("Display is " << (int)(screen->format->BitsPerPixel) << " bpp.");

	return new SDLGLRenderer(SDLGL, vdp, screen);
}

#endif // __OPENGL_AVAILABLE__


#ifndef	X_DISPLAY_MISSING
// Xlib ====================================================================

bool XRendererFactory::isAvailable()
{
	return true; // TODO: Actually query.
}

Renderer *XRendererFactory::create(VDP *vdp)
{
	return new XRenderer(XLIB, vdp);
}
#endif

} // namespace openmsx

