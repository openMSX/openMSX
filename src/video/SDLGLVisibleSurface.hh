#ifndef SDLGLVISIBLESURFACE_HH
#define SDLGLVISIBLESURFACE_HH

#include "VisibleSurface.hh"

namespace openmsx {

/** Visible surface for SDL openGL renderers
 */
class SDLGLVisibleSurface final : public VisibleSurface
{
public:
	SDLGLVisibleSurface(unsigned width, unsigned height,
	                    Display& display,
	                    RTScheduler& rtScheduler,
	                    EventDistributor& eventDistributor,
	                    InputEventGenerator& inputEventGenerator,
	                    CliComm& cliComm);
	~SDLGLVisibleSurface() override;

	static void saveScreenshotGL(const OutputSurface& output,
	                             const std::string& filename);

private:
	// OutputSurface
	void saveScreenshot(const std::string& filename) override;

	// VisibleSurface
	void finish() override;
	std::unique_ptr<Layer> createSnowLayer() override;
	std::unique_ptr<Layer> createConsoleLayer(
		Reactor& reactor, CommandConsole& console) override;
	std::unique_ptr<Layer> createOSDGUILayer(OSDGUI& gui) override;
	std::unique_ptr<OutputSurface> createOffScreenSurface() override;

	SDL_GLContext glContext;
};

} // namespace openmsx

#endif
