// $Id$

#ifndef DISPLAY_HH
#define DISPLAY_HH

#include "Layer.hh"
#include "EventListener.hh"
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

/** Represents the output window/screen of openMSX.
  * A display contains several layers.
  */
class Display: private EventListener, private LayerListener
{
public:
	// TODO: Keep as singleton?
	static std::auto_ptr<Display> INSTANCE;

	Display(std::auto_ptr<VideoSystem> videoSystem);
	virtual ~Display();

	virtual bool signalEvent(const Event& event);

	const std::auto_ptr<VideoSystem>& getVideoSystem() {
		return videoSystem;
	}

	/** Redraw the display.
	  */
	void repaint();
	void repaintDelayed(unsigned long long delta);

	void addLayer(Layer* layer);

private:
	typedef std::vector<Layer*> Layers;
	
	/** Find frontmost opaque layer.
	  */
	Layers::iterator baseLayer();

	// LayerListener interface
	virtual void updateCoverage(Layer* layer, Layer::Coverage coverage);
	virtual void updateZ(Layer* layer, Layer::ZIndex z);

	Layers layers;
	std::auto_ptr<VideoSystem> videoSystem;

	// fps related data
	static const unsigned NUM_FRAME_DURATIONS = 50;
	CircularBuffer<unsigned long long, NUM_FRAME_DURATIONS> frameDurations;
	unsigned long long frameDurationSum;
	unsigned long long prevTimeStamp;

	// Delayed repaint stuff
	class RepaintAlarm : public Alarm {
	private:
		virtual void alarm();
	} alarm;

	// Commands
	class ScreenShotCmd : public SimpleCommand {
	public:
		ScreenShotCmd(Display& display);
		virtual std::string execute(const std::vector<std::string>& tokens);
		virtual std::string help(const std::vector<std::string>& tokens) const;
	private:
		Display& display;
	} screenShotCmd;

	// Info 
	class FpsInfoTopic : public InfoTopic {
	public:
		FpsInfoTopic(Display& parent);
		virtual void execute(const std::vector<CommandArgument>& tokens,
		                     CommandArgument& result) const;
		virtual std::string help(const std::vector<std::string>& tokens) const;
	private:
		Display& parent;
	} fpsInfo;
};

} // namespace openmsx

#endif // DISPLAY_HH

