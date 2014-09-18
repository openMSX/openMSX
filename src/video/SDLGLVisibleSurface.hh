#ifndef SDLGLVISIBLESURFACE_HH
#define SDLGLVISIBLESURFACE_HH

#include "VisibleSurface.hh"
#include "SDLGLOutputSurface.hh"

namespace openmsx {

/** Visible surface for openGL renderers, both SDLGL-PP and SDLGL-FBxx
 */
class SDLGLVisibleSurface final : public VisibleSurface
                                , public SDLGLOutputSurface
{
public:
	SDLGLVisibleSurface(unsigned width, unsigned height,
	                    RenderSettings& renderSettings,
	                    RTScheduler& rtScheduler,
	                    EventDistributor& eventDistributor,
	                    InputEventGenerator& inputEventGenerator,
	                    CliComm& cliComm,
	                    FrameBuffer frameBuffer = FB_NONE);
	virtual ~SDLGLVisibleSurface();

private:
	// OutputSurface
	virtual void flushFrameBuffer();
	virtual void saveScreenshot(const std::string& filename);
	virtual void clearScreen();

	// VisibleSurface
	virtual void finish();
	virtual std::unique_ptr<Layer> createSnowLayer(Display& display);
	virtual std::unique_ptr<Layer> createConsoleLayer(
		Reactor& reactor, CommandConsole& console);
	virtual std::unique_ptr<Layer> createOSDGUILayer(OSDGUI& gui);
	virtual std::unique_ptr<OutputSurface> createOffScreenSurface();
};

} // namespace openmsx

#endif
