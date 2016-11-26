#ifndef SDLVISIBLESURFACE_HH
#define SDLVISIBLESURFACE_HH

#include "VisibleSurface.hh"

namespace openmsx {

class SDLVisibleSurface final : public VisibleSurface
{
public:
	SDLVisibleSurface(unsigned width, unsigned height,
	                  Display& display,
	                  RTScheduler& rtScheduler,
	                  EventDistributor& eventDistributor,
	                  InputEventGenerator& inputEventGenerator,
	                  CliComm& cliComm);

	static void saveScreenshotSDL(OutputSurface& output,
	                              const std::string& filename);

private:
	// OutputSurface
	void saveScreenshot(const std::string& filename) override;
	void clearScreen() override;

	// VisibleSurface
	void flushFrameBuffer() override;
	void finish() override;
	std::unique_ptr<Layer> createSnowLayer() override;
	std::unique_ptr<Layer> createConsoleLayer(
		Reactor& reactor, CommandConsole& console) override;
	std::unique_ptr<Layer> createOSDGUILayer(OSDGUI& gui) override;
	std::unique_ptr<OutputSurface> createOffScreenSurface() override;
};

} // namespace openmsx

#endif
