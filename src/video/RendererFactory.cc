// $Id$

#include <SDL.h>
#include "RendererFactory.hh"
#include "openmsx.hh"
#include "RenderSettings.hh"
#include "DummyRenderer.hh"
#include "SDLRenderer.hh"
#include "XRenderer.hh"
#include "CliCommOutput.hh"
#include "CommandLineParser.hh"
#include "Icon.hh"
#include "InputEventGenerator.hh"
#include "Version.hh"
#include "HardwareConfig.hh"

#ifdef COMPONENT_GL
#include "SDLGLRenderer.hh"
#endif


namespace openmsx {

static bool initSDLVideo()
{
	if (!SDL_WasInit(SDL_INIT_VIDEO) && SDL_InitSubSystem(SDL_INIT_VIDEO) < 0) {
		//throw FatalError(string("Couldn't init SDL video: ") + SDL_GetError());
		return false; // TODO: use exceptions here too
	}

	HardwareConfig& hardwareConfig = HardwareConfig::instance();
	string title = Version::WINDOW_TITLE + " - " + hardwareConfig.getChild("info").getChildData("manufacturer") + " " + hardwareConfig.getChild("info").getChildData("code");
	SDL_WM_SetCaption(title.c_str(), 0);

	// Set icon
	static unsigned int iconRGBA[OPENMSX_ICON_SIZE * OPENMSX_ICON_SIZE];
	for (int i = 0; i < OPENMSX_ICON_SIZE * OPENMSX_ICON_SIZE; i++) {
 		iconRGBA[i] = iconColours[iconData[i]];
 	}
 	SDL_Surface *iconSurf = SDL_CreateRGBSurfaceFrom(
			iconRGBA, OPENMSX_ICON_SIZE, OPENMSX_ICON_SIZE, 32, OPENMSX_ICON_SIZE * sizeof(unsigned int),
			0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
	SDL_SetColorKey(iconSurf, SDL_SRCCOLORKEY, 0);
	SDL_WM_SetIcon(iconSurf, NULL);
	SDL_FreeSurface(iconSurf);
	InputEventGenerator::instance().reinit();
	return true;
}

auto_ptr<RendererFactory> RendererFactory::getCurrent()
{
	switch (RenderSettings::instance().getRenderer()->getValue()) {
	case DUMMY:
		return auto_ptr<RendererFactory>(new DummyRendererFactory());
	case SDLHI:
		return auto_ptr<RendererFactory>(new SDLHiRendererFactory());
	case SDLLO:
		return auto_ptr<RendererFactory>(new SDLLoRendererFactory());
#ifdef COMPONENT_GL
	case SDLGL:
		return auto_ptr<RendererFactory>(new SDLGLRendererFactory());
#endif
#ifdef HAVE_X11
	case XLIB:
		return auto_ptr<RendererFactory>(new XRendererFactory());
#endif
	default:
		throw MSXException("Unknown renderer");
	}
}

auto_ptr<Renderer> RendererFactory::createRenderer(VDP *vdp)
{
	auto_ptr<RendererFactory> factory = getCurrent();
	return factory->create(vdp);
}

auto_ptr<RendererFactory::RendererSetting> RendererFactory::createRendererSetting(
	const string& defaultRenderer)
{
	typedef EnumSetting<RendererID>::Map RendererMap;
	RendererMap rendererMap;
	rendererMap["none"] = DUMMY; // TODO: only register when in CliComm mode
	rendererMap["SDLHi"] = SDLHI;
	rendererMap["SDLLo"] = SDLLO;
#ifdef COMPONENT_GL
	rendererMap["SDLGL"] = SDLGL;
#endif
#ifdef HAVE_X11
	// XRenderer is not ready for users.
	// rendererMap["Xlib" ] = XLIB;
#endif
	RendererMap::const_iterator it =
	       rendererMap.find(defaultRenderer);
	RendererID defaultValue;
	if (it != rendererMap.end()) {
		defaultValue = it->second;
	} else {
		CliCommOutput::instance().printWarning(
			"Invalid renderer requested: \"" + defaultRenderer + "\"");
		defaultValue = SDLHI;
	}
	RendererID initialValue =
		(CommandLineParser::instance().getParseStatus() ==
		 CommandLineParser::CONTROL)
		? DUMMY
		: defaultValue;
	return auto_ptr<RendererSetting>(new RendererSetting(
		"renderer", "rendering back-end used to display the MSX screen",
		initialValue, defaultValue, rendererMap));
}

// Dummy ===================================================================

bool DummyRendererFactory::isAvailable()
{
	return true; // TODO: Actually query.
}

auto_ptr<Renderer> DummyRendererFactory::create(VDP *vdp)
{
	return auto_ptr<Renderer>(new DummyRenderer(DUMMY, vdp));
}

// SDLHi ===================================================================

bool SDLHiRendererFactory::isAvailable()
{
	return true; // TODO: Actually query.
}

auto_ptr<Renderer> SDLHiRendererFactory::create(VDP *vdp)
{
	const unsigned WIDTH = 640;
	const unsigned HEIGHT = 480;

	bool fullScreen = RenderSettings::instance().getFullScreen()->getValue();
	int flags = SDL_SWSURFACE | (fullScreen ? SDL_FULLSCREEN : 0);

	if (!initSDLVideo()) {
		printf("FAILED to init SDL video!");
		return auto_ptr<Renderer>();
	}

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
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
		// TODO: Throw exception.
		return auto_ptr<Renderer>();
	}
	PRT_DEBUG("Display is " << (int)(screen->format->BitsPerPixel) << " bpp.");

	switch (screen->format->BytesPerPixel) {
	case 1:
		return auto_ptr<Renderer>(
		           new SDLRenderer<Uint8, Renderer::ZOOM_REAL>(
		                                 SDLHI, vdp, screen));
	case 2:
		return auto_ptr<Renderer>(
		           new SDLRenderer<Uint16, Renderer::ZOOM_REAL>(
		                                 SDLHI, vdp, screen));
	case 4:
		return auto_ptr<Renderer>(
		           new SDLRenderer<Uint32, Renderer::ZOOM_REAL>(
		                                 SDLHI, vdp, screen));
	default:
		printf("FAILED to open supported screen!");
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
		return auto_ptr<Renderer>();
	}
}

// SDLLo ===================================================================

bool SDLLoRendererFactory::isAvailable()
{
	return true; // TODO: Actually query.
}

auto_ptr<Renderer> SDLLoRendererFactory::create(VDP *vdp)
{
	const unsigned WIDTH = 320;
	const unsigned HEIGHT = 240;

	bool fullScreen = RenderSettings::instance().getFullScreen()->getValue();
	int flags = SDL_SWSURFACE | (fullScreen ? SDL_FULLSCREEN : 0);

	if (!initSDLVideo()) {
		printf("FAILED to init SDL video!");
		return auto_ptr<Renderer>();
	}

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
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
		// TODO: Throw exception.
		return auto_ptr<Renderer>();
	}
	PRT_DEBUG("Display is " << (int)(screen->format->BitsPerPixel) << " bpp.");

	switch (screen->format->BytesPerPixel) {
	case 1:
		return auto_ptr<Renderer>(
		           new SDLRenderer<Uint8, Renderer::ZOOM_256>(
		                                 SDLLO, vdp, screen));
	case 2:
		return auto_ptr<Renderer>(
		           new SDLRenderer<Uint16, Renderer::ZOOM_256>(
		                                 SDLLO, vdp, screen));
	case 4:
		return auto_ptr<Renderer>(
		           new SDLRenderer<Uint32, Renderer::ZOOM_256>(
		                                 SDLLO, vdp, screen));
	default:
		printf("FAILED to open supported screen!");
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
		// TODO: Throw exception.
		return auto_ptr<Renderer>();
	}

}

// SDLGL ===================================================================

#ifdef COMPONENT_GL

bool SDLGLRendererFactory::isAvailable()
{
	return true; // TODO: Actually query.
}

auto_ptr<Renderer> SDLGLRendererFactory::create(VDP *vdp)
{
	const unsigned WIDTH = 640;
	const unsigned HEIGHT = 480;

	bool fullScreen = RenderSettings::instance().getFullScreen()->getValue();
	int flags = SDL_OPENGL | SDL_HWSURFACE
		| (fullScreen ? SDL_FULLSCREEN : 0);

	if (!initSDLVideo()) {
		printf("FAILED to init SDL video!");
		return auto_ptr<Renderer>();
	}

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
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
		// TODO: Throw exception.
		return auto_ptr<Renderer>();
	}
	PRT_DEBUG("Display is " << (int)(screen->format->BitsPerPixel) << " bpp.");

	return auto_ptr<Renderer>(new SDLGLRenderer(SDLGL, vdp, screen));
}

#endif // COMPONENT_GL


#ifdef HAVE_X11
// Xlib ====================================================================

bool XRendererFactory::isAvailable()
{
	return true; // TODO: Actually query.
}

auto_ptr<Renderer> XRendererFactory::create(VDP *vdp)
{
	return auto_ptr<Renderer>(new XRenderer(XLIB, vdp));
}
#endif

} // namespace openmsx

