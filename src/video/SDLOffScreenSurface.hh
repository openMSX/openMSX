// $Id$

#ifndef SDLOFFSCREENSURFACE_HH
#define SDLOFFSCREENSURFACE_HH

#include "OutputSurface.hh"
#include "SDLSurfacePtr.hh"

namespace openmsx {

class SDLOffScreenSurface : public OutputSurface
{
public:
	explicit SDLOffScreenSurface(const SDL_Surface& prototype);

private:
	// OutputSurface
	virtual void saveScreenshot(const std::string& filename);

	SDLSurfacePtr surface;
};

} // namespace openmsx

#endif
