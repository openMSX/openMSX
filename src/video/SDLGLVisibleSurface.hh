// $Id$

#ifndef SDLGLVISIBLESURFACE_HH
#define SDLGLVISIBLESURFACE_HH

#include "VisibleSurface.hh"
#include "GLUtil.hh"
#include <memory>

namespace openmsx {

class SDLGLVisibleSurface : public VisibleSurface
{
public:
	enum FrameBuffer { FB_NONE, FB_16BPP, FB_32BPP };

	SDLGLVisibleSurface(unsigned width, unsigned height, bool fullscreen,
	                   FrameBuffer frameBuffer = FB_NONE);
	virtual ~SDLGLVisibleSurface();

	virtual bool init();
	virtual unsigned mapRGB(double dr, double dg, double db);
	virtual void drawFrameBuffer();
	virtual void finish();

	virtual void takeScreenShot(const std::string& filename);

	virtual std::auto_ptr<Layer> createSnowLayer();
	virtual std::auto_ptr<Layer> createConsoleLayer(
		Reactor& reactor);
	virtual std::auto_ptr<Layer> createIconLayer(
		CommandController& commandController,
		Display& display, IconStatus& iconStatus);

private:
	FrameBuffer frameBuffer;
	// Note: This must be a pointer because the texture should not be allocated
	//       before the createSurface call.
	std::auto_ptr<ColourTexture> texture;
	double texCoordX, texCoordY;
};

} // namespace openmsx

#endif
