#ifndef SDLVISIBLESURFACEBASE_HH
#define SDLVISIBLESURFACEBASE_HH

#include "OutputSurface.hh"
#include "VisibleSurface.hh"
#include "SDLSurfacePtr.hh"

namespace openmsx {

/** Common functionality for the plain SDL and SDLGL VisibleSurface classes.
  */
class SDLVisibleSurfaceBase : public OutputSurface, public VisibleSurface
{
public:
	void updateWindowTitle() override;
	bool setFullScreen(bool fullscreen) override;
	~SDLVisibleSurfaceBase() override;

protected:
	using VisibleSurface::VisibleSurface;
	void createSurface(int width, int height, unsigned flags);
	virtual void fullScreenUpdated(bool fullscreen) = 0;

protected:
	SDLSubSystemInitializer<SDL_INIT_VIDEO> videoSubSystem;
	SDLWindowPtr window;

private:
	static int windowPosX;
	static int windowPosY;
};

} // namespace openmsx

#endif
