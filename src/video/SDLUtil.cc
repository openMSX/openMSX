// $Id$

#include "SDLUtil.hh"
#include "Icon.hh"
#include "InputEventGenerator.hh"
#include "Version.hh"
#include "HardwareConfig.hh"
#include "InitException.hh"
#include <SDL.h>


namespace openmsx {

void initSDLVideo()
{
	if (!SDL_WasInit(SDL_INIT_VIDEO)
	&& SDL_InitSubSystem(SDL_INIT_VIDEO) < 0) {
		throw InitException(
			string("SDL video init failed: ") + SDL_GetError() );
	}

	HardwareConfig& hardwareConfig = HardwareConfig::instance();
	string title = Version::WINDOW_TITLE + " - " +
		hardwareConfig.getChild("info").getChildData("manufacturer") + " " +
		hardwareConfig.getChild("info").getChildData("code");
	SDL_WM_SetCaption(title.c_str(), 0);

	// Set icon.
	static unsigned int iconRGBA[OPENMSX_ICON_SIZE * OPENMSX_ICON_SIZE];
	for (int i = 0; i < OPENMSX_ICON_SIZE * OPENMSX_ICON_SIZE; i++) {
 		iconRGBA[i] = iconColours[iconData[i]];
 	}
 	SDL_Surface* iconSurf = SDL_CreateRGBSurfaceFrom(
		iconRGBA, OPENMSX_ICON_SIZE, OPENMSX_ICON_SIZE, 32,
		OPENMSX_ICON_SIZE * sizeof(unsigned int),
		0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000
		);
	SDL_SetColorKey(iconSurf, SDL_SRCCOLORKEY, 0);
	SDL_WM_SetIcon(iconSurf, NULL);
	SDL_FreeSurface(iconSurf);

	InputEventGenerator::instance().reinit();
}

} // namespace openmsx

