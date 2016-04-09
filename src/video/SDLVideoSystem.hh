#ifndef SDLVIDEOSYSTEM_HH
#define SDLVIDEOSYSTEM_HH

#include "VideoSystem.hh"
#include "EventListener.hh"
#include "Observer.hh"
#include "components.hh"
#include <memory>

namespace openmsx {

class Reactor;
class CommandConsole;
class Display;
class RenderSettings;
class VisibleSurface;
class Layer;
class Setting;

class SDLVideoSystem final : public VideoSystem, private EventListener
                           , private Observer<Setting>
{
public:
	/** Activates this video system.
	  * @throw InitException If initialisation fails.
	  */
	explicit SDLVideoSystem(Reactor& reactor, CommandConsole& console);

	/** Deactivates this video system.
	  */
	~SDLVideoSystem();

	// VideoSystem interface:
	std::unique_ptr<Rasterizer> createRasterizer(VDP& vdp) override;
	std::unique_ptr<V9990Rasterizer> createV9990Rasterizer(
		V9990& vdp) override;
#if COMPONENT_LASERDISC
	std::unique_ptr<LDRasterizer> createLDRasterizer(
		LaserdiscPlayer& ld) override;
#endif
	bool checkSettings() override;
	void flush() override;
	void takeScreenShot(const std::string& filename, bool withOsd) override;
	void setWindowTitle(const std::string& title) override;
	OutputSurface* getOutputSurface() override;

private:
	// EventListener
	int signalEvent(const std::shared_ptr<const Event>& event) override;
	// Observer
	void update(const Setting& subject) override;

	void getWindowSize(unsigned& width, unsigned& height);
	void resize();

	Reactor& reactor;
	Display& display;
	RenderSettings& renderSettings;
	std::unique_ptr<VisibleSurface> screen;
	std::unique_ptr<Layer> consoleLayer;
	std::unique_ptr<Layer> snowLayer;
	std::unique_ptr<Layer> iconLayer;
	std::unique_ptr<Layer> osdGuiLayer;
};

} // namespace openmsx

#endif
