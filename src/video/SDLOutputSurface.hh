// $Id$

#ifndef SDLOUTPUTSURFACE_HH
#define SDLOUTPUTSURFACE_HH

#include "OutputSurface.hh"

namespace openmsx {

class SDLOutputSurface : public OutputSurface
{
public:
	SDLOutputSurface(unsigned width, unsigned height, bool fullscreen);

	virtual bool init();
	virtual void drawFrameBuffer();
	virtual void finish();

	virtual void takeScreenShot(const std::string& filename);

	virtual std::auto_ptr<Layer> createSnowLayer();
	virtual std::auto_ptr<Layer> createConsoleLayer(
		MSXMotherBoard& motherboard);
	virtual std::auto_ptr<Layer> createIconLayer(
		CommandController& commandController,
		EventDistributor& eventDistributor, Display& display);
};

} // namespace openmsx

#endif
