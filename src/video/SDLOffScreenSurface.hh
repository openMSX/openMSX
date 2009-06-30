// $Id$

#ifndef SDLOFFSCREENSURFACE_HH
#define SDLOFFSCREENSURFACE_HH

#include "OutputSurface.hh"

namespace openmsx {

class SDLOffScreenSurface : public OutputSurface
{
public:
	explicit SDLOffScreenSurface(const SDL_Surface& prototype);
	virtual ~SDLOffScreenSurface();

private:
	// OutputSurface
	virtual void saveScreenshot(const std::string& filename);

	SDL_Surface* surface;
};

} // namespace openmsx

#endif
