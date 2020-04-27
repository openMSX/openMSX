#ifndef SDLGLVISIBLESURFACE_HH
#define SDLGLVISIBLESURFACE_HH

#include "SDLVisibleSurfaceBase.hh"

namespace openmsx {

/** Visible surface for SDL openGL renderers
 */
class SDLGLVisibleSurface final : public SDLVisibleSurfaceBase
{
public:
	SDLGLVisibleSurface(unsigned width, unsigned height,
	                    Display& display,
	                    RTScheduler& rtScheduler,
	                    EventDistributor& eventDistributor,
	                    InputEventGenerator& inputEventGenerator,
	                    CliComm& cliComm,
	                    VideoSystem& videoSystem);
	~SDLGLVisibleSurface() override;

	static void saveScreenshotGL(const OutputSurface& output,
	                             const std::string& filename);

	// OutputSurface
	void saveScreenshot(const std::string& filename) override;

	// VisibleSurface
	void finish() override;
	std::unique_ptr<Layer> createSnowLayer() override;
	std::unique_ptr<Layer> createConsoleLayer(
		Reactor& reactor, CommandConsole& console) override;
	std::unique_ptr<Layer> createOSDGUILayer(OSDGUI& gui) override;
	std::unique_ptr<OutputSurface> createOffScreenSurface() override;

private:
	struct SyncToVBlankModeObserver : openmsx::Observer<Setting> {
		void update(const Setting& setting) override;
	} syncToVBlankModeObserver;

	SDL_GLContext glContext;
};

} // namespace openmsx

#endif
