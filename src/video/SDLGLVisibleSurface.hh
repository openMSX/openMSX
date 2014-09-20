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
	~SDLGLVisibleSurface();

private:
	// OutputSurface
	void flushFrameBuffer() override;
	void saveScreenshot(const std::string& filename) override;
	void clearScreen() override;

	// VisibleSurface
	void finish() override;
	std::unique_ptr<Layer> createSnowLayer(Display& display) override;
	std::unique_ptr<Layer> createConsoleLayer(
		Reactor& reactor, CommandConsole& console) override;
	std::unique_ptr<Layer> createOSDGUILayer(OSDGUI& gui) override;
	std::unique_ptr<OutputSurface> createOffScreenSurface() override;
};

} // namespace openmsx

#endif
