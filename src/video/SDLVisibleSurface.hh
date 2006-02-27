// $Id$

#ifndef SDLVISIBLESURFACE_HH
#define SDLVISIBLESURFACE_HH

#include "VisibleSurface.hh"

namespace openmsx {

class SDLVisibleSurface : public VisibleSurface
{
public:
	SDLVisibleSurface(unsigned width, unsigned height, bool fullscreen);

	virtual bool init();
	virtual void drawFrameBuffer();
	virtual void finish();

	virtual void takeScreenShot(const std::string& filename);

	virtual std::auto_ptr<Layer> createSnowLayer();
	virtual std::auto_ptr<Layer> createConsoleLayer(Reactor& reactor);
	virtual std::auto_ptr<Layer> createIconLayer(
		CommandController& commandController,
		Display& display, IconStatus& iconStatus);
};

} // namespace openmsx

#endif
