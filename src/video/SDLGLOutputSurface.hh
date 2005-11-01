// $Id$

#ifndef SDLGLOUTPUTSURFACE_HH
#define SDLGLOUTPUTSURFACE_HH

#include "OutputSurface.hh"
#include "GLUtil.hh"

namespace openmsx {

class SDLGLOutputSurface : public OutputSurface
{
public:
	SDLGLOutputSurface(unsigned width, unsigned height, bool fullscreen);
	virtual ~SDLGLOutputSurface();

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

private:
	GLuint textureId;
	double texCoordX, texCoordY;
};

} // namespace openmsx

#endif
