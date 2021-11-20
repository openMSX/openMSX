#ifndef SDLOFFSCREENSURFACE_HH
#define SDLOFFSCREENSURFACE_HH

#include "SDLOutputSurface.hh"
#include "SDLSurfacePtr.hh"
#include "MemBuffer.hh"
#include "aligned.hh"

namespace openmsx {

class SDLOffScreenSurface final : public SDLOutputSurface
{
public:
	explicit SDLOffScreenSurface(const SDL_Surface& prototype);

private:
	// OutputSurface
	void saveScreenshot(const std::string& filename) override;
	void clearScreen() override;

private:
	MemBuffer<char, SSE_ALIGNMENT> buffer;
	SDLSurfacePtr surface;
	SDLRendererPtr renderer;
};

} // namespace openmsx

#endif
