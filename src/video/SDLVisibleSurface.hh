#ifndef SDLVISIBLESURFACE_HH
#define SDLVISIBLESURFACE_HH

#include "SDLVisibleSurfaceBase.hh"

namespace openmsx {

class SDLVisibleSurface final : public SDLVisibleSurfaceBase
{
public:
	SDLVisibleSurface(unsigned width, unsigned height,
	                  Display& display,
	                  RTScheduler& rtScheduler,
	                  EventDistributor& eventDistributor,
	                  InputEventGenerator& inputEventGenerator,
	                  CliComm& cliComm,
	                  VideoSystem& videoSystem);

	static void saveScreenshotSDL(const SDLOutputSurface& output,
	                              const std::string& filename);

	// OutputSurface
	void saveScreenshot(const std::string& filename) override;
	void flushFrameBuffer() override;
	void clearScreen() override;

	// VisibleSurface
	void finish() override;
	[[nodiscard]] std::unique_ptr<Layer> createSnowLayer() override;
	[[nodiscard]] std::unique_ptr<Layer> createConsoleLayer(
		Reactor& reactor, CommandConsole& console) override;
	[[nodiscard]] std::unique_ptr<Layer> createOSDGUILayer(OSDGUI& gui) override;
	[[nodiscard]] std::unique_ptr<OutputSurface> createOffScreenSurface() override;
	void fullScreenUpdated(bool fullscreen) override;

private:
	SDLRendererPtr renderer;
	SDLSurfacePtr surface;
	SDLTexturePtr texture;
};

} // namespace openmsx

#endif
