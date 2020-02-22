#ifndef SDLVISIBLESURFACEBASE_HH
#define SDLVISIBLESURFACEBASE_HH

#include "SDLOutputSurface.hh"
#include "VisibleSurface.hh"
#include "SDLSurfacePtr.hh"

namespace openmsx {

/** Common functionality for the plain SDL and SDLGL VisibleSurface classes.
  */
class SDLVisibleSurfaceBase : public SDLOutputSurface, public VisibleSurface
{
public:
	void updateWindowTitle() override;
	bool setFullScreen(bool fullscreen) override;

protected:
	using VisibleSurface::VisibleSurface;
	void createSurface(int width, int height, unsigned flags);

	SDLSubSystemInitializer<SDL_INIT_VIDEO> videoSubSystem;
	SDLWindowPtr window;
};

} // namespace openmsx

#endif
