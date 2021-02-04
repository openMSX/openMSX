#ifndef VISIBLESURFACE_HH
#define VISIBLESURFACE_HH

#include "Observer.hh"
#include "EventListener.hh"
#include "RTSchedulable.hh"
#include <memory>

namespace openmsx {

class Layer;
class OutputSurface;
class Reactor;
class CommandConsole;
class EventDistributor;
class InputEventGenerator;
class Setting;
class Display;
class OSDGUI;
class CliComm;
class VideoSystem;

/** An OutputSurface which is visible to the user, such as a window or a
  * full screen display.
  */
class VisibleSurface : public EventListener, private Observer<Setting>
                     , private RTSchedulable
{
public:
	virtual ~VisibleSurface();
	virtual void updateWindowTitle() = 0;
	virtual bool setFullScreen(bool fullscreen) = 0;

	/** When a complete frame is finished, call this method.
	  * It will 'actually' display it. E.g. when using double buffering
	  * it will swap the front and back buffer.
	  */
	virtual void finish() = 0;

	[[nodiscard]] virtual std::unique_ptr<Layer> createSnowLayer() = 0;
	[[nodiscard]] virtual std::unique_ptr<Layer> createConsoleLayer(
		Reactor& reactor, CommandConsole& console) = 0;
	[[nodiscard]] virtual std::unique_ptr<Layer> createOSDGUILayer(OSDGUI& gui) = 0;

	/** Create an off-screen OutputSurface which has similar properties
	  * as this VisibleSurface. E.g. used to re-render the current frame
	  * without OSD elements to take a screenshot.
	  */
	[[nodiscard]] virtual std::unique_ptr<OutputSurface> createOffScreenSurface() = 0;

	[[nodiscard]] CliComm& getCliComm() const { return cliComm; }
	[[nodiscard]] Display& getDisplay() const { return display; }

protected:
	VisibleSurface(Display& display,
	               RTScheduler& rtScheduler,
	               EventDistributor& eventDistributor,
	               InputEventGenerator& inputEventGenerator,
	               CliComm& cliComm,
	               VideoSystem& videoSystem);

private:
	void updateCursor();

	// Observer
	void update(const Setting& setting) noexcept override;
	// EventListener
	int signalEvent(const std::shared_ptr<const Event>& event) noexcept override;
	// RTSchedulable
	void executeRT() override;

private:
	Display& display;
	EventDistributor& eventDistributor;
	InputEventGenerator& inputEventGenerator;
	CliComm& cliComm;
	VideoSystem& videoSystem;
	bool grab = false;
};

} // namespace openmsx

#endif
