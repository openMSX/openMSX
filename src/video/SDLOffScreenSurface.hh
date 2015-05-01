#ifndef SDLOFFSCREENSURFACE_HH
#define SDLOFFSCREENSURFACE_HH

#include "OutputSurface.hh"
#include "SDLSurfacePtr.hh"
#include "MemBuffer.hh"

namespace openmsx {

class SDLOffScreenSurface final : public OutputSurface
{
public:
	explicit SDLOffScreenSurface(const SDL_Surface& prototype);

private:
	// OutputSurface
	void saveScreenshot(const std::string& filename) override;
	void clearScreen() override;

	SDLSurfacePtr surface;
	MemBuffer<char, SSE2_ALIGNMENT> buffer;
};

} // namespace openmsx

#endif
