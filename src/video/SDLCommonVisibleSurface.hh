#ifndef SDLCOMMONVISIBLESURFACE_HH
#define SDLCOMMONVISIBLESURFACE_HH

#include "OutputSurface.hh"
#include "VisibleSurface.hh"
#include "SDLSurfacePtr.hh"

namespace openmsx {

/** Common functionality for the plain SDL and SDLGL VisibleSurface classes.
  */
class SDLCommonVisibleSurface : public OutputSurface, public VisibleSurface
{
public:
	void updateWindowTitle() override;
	bool setFullScreen(bool fullscreen) override;

protected:
	using VisibleSurface::VisibleSurface;
	void createSurface(int width, int height, unsigned flags);

	SDLSubSystemInitializer<SDL_INIT_VIDEO> videoSubSystem;
	SDLWindowPtr window;
	SDLRendererPtr renderer;
	SDLSurfacePtr surface;
	SDLTexturePtr texture;
};

} // namespace openmsx

#endif
