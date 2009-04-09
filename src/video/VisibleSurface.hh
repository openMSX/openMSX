// $Id$

#ifndef VISIBLESURFACE_HH
#define VISIBLESURFACE_HH

#include "OutputSurface.hh"
#include "Observer.hh"
#include "EventListener.hh"
#include "Alarm.hh"
#include <string>
#include <memory>

namespace openmsx {

class Layer;
class Reactor;
class CommandController;
class EventDistributor;
class RenderSettings;
class Setting;
class Display;
class OSDGUI;

/** An OutputSurface which is visible to the user, such as a window or a
  * full screen display.
  * This class provides a frame buffer based renderer a common interface,
  * no matter whether the back-end is plain SDL or SDL+OpenGL.
  */
class VisibleSurface : public OutputSurface, public EventListener,
                       private Observer<Setting>, private Alarm
{
public:
	virtual ~VisibleSurface();
	void setWindowTitle(const std::string& title);
	bool setFullScreen(bool fullscreen);

	/** When a complete frame is finished, call this method.
	  * It will 'actually' display it. E.g. when using double buffering
	  * it will swap the front and back buffer.
	  */
	virtual void finish() = 0;

	virtual std::auto_ptr<Layer> createSnowLayer(Display& display) = 0;
	virtual std::auto_ptr<Layer> createConsoleLayer(
		Reactor& reactor) = 0;
	virtual std::auto_ptr<Layer> createOSDGUILayer(OSDGUI& gui) = 0;

	/** Create an off-screen OutputSurface which has similar properties
	  * as this VisibleSurface. E.g. used to re-render the current frame
	  * without OSD elements to take a screenshot.
	  */
	virtual std::auto_ptr<OutputSurface> createOffScreenSurface() = 0;

protected:
	VisibleSurface(RenderSettings& renderSettings,
	               EventDistributor& eventDistributor);
	void createSurface(unsigned width, unsigned height, int flags);

private:
	void updateCursor();

	// Observer
	virtual void update(const Setting& setting);
	// EventListener
	virtual bool signalEvent(shared_ptr<const Event> event);
	// Alarm
	virtual bool alarm();

	RenderSettings& renderSettings;
	EventDistributor& eventDistributor;
};

} // namespace openmsx

#endif
