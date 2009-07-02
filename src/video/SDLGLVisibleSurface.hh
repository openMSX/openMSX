// $Id$

#ifndef SDLGLVISIBLESURFACE_HH
#define SDLGLVISIBLESURFACE_HH

#include "VisibleSurface.hh"
#include "SDLGLOutputSurface.hh"

namespace openmsx {

/** Visible surface for openGL renderers, both SDLGL-PP and SDLGL-FBxx
 */
class SDLGLVisibleSurface : public VisibleSurface, public SDLGLOutputSurface
{
public:
	SDLGLVisibleSurface(unsigned width, unsigned height, bool fullscreen,
	                    RenderSettings& renderSettings,
	                    EventDistributor& eventDistributor,
	                    InputEventGenerator& inputEventGenerator,
	                    FrameBuffer frameBuffer = FB_NONE);
	virtual ~SDLGLVisibleSurface();

private:
	// OutputSurface
	virtual void flushFrameBuffer();
	virtual void saveScreenshot(const std::string& filename);

	// VisibleSurface
	virtual void finish();
	virtual std::auto_ptr<Layer> createSnowLayer(Display& display);
	virtual std::auto_ptr<Layer> createConsoleLayer(
		Reactor& reactor, CommandConsole& console);
	virtual std::auto_ptr<Layer> createOSDGUILayer(OSDGUI& gui);
	virtual std::auto_ptr<OutputSurface> createOffScreenSurface();
};

} // namespace openmsx

#endif
