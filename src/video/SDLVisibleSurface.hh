#ifndef SDLVISIBLESURFACE_HH
#define SDLVISIBLESURFACE_HH

#include "VisibleSurface.hh"

namespace openmsx {

class SDLVisibleSurface final : public VisibleSurface
{
public:
	SDLVisibleSurface(unsigned width, unsigned height,
	                  RenderSettings& renderSettings,
	                  RTScheduler& rtScheduler,
	                  EventDistributor& eventDistributor,
	                  InputEventGenerator& inputEventGenerator,
	                  CliComm& cliComm);

private:
	// OutputSurface
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
