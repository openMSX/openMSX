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
