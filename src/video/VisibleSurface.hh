#ifndef VISIBLESURFACE_HH
#define VISIBLESURFACE_HH

#include "OutputSurface.hh"
#include "Observer.hh"
#include "EventListener.hh"
#include "RTSchedulable.hh"
#include "SDLSurfacePtr.hh"
#include <memory>

namespace openmsx {

class Layer;
class Reactor;
class CommandConsole;
class EventDistributor;
class InputEventGenerator;
class Setting;
class Display;
class OSDGUI;
class CliComm;

/** An OutputSurface which is visible to the user, such as a window or a
  * full screen display.
  * This class provides a frame buffer based renderer a common interface,
  * no matter whether the back-end is plain SDL or SDL+OpenGL.
  */
class VisibleSurface : public OutputSurface, public EventListener,
                       private Observer<Setting>, private RTSchedulable
{
public:
	~VisibleSurface() override;
	void updateWindowTitle();
	bool setFullScreen(bool fullscreen);

	/** When a complete frame is finished, call this method.
	  * It will 'actually' display it. E.g. when using double buffering
	  * it will swap the front and back buffer.
	  */
	virtual void finish() = 0;

	virtual std::unique_ptr<Layer> createSnowLayer() = 0;
	virtual std::unique_ptr<Layer> createConsoleLayer(
		Reactor& reactor, CommandConsole& console) = 0;
	virtual std::unique_ptr<Layer> createOSDGUILayer(OSDGUI& gui) = 0;

	/** Create an off-screen OutputSurface which has similar properties
	  * as this VisibleSurface. E.g. used to re-render the current frame
	  * without OSD elements to take a screenshot.
	  */
	virtual std::unique_ptr<OutputSurface> createOffScreenSurface() = 0;

	Display& getDisplay() const { return display; }

protected:
	VisibleSurface(Display& display,
	               RTScheduler& rtScheduler,
	               EventDistributor& eventDistributor,
	               InputEventGenerator& inputEventGenerator,
	               CliComm& cliComm);
	void createSurface(int width, int height, unsigned flags);

	SDLSubSystemInitializer<SDL_INIT_VIDEO> videoSubSystem;
	SDLWindowPtr window;
	SDLRendererPtr renderer;
	SDLSurfacePtr surface;
	SDLTexturePtr texture;

private:
	void updateCursor();

	// Observer
	void update(const Setting& setting) override;
	// EventListener
	int signalEvent(const std::shared_ptr<const Event>& event) override;
	// RTSchedulable
	void executeRT() override;

	Display& display;
	EventDistributor& eventDistributor;
	InputEventGenerator& inputEventGenerator;
	CliComm& cliComm;
};

} // namespace openmsx

#endif
