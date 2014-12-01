#ifndef DISPLAY_HH
#define DISPLAY_HH

#include "RenderSettings.hh"
#include "EventListener.hh"
#include "LayerListener.hh"
#include "RTSchedulable.hh"
#include "Observer.hh"
#include "CircularBuffer.hh"
#include <memory>
#include <vector>
#include <cstdint>

namespace openmsx {

class Layer;
class Reactor;
class VideoSystem;
class CliComm;
class CommandConsole;
class RenderSettings;
class VideoSystemChangeListener;
class Setting;
class ScreenShotCmd;
class FpsInfoTopic;
class OSDGUI;
class OutputSurface;

/** Represents the output window/screen of openMSX.
  * A display contains several layers.
  */
class Display final : public EventListener, private Observer<Setting>
                    , private LayerListener, private RTSchedulable
{
public:
	typedef std::vector<Layer*> Layers;

	explicit Display(Reactor& reactor);
	~Display();

	void createVideoSystem();
	VideoSystem& getVideoSystem();

	CliComm& getCliComm() const;
	RenderSettings& getRenderSettings() const { return *renderSettings; }
	OSDGUI& getOSDGUI() const { return *osdGui; }
	CommandConsole& getCommandConsole() { return *commandConsole; }

	/** Redraw the display.
	  */
	void repaint();
	void repaint(OutputSurface& surface);
	void repaintDelayed(uint64_t delta);

	void addLayer(Layer& layer);
	void removeLayer(Layer& layer);

	void attach(VideoSystemChangeListener& listener);
	void detach(VideoSystemChangeListener& listener);

	Layer* findActiveLayer() const;
	const Layers& getAllLayers() const { return layers; }

private:
	void resetVideoSystem();
	void setWindowTitle();

	// EventListener interface
	int signalEvent(const std::shared_ptr<const Event>& event) override;

	// RTSchedulable
	void executeRT() override;

	// Observer<Setting> interface
	void update(const Setting& setting) override;

	void checkRendererSwitch();
	void doRendererSwitch();
	void doRendererSwitch2();

	/** Find frontmost opaque layer.
	  */
	Layers::iterator baseLayer();

	// LayerListener interface
	void updateZ(Layer& layer) override;

	Layers layers;
	std::unique_ptr<VideoSystem> videoSystem;

	std::vector<VideoSystemChangeListener*> listeners;

	// fps related data
	static const unsigned NUM_FRAME_DURATIONS = 50;
	CircularBuffer<uint64_t, NUM_FRAME_DURATIONS> frameDurations;
	uint64_t frameDurationSum;
	uint64_t prevTimeStamp;

	friend class FpsInfoTopic;
	const std::unique_ptr<ScreenShotCmd> screenShotCmd;
	const std::unique_ptr<FpsInfoTopic> fpsInfo;
	const std::unique_ptr<OSDGUI> osdGui;

	Reactor& reactor;
	const std::unique_ptr<RenderSettings> renderSettings;
	const std::unique_ptr<CommandConsole> commandConsole;

	// the current renderer
	RenderSettings::RendererID currentRenderer;

	bool renderFrozen;
	bool switchInProgress;
};

} // namespace openmsx

#endif
