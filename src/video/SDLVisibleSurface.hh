// $Id$

#ifndef SDLVISIBLESURFACE_HH
#define SDLVISIBLESURFACE_HH

#include "VisibleSurface.hh"

namespace openmsx {

class SDLVisibleSurface : public VisibleSurface
{
public:
	SDLVisibleSurface(unsigned width, unsigned height, bool fullscreen,
	                  RenderSettings& renderSettings,
	                  EventDistributor& eventDistributor);

private:
	// OutputSurface
	virtual void saveScreenshot(const std::string& filename);

	// VisibleSurface
	virtual void finish();
	virtual std::auto_ptr<Layer> createSnowLayer(Display& display);
	virtual std::auto_ptr<Layer> createConsoleLayer(Reactor& reactor);
	virtual std::auto_ptr<Layer> createOSDGUILayer(OSDGUI& gui);
	virtual std::auto_ptr<OutputSurface> createOffScreenSurface();
};

} // namespace openmsx

#endif
