// $Id$

#ifndef SDLUTIL_HH
#define SDLUTIL_HH

struct SDL_Surface;

namespace openmsx {

class RenderSettings;

/** Initialise SDL's video subsystem and find a usable video mode.
  * Also configures all window properties, like title, icon and
  * disabling the mouse cursor.
  * @param width Horizontal resolution in pixels.
  * @param height Vertical resolution in pixels.
  * @param flags Flags indicating what kind of drawing surface is desired.
  *   See SDL_SetVideoMode documentation for details.
  *   Do not specify full screen flag; it is added automatically if needed.
  * @return The surface that can be drawn to.
  * @throws InitException If initialisation fails
  *   or no suitable video mode is found.
  */
SDL_Surface* openSDLVideo(RenderSettings& renderSettings,
                          int width, int height, int flags);

/** Shuts down SDL's video subsystem.
  */
void closeSDLVideo(SDL_Surface* screen);

} // namespace openmsx

#endif
