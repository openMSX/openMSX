// $Id$

#ifndef DISPLAY_HH
#define DISPLAY_HH

#include "RendererFactory.hh"
#include "EventListener.hh"
#include "LayerListener.hh"
#include "Observer.hh"
#include "CircularBuffer.hh"
#include "noncopyable.hh"
#include <memory>
#include <string>
#include <vector>

namespace openmsx {

class Layer;
class Reactor;
class VideoSystem;
class CliComm;
class CommandConsole;
class RenderSettings;
class VideoSystemChangeListener;
class Setting;
class AlarmEvent;
class ScreenShotCmd;
class FpsInfoTopic;
class OSDGUI;
class OutputSurface;

/** Represents the output window/screen of openMSX.
  * A display contains several layers.
  */
class Display : private EventListener, private Observer<Setting>,
                private LayerListener, private noncopyable
{
public:
	explicit Display(Reactor& reactor);
	virtual ~Display();

	void createVideoSystem();
	VideoSystem& getVideoSystem();

	CliComm& getCliComm() const;
	RenderSettings& getRenderSettings() const;
	OSDGUI& getOSDGUI() const;
	CommandConsole& getCommandConsole();

	/** Redraw the display.
	  */
	void repaint();
	void repaint(OutputSurface& surface);
	void repaintDelayed(unsigned long long delta);

	void addLayer(Layer& layer);
	void removeLayer(Layer& layer);

	void attach(VideoSystemChangeListener& listener);
	void detach(VideoSystemChangeListener& listener);

	Layer* findLayer(const std::string& name) const;

private:
	void resetVideoSystem();
	void setWindowTitle();

	// EventListener interface
	virtual int signalEvent(shared_ptr<const Event> event);

	// Observer<Setting> interface
	virtual void update(const Setting& setting);

	void checkRendererSwitch();
	void doRendererSwitch();
	void doRendererSwitch2();

	typedef std::vector<Layer*> Layers;

	/** Find frontmost opaque layer.
	  */
	Layers::iterator baseLayer();

	// LayerListener interface
	virtual void updateZ(Layer& layer);

	Layers layers;
	std::auto_ptr<VideoSystem> videoSystem;

	typedef std::vector<VideoSystemChangeListener*> Listeners;
	Listeners listeners;

	// fps related data
	static const unsigned NUM_FRAME_DURATIONS = 50;
	CircularBuffer<unsigned long long, NUM_FRAME_DURATIONS> frameDurations;
	unsigned long long frameDurationSum;
	unsigned long long prevTimeStamp;

	friend class FpsInfoTopic;
	const std::auto_ptr<AlarmEvent> alarm; // delayed repaint
	const std::auto_ptr<ScreenShotCmd> screenShotCmd;
	const std::auto_ptr<FpsInfoTopic> fpsInfo;
	const std::auto_ptr<OSDGUI> osdGui;

	Reactor& reactor;
	const std::auto_ptr<RenderSettings> renderSettings;
	const std::auto_ptr<CommandConsole> commandConsole;

	// the current renderer
	RendererFactory::RendererID currentRenderer;

	bool switchInProgress;
};

} // namespace openmsx

#endif
