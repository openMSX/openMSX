// $Id$

#ifndef SDLGLVISIBLESURFACE_HH
#define SDLGLVISIBLESURFACE_HH

#include "VisibleSurface.hh"
#include "GLUtil.hh"

namespace openmsx {

class SDLGLVisibleSurface : public VisibleSurface
{
public:
	enum FrameBuffer { FB_NONE, FB_16BPP, FB_32BPP };

	SDLGLVisibleSurface(unsigned width, unsigned height, bool fullscreen,
	                   FrameBuffer frameBuffer = FB_NONE);
	virtual ~SDLGLVisibleSurface();

	virtual bool init();
	virtual void drawFrameBuffer();
	virtual void finish();

	virtual void takeScreenShot(const std::string& filename);

	virtual std::auto_ptr<Layer> createSnowLayer();
	virtual std::auto_ptr<Layer> createConsoleLayer(
		MSXMotherBoard& motherboard);
	virtual std::auto_ptr<Layer> createIconLayer(
		CommandController& commandController,
		Display& display, IconStatus& iconStatus);

private:
	FrameBuffer frameBuffer;
	GLuint textureId;
	double texCoordX, texCoordY;
};

} // namespace openmsx

#endif
