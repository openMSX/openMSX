// $Id$

#ifndef DISPLAY_HH
#define DISPLAY_HH

#include "Layer.hh"
#include "RendererFactory.hh"
#include "EventListener.hh"
#include "Observer.hh"
#include "CircularBuffer.hh"
#include <memory>
#include <vector>

namespace openmsx {

class Reactor;
class VideoSystem;
class RenderSettings;
class VideoSystemChangeListener;
class Setting;
class RepaintAlarm;
class ScreenShotCmd;
class FpsInfoTopic;

/** Represents the output window/screen of openMSX.
  * A display contains several layers.
  */
class Display : private EventListener, private Observer<Setting>,
                private LayerListener
{
public:
	explicit Display(Reactor& reactor);
	virtual ~Display();

	void createVideoSystem();
	VideoSystem& getVideoSystem();

	RenderSettings& getRenderSettings();

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
	void setWindowTitle();

	// EventListener interface
	virtual void signalEvent(const Event& event);

	// Observer<Setting> interface
	virtual void update(const Setting& setting);

	void checkRendererSwitch();
	void doRendererSwitch();

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

	friend class FpsInfoTopic;
	const std::auto_ptr<RepaintAlarm> alarm; // delayed repaint
	const std::auto_ptr<ScreenShotCmd> screenShotCmd;
	const std::auto_ptr<FpsInfoTopic> fpsInfo;

	Reactor& reactor;
	std::auto_ptr<RenderSettings> renderSettings;
	unsigned switchInProgress;
};

} // namespace openmsx

#endif
