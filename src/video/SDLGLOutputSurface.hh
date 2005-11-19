// $Id$

#ifndef SDLGLOUTPUTSURFACE_HH
#define SDLGLOUTPUTSURFACE_HH

#include "OutputSurface.hh"
#include "GLUtil.hh"

namespace openmsx {

class SDLGLOutputSurface : public OutputSurface
{
public:
	enum FrameBuffer { FB_NONE, FB_16BPP, FB_32BPP };

	SDLGLOutputSurface(unsigned width, unsigned height, bool fullscreen,
	                   FrameBuffer frameBuffer = FB_NONE);
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
		Display& display, IconStatus& iconStatus);

private:
	FrameBuffer frameBuffer;
	GLuint textureId;
	double texCoordX, texCoordY;
};

} // namespace openmsx

#endif
