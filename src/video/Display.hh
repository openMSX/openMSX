// $Id$

#ifndef DISPLAY_HH
#define DISPLAY_HH

#include "Layer.hh"
#include "RendererFactory.hh"
#include "EventListener.hh"
#include "SettingListener.hh"
#include "Command.hh"
#include "InfoTopic.hh"
#include "CircularBuffer.hh"
#include "Alarm.hh"
#include "openmsx.hh"
#include <memory>
#include <string>
#include <vector>

namespace openmsx {

class VideoSystem;
class MSXMotherBoard;
class EventDistributor;
class RenderSettings;
class VideoSystemChangeListener;

/** Represents the output window/screen of openMSX.
  * A display contains several layers.
  */
class Display : private EventListener, private SettingListener,
                private LayerListener
{
public:
	Display(MSXMotherBoard& motherboard);
	virtual ~Display();

	void createVideoSystem();
	VideoSystem& getVideoSystem();

	/** Redraw the display.
	  */
	void repaint();
	void repaintDelayed(unsigned long long delta);

	void addLayer(Layer& layer);
	void removeLayer(Layer& layer);

	void attach(VideoSystemChangeListener& listener);
	void detach(VideoSystemChangeListener& listener);

private:
	void resetVideoSystem();

	// EventListener interface
	virtual void signalEvent(const Event& event);

	// SettingListener interface
	virtual void update(const Setting* setting);

	void checkRendererSwitch();

	typedef std::vector<Layer*> Layers;

	/** Find frontmost opaque layer.
	  */
	Layers::iterator baseLayer();

	// LayerListener interface
	virtual void updateZ(Layer& layer, Layer::ZIndex z);

	Layers layers;
	std::auto_ptr<VideoSystem> videoSystem;

	// the current renderer
	RendererFactory::RendererID currentRenderer;

	typedef std::vector<VideoSystemChangeListener*> Listeners;
	Listeners listeners;

	// fps related data
	static const unsigned NUM_FRAME_DURATIONS = 50;
	CircularBuffer<unsigned long long, NUM_FRAME_DURATIONS> frameDurations;
	unsigned long long frameDurationSum;
	unsigned long long prevTimeStamp;

	// Delayed repaint stuff
	class RepaintAlarm : public Alarm {
	public:
		RepaintAlarm(EventDistributor& eventDistributor);
		virtual void alarm();
	private:
		EventDistributor& eventDistributor;
	} alarm;

	// Commands
	class ScreenShotCmd : public SimpleCommand {
	public:
		ScreenShotCmd(CommandController& commandController, Display& display);
		virtual std::string execute(const std::vector<std::string>& tokens);
		virtual std::string help(const std::vector<std::string>& tokens) const;
	private:
		Display& display;
	} screenShotCmd;

	// Info
	class FpsInfoTopic : public InfoTopic {
	public:
		FpsInfoTopic(CommandController& commandController, Display& display);
		virtual void execute(const std::vector<TclObject*>& tokens,
		                     TclObject& result) const;
		virtual std::string help(const std::vector<std::string>& tokens) const;
	private:
		Display& display;
	} fpsInfo;

	MSXMotherBoard& motherboard;
	RenderSettings& renderSettings;
	unsigned switchInProgress;
};

} // namespace openmsx

#endif
