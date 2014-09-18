#ifndef SDLOFFSCREENSURFACE_HH
#define SDLOFFSCREENSURFACE_HH

#include "OutputSurface.hh"
#include "SDLSurfacePtr.hh"

namespace openmsx {

class SDLOffScreenSurface final : public OutputSurface
{
public:
	explicit SDLOffScreenSurface(const SDL_Surface& prototype);
	~SDLOffScreenSurface();

private:
	// OutputSurface
	virtual void saveScreenshot(const std::string& filename);
	virtual void clearScreen();

	SDLSurfacePtr surface;
	void* buffer;
};

} // namespace openmsx

#endif
