// $Id$

#ifndef SDLVISIBLESURFACE_HH
#define SDLVISIBLESURFACE_HH

#include "VisibleSurface.hh"

namespace openmsx {

class SDLVisibleSurface : public VisibleSurface
{
public:
	SDLVisibleSurface(unsigned width, unsigned height, bool fullscreen,
			RenderSettings& renderSettings, EventDistributor&
			eventDistributor);

	virtual void drawFrameBuffer();
	virtual void finish();

	virtual void takeScreenShot(const std::string& filename);

	virtual std::auto_ptr<Layer> createSnowLayer(Display& display);
	virtual std::auto_ptr<Layer> createConsoleLayer(Reactor& reactor);
	virtual std::auto_ptr<Layer> createOSDGUILayer(OSDGUI& gui);
};

} // namespace openmsx

#endif
